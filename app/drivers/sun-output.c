
/*
 * The Real SoundTracker - Sun (output) driver.
 * 
 * Copyright (C) 2001, 2002, 2003 CubeSoft Communications, Inc.
 * <http://www.csoft.org>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/audioio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "audio-subs.h"
#include "driver.h"
#include "driver-subs.h"
#include "errors.h"
#include "gui-subs.h"
#include "mixer.h"
#include "preferences.h"

typedef struct sun_driver {
    GtkWidget* configwidget;
    GtkWidget* prefs_devaudio_w;
    GtkWidget* prefs_resolution_w[2];
    GtkWidget* prefs_channels_w[2];
    GtkWidget* prefs_mixfreq_w[4];
    GtkWidget *bufsizespin_w, *bufsizelabel_w, *estimatelabel_w;

    int playrate;
    int stereo;
    int bits;
    int bufsize;
    int numbufs;
    int mf;
    gboolean realtimecaps;

    int soundfd;
    void* sndbuf;
    gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat);
    int firstpoll;

    gchar* p_devaudio;
    int p_resolution;
    int p_channels;
    int p_mixfreq;
    int p_bufsize;

    double outtime;
    double playtime;

    audio_info_t info;
    DRIVER_THREAD_STUFF
} sun_driver;

static const int mixfreqs[] = { 8000, 16000, 22050, 44100, -1 };

static void*
sun_playing(void* data)
{
    sun_driver* const d = data;
    static int size;
    static struct timeval tv;

    DRIVER_THREAD_LOOP_BEGIN(d, TRUE)
        if (!d->firstpoll) {
            size = (d->stereo + 1) * (d->bits / 8) * d->bufsize;
            write(d->soundfd, d->sndbuf, size);

            if (!d->realtimecaps) {
                gettimeofday(&tv, NULL);
                d->outtime = tv.tv_sec + tv.tv_usec / 1e6;
                d->playtime += (double)d->bufsize / d->playrate;
            }
        }

        d->firstpoll = FALSE;

        d->callback(d->sndbuf, d->bufsize, d->playrate, d->mf);
    DRIVER_THREAD_LOOP_END(d)

    return NULL;
}

static void
prefs_init_from_structure(sun_driver* d)
{
    int i;

    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(d->prefs_resolution_w[d->p_resolution / 8 - 1]),
        TRUE);
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(d->prefs_channels_w[d->p_channels - 1]),
        TRUE);

    for (i = 0; mixfreqs[i] != -1; i++) {
        if (d->p_mixfreq == mixfreqs[i])
            break;
    }
    if (mixfreqs[i] == -1) {
        i = 3;
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->prefs_mixfreq_w[i]), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(d->bufsizespin_w), d->p_bufsize);

    gtk_entry_set_text(GTK_ENTRY(d->prefs_devaudio_w), d->p_devaudio);
}

static void
prefs_update_estimate(sun_driver* d)
{
    gchar* buf = g_strdup_printf(_("Estimated audio delay: %f milliseconds"),
        (double)(1000 * (1 << d->p_bufsize)) / d->p_mixfreq);

    gtk_label_set_text(GTK_LABEL(d->estimatelabel_w), buf);
    g_free(buf);
}

static void
prefs_resolution_changed(void* a,
    sun_driver* d)
{
    d->p_resolution = (find_current_toggle(d->prefs_resolution_w, 2) + 1) * 8;
}

static void
prefs_channels_changed(void* a,
    sun_driver* d)
{
    d->p_channels = find_current_toggle(d->prefs_channels_w, 2) + 1;
}

static void
prefs_mixfreq_changed(void* a,
    sun_driver* d)
{
    d->p_mixfreq = mixfreqs[find_current_toggle(d->prefs_mixfreq_w, 4)];
    prefs_update_estimate(d);
}

static void
prefs_bufsize_changed(GtkSpinButton* w,
    sun_driver* d)
{
    gchar* buf;

    d->p_bufsize = gtk_spin_button_get_value_as_int(w);

    buf = g_strdup_printf(_("(%d dicretes)"), 1 << d->p_bufsize);
    gtk_label_set_text(GTK_LABEL(d->bufsizelabel_w), buf);
    g_free(buf);
    prefs_update_estimate(d);
}

static void
sun_devaudio_changed(void* a,
    sun_driver* d)
{
    gchar *buf = gtk_entry_get_text(GTK_ENTRY(d->prefs_devaudio_w);

    if (strcmp(buf, d->p_devaudio)) {
        g_free(d->p_devaudio);
        d->p_devaudio = g_strdup(buf);
    }
}

static void
sun_make_config_widgets(sun_driver* d)
{
    GtkWidget *thing, *mainbox, *box2, *box3;
    static const char* resolutionlabels[] = { "8 bits", "16 bits" };
    static const char* channelslabels[] = { "Mono", "Stereo" };
    static const char* mixfreqlabels[] = { "8000", "16000", "22050", "44100" };

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);

    thing = gtk_label_new(
        _("These changes won't take effect until you restart playing."));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Output device (e.g. '/dev/audio'):"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);

    thing = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(thing), 126);
    gtk_widget_show(thing);
    gtk_box_pack_end(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    gtk_entry_set_text(GTK_ENTRY(thing), d->p_devaudio);
    g_signal_connect_after(thing, "changed",
        G_CALLBACK(sun_devaudio_changed), d);
    d->prefs_devaudio_w = thing;

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Resolution:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    make_radio_group_full_ext(resolutionlabels, box2, d->prefs_resolution_w,
        FALSE, TRUE, (void (*)())prefs_resolution_changed, d, TRUE);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Channels:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    make_radio_group_full_ext(channelslabels, box2, d->prefs_channels_w,
        FALSE, TRUE, (void (*)())prefs_channels_changed, d, TRUE);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Frequency [Hz]:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    make_radio_group_full_ext(mixfreqlabels, box2, d->prefs_mixfreq_w,
        FALSE, TRUE, (void (*)())prefs_mixfreq_changed, d, TRUE);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Buffer Size:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);

    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_end(GTK_BOX(box2), box3, FALSE, FALSE, 0);
    gtk_widget_show(box3);

    d->bufsizespin_w = thing = gtk_spin_button_new(GTK_ADJUSTMENT(
                                                       gtk_adjustment_new(5.0, 5.0, 15.0, 1.0, 1.0, 0.0)),
        0, 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "value-changed",
        G_CALLBACK(prefs_bufsize_changed), d);

    d->bufsizelabel_w = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    d->estimatelabel_w = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    prefs_init_from_structure(d);
}

static GtkWidget*
sun_getwidget(void* dp)
{
    sun_driver* const d = dp;

    return d->configwidget;
}

static void*
sun_new(gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat))
{
    sun_driver* d = g_new(sun_driver, 1);

    d->p_devaudio = g_strdup("/dev/audio");
    d->p_mixfreq = 44100;
    d->p_channels = 2;
    d->p_resolution = 16;
    d->p_bufsize = 11; // 2048;
    d->soundfd = -1;
    d->sndbuf = NULL;
    d->callback = callback;

    DRIVER_THREAD_INIT(d)

    sun_make_config_widgets(d);

    return d;
}

static void
sun_destroy(void* dp)
{
    sun_driver* const d = dp;

    gtk_widget_destroy(d->configwidget);
    DRIVER_THREAD_CLEAR(d)

    g_free(dp);
}

static gboolean
sun_try_format(sun_driver* d, int fmt, int precision)
{
    audio_encoding_t enc;

    for (enc.index = 0; ioctl(d->soundfd, AUDIO_GETENC, &enc) == 0;
         enc.index++) {
        if (enc.encoding == fmt && enc.precision == precision) {
            d->info.play.encoding = enc.encoding;
            d->info.play.precision = enc.precision;
            if (ioctl(d->soundfd, AUDIO_SETINFO, &d->info) == 0) {
                return TRUE;
            } else {
                return FALSE;
            }
        }
    }

    return FALSE;
}

static gboolean
sun_try_channels(sun_driver* d, int nch)
{
    d->info.play.channels = nch;
    if (ioctl(d->soundfd, AUDIO_SETINFO, &d->info) != 0) {
        return FALSE;
    }

    return TRUE;
}

static void
sun_release(void* dp)
{
    sun_driver* const d = dp;

    DRIVER_THREAD_CLEAR(d)

    if (d->soundfd >= 0) {
        ioctl(d->soundfd, AUDIO_FLUSH, NULL);
        close(d->soundfd);
        d->soundfd = -1;
    }

    DRIVER_THREAD_WAIT(d)

    free(d->sndbuf);
    d->sndbuf = NULL;
}

static gboolean
sun_open(void* dp)
{
    gchar* buf;
    sun_driver* const d = dp;
    int mf = 0, i;

    AUDIO_INITINFO(&d->info);

    errno = 0;
    d->soundfd = open(d->p_devaudio, O_WRONLY);
    if (d->soundfd < 0) {
        buf = g_strdup_printf(_("SUN output (%s): Cannot open device"), d->p_devaudio);
        error_errno(buf);
        g_free(buf);
        goto out;
    }

    d->info.mode = AUMODE_PLAY;
    if (ioctl(d->soundfd, AUDIO_SETINFO, &d->info) != 0) {
        buf = g_strdup_printf(_("SUN output (%s) does not support playback"), d->p_devaudio);
        error_errno(buf);
        g_free(buf);
        goto out;
    }

    d->playrate = d->p_mixfreq;
    d->info.play.sample_rate = d->playrate;
    if (ioctl(d->soundfd, AUDIO_SETINFO, &d->info) != 0) {
        buf = g_strdup_printf(_("SUN output (%s): Cannot handle %d Hz"), d->p_devaudio);
        error_errno(buf);
        g_free(buf);
        goto out;
    }

    d->bits = 0;
    if (d->p_resolution == 16) {
        if (sun_try_format(d, AUDIO_ENCODING_SLINEAR_LE, 16)) {
            d->bits = 16;
            mf = ST_MIXER_FORMAT_S16_LE;
        } else if (sun_try_format(d, AUDIO_ENCODING_SLINEAR_BE, 16)) {
            d->bits = 16;
            mf = ST_MIXER_FORMAT_S16_BE;
        } else if (sun_try_format(d, AUDIO_ENCODING_ULINEAR_LE, 16)) {
            d->bits = 16;
            mf = ST_MIXER_FORMAT_U16_LE;
        } else if (sun_try_format(d, AUDIO_ENCODING_ULINEAR_BE, 16)) {
            d->bits = 16;
            mf = ST_MIXER_FORMAT_U16_BE;
        }
    }
    if (d->bits != 16) {
        if (sun_try_format(d, AUDIO_ENCODING_SLINEAR, 8)) {
            d->bits = 8;
            mf = ST_MIXER_FORMAT_S8;
        } else if (sun_try_format(d, AUDIO_ENCODING_PCM8, 8)) {
            d->bits = 8;
            mf = ST_MIXER_FORMAT_U8;
        } else {
            buf = g_strdup_printf(_("SUN output (%s): Required sound encoding not supported"), d->p_devaudio);
            error_error(buf);
            g_free(buf);
            goto out;
        }
    }

    if (d->p_channels == 2 && sun_try_channels(d, 2)) {
        d->stereo = 1;
        mf |= ST_MIXER_FORMAT_STEREO;
    } else if (sun_try_channels(d, 1)) {
        d->stereo = 0;
    }

    d->mf = mf;

    i = 0x00020000 + d->p_bufsize + d->stereo + (d->bits / 8 - 1);
    d->info.blocksize = 1 << (i & 0xffff);
    d->info.hiwat = ((unsigned)i >> 16) & 0x7fff;
    if (d->info.hiwat == 0) {
        d->info.hiwat = 65536;
    }
    if (ioctl(d->soundfd, AUDIO_SETINFO, &d->info) != 0) {
        buf = g_strdup_printf(_("SUN output (%s): Cannot set block size"), d->p_devaudio);
        error_errno(buf);
        g_free(buf);
        goto out;
    }

    if (ioctl(d->soundfd, AUDIO_GETINFO, &d->info) != 0) {
        buf = g_strdup_printf(_("SUN input (%s): Cannot get device information"), d->p_devaudio);
        error_errno(buf);
        g_free(buf);
        goto out;
    }
    d->bufsize = d->info.blocksize;
    d->numbufs = d->info.hiwat;

#if 0
    if (ioctl(d->soundfd, AUDIO_GETPROPS, &i) == 0) {
        /* XXX use only if we are recording. */
        d->realtimecaps = i & AUDIO_PROP_FULLDUPLEX;
    }
