
/*
 * The Real SoundTracker - OSS (input/output) driver.
 *
 * Copyright (C) 1998-2019 Michael Krause
 * Copyright (C) 2022 Yury Aliaev
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

#if DRIVER_OSS

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#elif HAVE_MACHINE_SOUNDCARD_H
#include <machine/soundcard.h>
#elif HAVE_SOUNDCARD_H
#include <soundcard.h>
#endif
#include <sys/time.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "audio-subs.h"
#include "driver.h"
#include "driver-subs.h"
#include "errors.h"
#include "gui-subs.h"
#include "mixer.h"
#include "preferences.h"

typedef struct oss_driver {
    GtkWidget* configwidget;
    GtkWidget* prefs_devdsp_w;
    GtkWidget* prefs_resolution_w[2];
    GtkWidget* prefs_channels_w[2];
    GtkWidget* prefs_mixfreq_w[4];
    GtkWidget *bufsizespin_w, *bufsizelabel_w, *estimatelabel_w;

    int playrate;
    int stereo;
    int bits;
    int fragsize, size;
    int numfrags;
    int mf;
    gboolean realtimecaps;

    int soundfd;
    void* sndbuf;
    DOUBLE_BUFFER_STUFF
    gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat);
    int firstpoll;

    gchar* p_devdsp_saved;
    const gchar* p_devdsp;
    int p_resolution;
    int p_channels;
    int p_mixfreq;
    int p_fragsize;

    double outtime;
    double playtime;

    gboolean sampling;
    DRIVER_THREAD_STUFF
} oss_driver;

static const int mixfreqs[] = { 8000, 16000, 22050, 44100, -1 };

static void*
oss_playing(void* data)
{
    oss_driver* const d = data;
    int w;
    struct timeval tv;

    DRIVER_THREAD_LOOP_BEGIN(d, TRUE)
        if (!d->firstpoll) {

            errno = 0;
            if ((w = write(d->soundfd, d->sndbuf, d->size) != d->size)) {
                if (w == -1) {
                    error_errno(_("OSS output: writing error"));
                    DRIVER_THREAD_ERROR
                } else {
                    fprintf(stderr, "driver_oss: write not completely done.\n");
                }
            }

            if (!d->realtimecaps) {
                gettimeofday(&tv, NULL);
                d->outtime = tv.tv_sec + tv.tv_usec / 1e6;
                d->playtime += (double)d->fragsize / d->playrate;
            }
        }

        d->firstpoll = FALSE;

        d->callback(d->sndbuf, d->fragsize, d->playrate, d->mf);
    DRIVER_THREAD_LOOP_END(d)

    return NULL;
}

static void*
oss_sampling(void *data)
{
    oss_driver* const d = data;

    DRIVER_THREAD_LOOP_BEGIN(d, FALSE);
    errno = 0;
    if (read(d->soundfd, d->sndbuf, d->size) != d->size) {
        error_errno(_("OSS input: reading error"));
        DRIVER_THREAD_ERROR
    }

    if (d->callback(d->sndbuf, d->size, d->playrate, d->mf)) {
        DOUBLE_BUFFER_ALLOCATE(d, d->sndbuf, d->size)
        if (!d->sndbuf) {
            error_errno(_("OSS input: out of memory error"));
            DRIVER_THREAD_ERROR
        }
    } else {
        DOUBLE_BUFFER_SWOP(d, d->sndbuf)
    }
    DRIVER_THREAD_LOOP_END(d)

    return NULL;
}

static void
prefs_init_from_structure(oss_driver* d)
{
    int i;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->prefs_resolution_w[d->p_resolution / 8 - 1]), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->prefs_channels_w[d->p_channels - 1]), TRUE);

    for (i = 0; mixfreqs[i] != -1; i++) {
        if (d->p_mixfreq == mixfreqs[i])
            break;
    }
    if (mixfreqs[i] == -1) {
        i = 3;
    }
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->prefs_mixfreq_w[i]), TRUE);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(d->bufsizespin_w), d->p_fragsize);

    gtk_entry_set_text(GTK_ENTRY(d->prefs_devdsp_w), d->p_devdsp_saved);
}

static void
prefs_update_estimate(oss_driver* d)
{
    gchar* buf = g_strdup_printf(_("Estimated audio delay: %f milliseconds"),
        1000 * (double)(1 << d->p_fragsize) / (double)d->p_mixfreq);

    gtk_label_set_text(GTK_LABEL(d->estimatelabel_w), buf);
    g_free(buf);
}

static void
prefs_resolution_changed(void* a,
    oss_driver* d)
{
    d->p_resolution = (find_current_toggle(d->prefs_resolution_w, 2) + 1) * 8;
}

static void
prefs_channels_changed(void* a,
    oss_driver* d)
{
    d->p_channels = find_current_toggle(d->prefs_channels_w, 2) + 1;
}

static void
prefs_mixfreq_changed(void* a,
    oss_driver* d)
{
    d->p_mixfreq = mixfreqs[find_current_toggle(d->prefs_mixfreq_w, 4)];

    if (!d->sampling)
        prefs_update_estimate(d);
}

static void
prefs_fragsize_changed(GtkSpinButton* w,
    oss_driver* d)
{
    gchar* buf;

    d->p_fragsize = gtk_spin_button_get_value_as_int(w);

    buf = g_strdup_printf(_("(%d discretes)"), 1 << d->p_fragsize);
    gtk_label_set_text(GTK_LABEL(d->bufsizelabel_w), buf);
    g_free(buf);
    if (!d->sampling)
        prefs_update_estimate(d);
}

static void
oss_devdsp_changed(void* a,
    oss_driver* d)
{
    d->p_devdsp = gtk_entry_get_text(GTK_ENTRY(d->prefs_devdsp_w));
}

static void
oss_make_config_widgets(oss_driver* d)
{
    GtkWidget *thing, *mainbox, *box2, *box3;
    static const char* resolutionlabels[] = { "8 bits", "16 bits" };
    static const char* channelslabels[] = { "Mono", "Stereo" };
    static const char* mixfreqlabels[] = { "8000", "16000", "22050", "44100" };

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);

    thing = gtk_label_new(_(d->sampling ? N_("These changes won't take effect until you restart sampling.")
                                        : N_("These changes won't take effect until you restart playing.")));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_widget_show(box2);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

    thing = gtk_label_new(_(d->sampling ? N_("Input device (e.g. '/dev/dsp'):")
                                        : N_("Output device (e.g. '/dev/dsp'):")));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);

    thing = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(thing), 126);
    gtk_widget_show(thing);
    gtk_box_pack_end(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    g_signal_connect_after(thing, "changed",
        G_CALLBACK(oss_devdsp_changed), d);
    d->prefs_devdsp_w = thing;

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

    d->bufsizespin_w = thing = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(5.0, 5.0, 15.0, 1.0, 1.0, 0.0)), 0, 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "value-changed",
        G_CALLBACK(prefs_fragsize_changed), d);

    d->bufsizelabel_w = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    if (!d->sampling) {
        d->estimatelabel_w = thing = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
        gtk_widget_show(thing);
    }

    prefs_init_from_structure(d);
}

static GtkWidget*
oss_getwidget(void* dp)
{
    oss_driver* const d = dp;

    return d->configwidget;
}

static oss_driver*
oss_new(gboolean sampling,
    gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat))
{
    oss_driver* d = g_new(oss_driver, 1);

    d->p_devdsp_saved = g_strdup("/dev/dsp");
    d->p_devdsp = d->p_devdsp_saved;
    d->p_mixfreq = 44100;
    d->p_channels = 2;
    d->p_resolution = 16;
    d->p_fragsize = 11; // 2048;
    d->soundfd = -1;
    d->sndbuf = NULL;
    DOUBLE_BUFFER_PREINIT(d)
    d->sampling = sampling;
    d->callback = callback;

    DRIVER_THREAD_INIT(d)

    oss_make_config_widgets(d);

    return d;
}

static void*
oss_new_playback(gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat))
{
    return oss_new(FALSE, callback);
}

static void*
oss_new_sampling(gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat))
{
    return oss_new(TRUE, callback);
}

static void
oss_destroy(void* dp)
{
    oss_driver* const d = dp;

    gtk_widget_destroy(d->configwidget);
    g_free(d->p_devdsp_saved);
    DRIVER_THREAD_CLEAR(d)

    g_free(dp);
}

static gboolean
oss_try_format(oss_driver* d, int f)
{
    int format = f;

    if (ioctl(d->soundfd, SNDCTL_DSP_SETFMT, &format) == -1) {
        error_errno(_("OSS driver: setting format failed"));
        return FALSE;
    }

    return format == f;
}

static gboolean
oss_try_stereo(oss_driver* d, int f)
{
    int format = f;

    if (ioctl(d->soundfd, SNDCTL_DSP_STEREO, &format) == -1) {
        error_errno(_("OSS driver: setting number of channels failed"));
        return FALSE;
    }

    return format == f;
}

static void
oss_release(void* dp)
{
    oss_driver* const d = dp;

    DRIVER_THREAD_STOP(d)

    if (d->soundfd >= 0) {
        ioctl(d->soundfd, SNDCTL_DSP_RESET, 0);
        close(d->soundfd);
        d->soundfd = -1;
    }

    DRIVER_THREAD_WAIT(d)
    DOUBLE_BUFFER_FREE(d)
}

static gboolean
oss_open(void* dp)
{
    oss_driver* const d = dp;
    int mf;
    int i;
    audio_buf_info info;

    /* O_NONBLOCK is required for the es1370 driver in Linux
       2.2.9, for example. It's open() behaviour is not
       OSS-conformant (though Thomas Sailer says it's okay). */
    errno = 0;
    if ((d->soundfd = open(d->p_devdsp, d->sampling ? O_RDONLY | O_NONBLOCK : O_WRONLY | O_NONBLOCK)) < 0) {
        gchar* buf;
        buf = g_strdup_printf(_("OSS driver (%s): Couldn't open %s"),
            d->sampling ? "sampling" : "sound output", d->p_devdsp);
        error_errno(buf);
        g_free(buf);
        goto out;
    }

    // Clear O_NONBLOCK again
    fcntl(d->soundfd, F_SETFL, 0);

    d->bits = 0;
    mf = 0;
    if (d->p_resolution == 16) {
        if (oss_try_format(d, AFMT_S16_LE)) {
            d->bits = 16;
            mf = ST_MIXER_FORMAT_S16_LE;
        } else if (oss_try_format(d, AFMT_S16_BE)) {
            d->bits = 16;
            mf = ST_MIXER_FORMAT_S16_BE;
        } else if (oss_try_format(d, AFMT_U16_LE)) {
            d->bits = 16;
            mf = ST_MIXER_FORMAT_U16_LE;
        } else if (oss_try_format(d, AFMT_U16_BE)) {
            d->bits = 16;
            mf = ST_MIXER_FORMAT_U16_BE;
        }
    }
    if (d->bits != 16) {
        if (oss_try_format(d, AFMT_S8)) {
            d->bits = 8;
            mf = ST_MIXER_FORMAT_S8;
        } else if (oss_try_format(d, AFMT_U8)) {
            d->bits = 8;
            mf = ST_MIXER_FORMAT_U8;
        } else {
            gchar* buf = g_strdup_printf(_("Required %s format not supported."),
                d->sampling ? "sampling" : "sound output");
            error_warning(buf);
            g_free(buf);
            goto out;
        }
    }

    if (d->p_channels == 2 && oss_try_stereo(d, 1)) {
        d->stereo = 1;
        mf |= ST_MIXER_FORMAT_STEREO;
    } else if (oss_try_stereo(d, 0)) {
        d->stereo = 0;
    }

    d->mf = mf;

    d->playrate = d->p_mixfreq;
    ioctl(d->soundfd, SNDCTL_DSP_SPEED, &d->playrate);

    if (d->sampling) {
        i = 0x00040000 + d->p_fragsize + d->stereo + (d->bits / 8 - 1);
        ioctl(d->soundfd, SNDCTL_DSP_SETFRAGMENT, &i);
        ioctl(d->soundfd, SNDCTL_DSP_GETBLKSIZE, &d->fragsize);
    } else {
        i = 0x00020000 + d->p_fragsize + d->stereo + (d->bits / 8 - 1);
        ioctl(d->soundfd, SNDCTL_DSP_SETFRAGMENT, &i);

        // Find out how many fragments OSS actually uses and how large they are.
        ioctl(d->soundfd, SNDCTL_DSP_GETOSPACE, &info);
        d->fragsize = info.fragsize;
        d->numfrags = info.fragstotal;

        ioctl(d->soundfd, SNDCTL_DSP_GETCAPS, &i);
        d->realtimecaps = i & DSP_CAP_REALTIME;
    }

    d->size = (d->stereo + 1) * (d->bits / 8) * d->fragsize;
    DOUBLE_BUFFER_INIT(d, d->sndbuf, d->sampling, d->size)

    if (d->sampling) {
        if (d->stereo == 1) {
            d->fragsize /= 2;
        }
        if (d->bits == 16) {
            d->fragsize /= 2;
        }

        // At least my ES1370 requires an initial read...
//        if (read(d->soundfd, d->sndbuf, d->fragsize) != d->fragsize)
//            perror("OSS input: read()");
    } else {
        d->firstpoll = TRUE;
        d->playtime = 0;
    }
    DRIVER_THREAD_RESUME(d)

    return TRUE;

