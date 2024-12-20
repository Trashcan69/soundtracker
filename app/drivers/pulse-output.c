
/*
 * The Real SoundTracker - PulseAudio output driver
 * 
 * Copyright (C) 2021 Yury Aliaev
 *
 * Some PulseAudio tricks were spied from Shairport Sync program by
 * Mike Brady
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>

#if DRIVER_PULSE

#include <pulse/pulseaudio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <time.h>

#include "driver.h"
#include "errors.h"
#include "gui-subs.h"
#include "mixer.h"
#include "preferences.h"
#include "st-subs.h"

typedef enum {
    PULSE_STATE_NOTREADY = 0,
    PULSE_STATE_READY,
    PULSE_STATE_RUNNING,
    PULSE_STATE_ERROR
} PulseState;

typedef struct {
    GtkWidget *configwidget, *freqlabel, *freq_combo;

    pa_threaded_mainloop* mainloop;
    pa_context* context;
    pa_stream* stream;

    gint rate, native_rate;
    STMixerFormat format;
    PulseState state;
    gdouble starttime;

    void* sndbuf;
    size_t len;
    gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat);
} pulse_driver;

/* Buffer size is hardcoded. Seems for PulseAudio it's a normal practice */
#define BSIZE (uint32_t)(2 * 2 * 65536) >> 4

static const guint mixfreqs[] = { 44100, 48000, 96000 };
#define NUM_FREQS ARRAY_SIZE(mixfreqs)

static void
pulse_error(const gchar* msg, pulse_driver* d)
{
    gchar *buf, *converted;

    converted = g_locale_to_utf8(pa_strerror(pa_context_errno(d->context)),
        -1, NULL, NULL, NULL);

    buf = g_strdup_printf(_("PulseAudio driver error: %s\n"
        "Error message is: %s\n"), _(msg), converted);
    error_error(buf);
    g_free(converted);
    g_free(buf);
}

static void
update_controls(pulse_driver* d)
{
    gchar* buf;
    gint selected = -1, i;

    buf = g_strdup_printf("%u", d->rate);
    gtk_label_set_text(GTK_LABEL(d->freqlabel), buf);
    g_free(buf);

    for (i = 0; i < NUM_FREQS; i++) {
        if (d->native_rate == mixfreqs[i])
            selected = i;
    }
    if (selected == -1)
        selected = i;

    gtk_combo_box_set_active(GTK_COMBO_BOX(d->freq_combo), selected);
}

static void
context_state_callback(pa_context* c, void* dp)
{
    pulse_driver* const d = dp;

    pa_threaded_mainloop_signal(d->mainloop, 0);
}

static void
stream_state_callback(pa_stream* s, void* dp) {
    pulse_driver* const d = dp;
    const pa_sample_spec *spc;

    g_assert(s);
    switch (pa_stream_get_state(s)) {
    case PA_STREAM_TERMINATED:
    case PA_STREAM_CREATING:
        break;

    case PA_STREAM_READY:
        spc = pa_stream_get_sample_spec(s);
        d->rate = spc->rate;
        d->format = (spc->format == PA_SAMPLE_S16LE ?
            ST_MIXER_FORMAT_S16_LE : ST_MIXER_FORMAT_S16_BE) | ST_MIXER_FORMAT_STEREO;
        d->state = PULSE_STATE_READY;
        update_controls(d);

        break;
    case PA_STREAM_FAILED:
    default:
        d->state = PULSE_STATE_ERROR;
        break;
    }
    pa_threaded_mainloop_signal(d->mainloop, 0);
}

static void
stream_write_callback(pa_stream* s, size_t length, void* dp)
{
    pulse_driver* const d = dp;

    d->len = length;
    pa_stream_begin_write(s, &d->sndbuf, &d->len);

    if (d->state == PULSE_STATE_RUNNING) {
        g_assert(s);

        /* sndbuf length is in bytes, but mixing routine gets number of samples */
        d->callback(d->sndbuf, d->len >> 2, d->rate, d->format);
    } else {
        /* Silently ignore this case and feed PulseAudio with zeroes. This can happen
           on playback stopping */
        memset(d->sndbuf, 0, d->len);
        pa_stream_write(d->stream, d->sndbuf, d->len, NULL, 0, PA_SEEK_RELATIVE);
    }
}