#endif

    d->sndbuf = calloc(1, d->bufsize);

    if (d->stereo == 1) {
        d->bufsize /= 2;
    }
    if (d->bits == 16) {
        d->bufsize /= 2;
    }

    d->firstpoll = TRUE;
    d->playtime = 0;
    DRIVER_THREAD_RESUME(d)

    return TRUE;

out:
    sun_release(dp);
    return FALSE;
}

static double
sun_get_play_time(void* dp)
{
    sun_driver* const d = dp;

    if (d->realtimecaps) {
        static audio_offset_t ooffs;

        ioctl(d->soundfd, AUDIO_GETOOFFS, &ooffs);

        return (double)ooffs.samples / (d->stereo + 1) / (d->bits / 8) / d->playrate;
    } else {
        struct timeval tv;
        double curtime;

        gettimeofday(&tv, NULL);
        curtime = tv.tv_sec + tv.tv_usec / 1e6;

        return d->playtime + curtime - d->outtime - d->numbufs * ((double)d->bufsize / d->playrate);
    }
}

static inline int
sun_get_play_rate(void* d)
{
    sun_driver* const dp = d;
    return dp->playrate;
}

static gboolean
sun_loadsettings(void* dp,
    const gchar* f)
{
    gchar* buf;
    sun_driver* const d = dp;

    if ((buf = prefs_get_string(f, "sun-devaudio", NULL))) {
        g_free(d->p_devaudio);
        d->p_devaudio = buf;
    }
    d->p_resolution = prefs_get_int(f, "sun-resolution", 16);
    d->p_channels = prefs_get_int(f, "sun-channels", 2);
    d->p_mixfreq = prefs_get_int(f, "sun-mixfreq", 44100);
    d->p_bufsize = prefs_get_int(f, "sun-bufsize", 11);

    prefs_init_from_structure(d);

    return TRUE;
}