out:
    oss_release(dp);
    return FALSE;
}

static double
oss_get_play_time(void* dp)
{
    oss_driver* const d = dp;

    if (d->realtimecaps) {
        count_info info;

        ioctl(d->soundfd, SNDCTL_DSP_GETOPTR, &info);

        return (double)info.bytes / (d->stereo + 1) / (d->bits / 8) / d->playrate;
    } else {
        struct timeval tv;
        double curtime;

        gettimeofday(&tv, NULL);
        curtime = tv.tv_sec + tv.tv_usec / 1e6;

        return d->playtime + curtime - d->outtime - d->numfrags * ((double)d->fragsize / d->playrate);
    }
}

static inline int
oss_get_play_rate(void* d)
{
    oss_driver* const dp = d;
    return dp->playrate;
}

static gboolean
oss_loadsettings(void* dp,
    const gchar* f)
{
    oss_driver* const d = dp;
    gchar* buf;

    if ((buf = prefs_get_string(f, "oss-devdsp", NULL))) {
        g_free(d->p_devdsp_saved);
        d->p_devdsp = d->p_devdsp_saved = buf;
    }
    d->p_resolution = prefs_get_int(f, "oss-resolution", d->p_resolution);
    d->p_channels = prefs_get_int(f, "oss-channels", d->p_channels);
    d->p_mixfreq = prefs_get_int(f, "oss-mixfreq", d->p_mixfreq);
    d->p_fragsize = prefs_get_int(f, "oss-fragsize", d->p_fragsize);

    prefs_init_from_structure(d);

    return TRUE;
}