static void
pulse_activate(void* dp, const gchar* group)
{
    pulse_driver* const d = dp;
    pa_sample_spec sample_spec;
    pa_channel_map map;
    pa_buffer_attr buffer_attr;
    gchar* buf;

    d->starttime = 0.0;
    d->mainloop = pa_threaded_mainloop_new();
    if (!d->mainloop) {
        error_error(_("PulseAudio driver error: Cannot create a main loop"));
        d->state = PULSE_STATE_ERROR;
        return;
    }

    d->context = pa_context_new(pa_threaded_mainloop_get_api(d->mainloop),
        PACKAGE_NAME);
    if (!d->context) {
        error_error(_("PulseAudio driver error: Cannot create a new context"));
        d->state = PULSE_STATE_ERROR;
        return;
    }

    pa_context_set_state_callback(d->context, context_state_callback, dp);
    pa_threaded_mainloop_lock(d->mainloop);
    if (pa_threaded_mainloop_start(d->mainloop)) {
        error_error(_("PulseAudio driver error: Cannot start the main loop"));
        d->state = PULSE_STATE_ERROR;
        goto out;
    }
    if (pa_context_connect(d->context, NULL, 0, NULL)) {
        pulse_error(N_("Cannot connect the context"), d);
        d->state = PULSE_STATE_ERROR;
        goto out;
    }

    for (;;) {
        pa_context_state_t context_state = pa_context_get_state(d->context);
        if (!PA_CONTEXT_IS_GOOD(context_state)) {
            pulse_error(N_("Bad context"), d);
            d->state = PULSE_STATE_ERROR;
            goto out;
        }
        if (context_state == PA_CONTEXT_READY)
            break;
        pa_threaded_mainloop_wait(d->mainloop);
    }

    sample_spec.format = PA_SAMPLE_S16NE;
    sample_spec.rate = d->native_rate ? d->native_rate : 48000;
    sample_spec.channels = 2;

    pa_channel_map_init_stereo(&map);

    buf = g_strdup_printf("%s output", PACKAGE_NAME);
    d->stream = pa_stream_new(d->context, buf, &sample_spec, NULL);
    g_free(buf);
    pa_stream_set_state_callback(d->stream, stream_state_callback, dp);
    pa_stream_set_write_callback(d->stream, stream_write_callback, dp);

    /* recommended settings, i.e. server uses sensible values */
    buffer_attr.maxlength = (uint32_t)-1;
    buffer_attr.tlength = BSIZE;
    buffer_attr.prebuf = (uint32_t)0;
    buffer_attr.minreq = (uint32_t)-1;

    if (pa_stream_connect_playback(d->stream, NULL, &buffer_attr,
            (d->native_rate ? 0 : PA_STREAM_FIX_RATE) | PA_STREAM_START_CORKED | PA_STREAM_INTERPOLATE_TIMING |
            PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY,
            NULL, NULL)) {
        pulse_error(N_("Cannot connect the playback stream"), d);
        d->state = PULSE_STATE_ERROR;
        goto out;
    }

    /* Wait for the stream to be ready */
    for (;;) {
        gboolean finished = FALSE;

        switch (d->state) {
        case PULSE_STATE_READY:
            finished = TRUE;
            break;
        case PULSE_STATE_ERROR:
            pulse_error(N_("Error while setting up the stream"), d);
            goto out;
        default:
            break;
        }
        if (finished)
            break;

        pa_threaded_mainloop_wait(d->mainloop);
    }
out:
    pa_threaded_mainloop_unlock(d->mainloop);
}

static void
pulse_deactivate(void* dp)
{
    pulse_driver* const d = dp;

    if (!d->mainloop)
        return;

    pa_threaded_mainloop_lock(d->mainloop);
    if (d->stream) {
        pa_stream_disconnect(d->stream);
        pa_stream_unref(d->stream);
        d->stream = NULL;
    }
    if (d->context) {
        pa_context_unref(d->context);
        d->context = NULL;
    }
    pa_threaded_mainloop_unlock(d->mainloop);

    pa_threaded_mainloop_stop(d->mainloop);
    pa_threaded_mainloop_free(d->mainloop);
    d->mainloop = NULL;
    d->state = PULSE_STATE_NOTREADY;
}

static void
release_success_cb(pa_stream* s, int success, void* dp)
{
    pulse_driver* const d = dp;

    d->state = success ? PULSE_STATE_READY : PULSE_STATE_ERROR;
}

static void
pulse_release(void* dp)
{
    pulse_driver* const d = dp;
    pa_usec_t usec;

    if (d->state != PULSE_STATE_RUNNING)
        return;

    pa_threaded_mainloop_lock(d->mainloop);
    pa_stream_flush(d->stream, NULL, NULL);
    pa_stream_cork(d->stream, 1, release_success_cb, dp);
    pa_threaded_mainloop_unlock(d->mainloop);
    for (;;) {
        const struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000};
        if (d->state != PULSE_STATE_RUNNING)
            break;
        nanosleep(&ts, NULL);
    }

    pa_stream_get_time(d->stream, &usec);
    /* If the sound stream is "corked" (that is, paused), the
       playtime will not be reset on next resume. So we subtract
       the last pause time point plus some delay of about 100ms
       added by PulseAudio */
    d->starttime = (gdouble)usec / 1.0e6 + 0.1;
}

static void
open_success_cb(pa_stream* s, int success, void* dp)
{
    pulse_driver* const d = dp;

    d->state = success ? PULSE_STATE_RUNNING : PULSE_STATE_ERROR;
}

static gboolean
pulse_open(void* dp)
{
    pulse_driver* const d = dp;

    if (d->state == PULSE_STATE_READY) {
        pa_threaded_mainloop_lock(d->mainloop);
        pa_stream_cork(d->stream, 0, open_success_cb, dp);
        pa_threaded_mainloop_unlock(d->mainloop);

        return TRUE;
    } else {
        error_error(_("PulseAudio driver error: Driver is not ready"));
        return FALSE;
    }
}