static gboolean
sun_savesettings(void* dp,
    const gchar* f)
{
    sun_driver* const d = dp;

    prefs_put_string(f, "sun-devaudio", d->p_devaudio);
    prefs_put_int(f, "sun-resolution", d->p_resolution);
    prefs_put_int(f, "sun-channels", d->p_channels);
    prefs_put_int(f, "sun-mixfreq", d->p_mixfreq);
    prefs_put_int(f, "sun-bufsize", d->p_bufsize);

    return TRUE;
}

static void
sun_activate(void *dp, const gchar* group)
{
    sun_driver * const d = dp;

    DRIVER_THREAD_NEW(d, sun_playing, dp, "Playback")
}

static void
sun_deactivate(void *dp)
{
    sun_driver * const d = dp;

    DRIVER_THREAD_CANCEL(d)
}

static void
sun_commit (void* dp)
{
    sun_driver * const d = dp;

    DRIVER_THREAD_COMMIT(d)
}

st_driver driver_out_sun = {
    "Sun Output",

    sun_new,
    sun_destroy,

    sun_open,
    sun_release,

    sun_getwidget,
    sun_loadsettings,
    sun_savesettings,

    sun_activate,
    sun_deactivate,

    sun_commit,

    sun_get_play_time,
    sun_get_play_rate
};