static void
oss_activate(void *dp, const gchar* group)
{
    oss_driver * const d = dp;

    DRIVER_THREAD_NEW(d, d->sampling ? oss_sampling : oss_playing, dp,
        d->sampling ? "Sampling" : "Playback")
}

static void
oss_deactivate(void *dp)
{
    oss_driver * const d = dp;

    DRIVER_THREAD_CANCEL(d)
}

static gboolean
oss_savesettings(void* dp,
    const gchar* f)
{
    oss_driver* const d = dp;

    prefs_put_string(f, "oss-devdsp", d->p_devdsp);
    prefs_put_int(f, "oss-resolution", d->p_resolution);
    prefs_put_int(f, "oss-channels", d->p_channels);
    prefs_put_int(f, "oss-mixfreq", d->p_mixfreq);
    prefs_put_int(f, "oss-fragsize", d->p_fragsize);

    return TRUE;
}

static void
oss_commit (void* dp)
{
    oss_driver * const d = dp;

    DRIVER_THREAD_COMMIT(d)
}

st_driver driver_out_oss = {
    "OSS Output",

    oss_new_playback,
    oss_destroy,

    oss_open,
    oss_release,

    oss_getwidget,
    oss_loadsettings,
    oss_savesettings,

    oss_activate,
    oss_deactivate,

    oss_commit,

    oss_get_play_time,
    oss_get_play_rate
};

st_driver driver_in_oss = {
    "OSS Input",

    oss_new_sampling,
    oss_destroy,

    oss_open,
    oss_release,

    oss_getwidget,
    oss_loadsettings,
    oss_savesettings,

    oss_activate,
    oss_deactivate,

    NULL,

    oss_get_play_time,
    oss_get_play_rate
};

#endif /* DRIVER_OSS */