static double
pulse_get_play_time(void* dp)
{
    pa_usec_t usec;
    pulse_driver* const d = dp;

    pa_stream_get_time(d->stream, &usec);
    return (gdouble)usec / 1.0e6 - d->starttime;
}

static inline int
pulse_get_play_rate(void* d)
{
    pulse_driver* const dp = d;
    return dp->rate;
}

static gboolean
pulse_loadsettings(void* dp,
    const gchar* f)
{
    pulse_driver* const d = dp;

    d->native_rate = prefs_get_int(f, "pulse-rate", 0);
    update_controls(d);

    return TRUE;
}

static gboolean
pulse_savesettings(void* dp,
    const gchar* f)
{
    pulse_driver* const d = dp;

    prefs_put_int(f, "pulse-rate", d->native_rate);

    return TRUE;
}

static void
pulse_commit(void* dp)
{
    pulse_driver * const d = dp;

    pa_stream_write(d->stream, d->sndbuf, d->len, NULL, 0LL, PA_SEEK_RELATIVE);
}

static void
mixfreq_changed(GtkComboBox* cb, pulse_driver* d)
{
    gint pos = gtk_combo_box_get_active(cb);
    guint new_rate;
    gboolean running = FALSE;
    static gdouble prev_time = 0.0;

    if (pos < 0 || pos > NUM_FREQS)
        return;

    new_rate = (pos == NUM_FREQS) ? 0 : mixfreqs[pos];
    if (d->native_rate == new_rate)
        return;

    d->native_rate = new_rate;
    /* Restart driver if it was in running state */
    if (d->state == PULSE_STATE_RUNNING) {
        running = TRUE;
        pulse_release(d);
        prev_time -= d->starttime - 0.1; /* Since we add 0.1s on release */
    }
    pulse_deactivate(d);
    pulse_activate(d, NULL);
    if (running) { /* Play time is reset on driver restart. So we compensate this */
        d->starttime = prev_time;
        pulse_open(d);
    }
}

static void
pulse_make_config_widgets(pulse_driver* d)
{
    GtkWidget *thing, *mainbox, *hbox;
    GtkListStore* ls;
    GtkTreeIter iter;
    gchar* buf;
    gint selected = -1, i;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);
    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mainbox), hbox, FALSE, FALSE, 0);

    thing = gtk_label_new(_("Frequency [Hz]:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

    ls = gtk_list_store_new(1, G_TYPE_STRING);

    for (i = 0; i < NUM_FREQS; i++) {
        if (d->native_rate == mixfreqs[i])
            selected = i;
        buf = g_strdup_printf("%u", mixfreqs[i]);
        gtk_list_store_append(ls, &iter);
        gtk_list_store_set(ls, &iter, 0, buf, -1);
        g_free(buf);
    }
    if (selected == -1)
        selected = i;

    gtk_list_store_append(ls, &iter);
    gtk_list_store_set(ls, &iter, 0, _("Server default"), -1);
    d->freq_combo = thing = gui_combo_new(ls);
    gtk_combo_box_set_active(GTK_COMBO_BOX(thing), selected);
    g_signal_connect(thing, "changed", G_CALLBACK(mixfreq_changed), d);
    gtk_box_pack_end(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mainbox), hbox, FALSE, FALSE, 0);

    thing = gtk_label_new(_("Actual frequency [Hz]:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    buf = g_strdup_printf("%u", d->rate);
    d->freqlabel = gtk_label_new(buf);
    g_free(buf);
    gtk_box_pack_end(GTK_BOX(hbox), d->freqlabel, FALSE, FALSE, 0);

    gtk_widget_show_all(mainbox);
}

static GtkWidget*
pulse_getwidget(void* dp)
{
    pulse_driver* const d = dp;

    return d->configwidget;
}

static void
pulse_destroy(void* dp)
{
    pulse_driver* const d = dp;

    gtk_widget_destroy(d->configwidget);

    g_free(dp);
}

static void*
pulse_new(gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat))
{
    pulse_driver* d = g_new(pulse_driver, 1);

    d->context = NULL;
    d->stream = NULL;
    d->sndbuf = NULL;
    d->callback = callback;
    d->mainloop = NULL;
    d->state = PULSE_STATE_NOTREADY;
    d->rate = d->native_rate = 0;
#ifdef WORDS_BIGENDIAN
    d->format = ST_MIXER_FORMAT_U16_BE | ST_MIXER_FORMAT_STEREO;
#elde
    d->format = ST_MIXER_FORMAT_U16_LE | ST_MIXER_FORMAT_STEREO;
#endif

    pulse_make_config_widgets(d);

    return d;
}

st_driver driver_out_pulse = {
    "Pulseaudio Output",

    pulse_new,
    pulse_destroy,

    pulse_open,
    pulse_release,

    pulse_getwidget,
    pulse_loadsettings,
    pulse_savesettings,

    pulse_activate,
    pulse_deactivate,

    pulse_commit,

    pulse_get_play_time,
    pulse_get_play_rate
};

#endif /* DRIVER_PULSE */
