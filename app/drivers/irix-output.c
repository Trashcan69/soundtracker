
/*
 * The Real SoundTracker - IRIX 5.x (output) driver.
 *
 * Copyright (C) 2001-2019 Michael Krause
 * Inspired by libmikmod's drv_sgi.c
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

#if DRIVER_SGI

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dmedia/audio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "driver.h"
#include "driver-subs.h"
#include "errors.h"
#include "gui-subs.h"
#include "mixer.h"
#include "preferences.h"

#define DEFAULT_SGI_FRAGSIZE 2048

typedef struct irix_driver {
    GtkWidget* configwidget;
    GMutex* configmutex;

    ALconfig sgi_config;
    ALport sgi_port;
    int sample_factor;
    int sgi_fragsize;
    int sgi_bufsize;
    char* audiobuffer;
    gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat);

    gboolean firstpoll;
    double outtime;
    double playtime;
    DRIVER_THREAD_STUFF
} irix_driver;

static void*
irix_playing(void* data)
{
    irix_driver* const d = data;
    struct timeval tv;

    DRIVER_THREAD_LOOP_BEGIN(d, TRUE)
        if (!d->firstpoll) {
            alWriteFrames(d->sgi_port, d->audiobuffer, d->sgi_fragsize);

            if (1) {
                gettimeofday(&tv, NULL);
                d->outtime = tv.tv_sec + tv.tv_usec / 1e6;
                d->playtime += (double)d->sgi_fragsize / 48000;
            }
        }

        d->firstpoll = FALSE;

        d->callback(d->audiobuffer, d->sgi_fragsize,
            48000, ST_MIXER_FORMAT_S16_BE | ST_MIXER_FORMAT_STEREO);
    DRIVER_THREAD_LOOP_END(d)

    return NULL;
}

static void
irix_make_config_widgets(irix_driver* d)
{
    GtkWidget *thing, *mainbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);

    thing = gtk_label_new(_("no settings (yet), sorry!"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
}

static GtkWidget*
irix_getwidget(void* dp)
{
    irix_driver* const d = dp;

    return d->configwidget;
}

static void*
irix_new(gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat))
{
    irix_driver* d = g_new(irix_driver, 1);

    g_mutex_init(&d->configmutex);

    d->sgi_fragsize = DEFAULT_SGI_FRAGSIZE;
    d->audiobuffer = NULL;
    d->callback = callback;

    DRIVER_THREAD_INIT(d)

    irix_make_config_widgets(d);

    return d;
}

static void
irix_destroy(void* dp)
{
    irix_driver* const d = dp;

    gtk_widget_destroy(d->configwidget);
    g_mutex_free(d->configmutex);
    DRIVER_THREAD_CLEAR(d)

    g_free(dp);
}

static void
irix_release(void* dp)
{
    irix_driver* const d = dp;

    DRIVER_THREAD_STOP(d)

    if (d->sgi_port) {
        alClosePort(d->sgi_port);
        d->sgi_port = NULL;
    }

    if (d->sgi_config) {
        alFreeConfig(d->sgi_config);
        d->sgi_config = NULL;
    }

    DRIVER_THREAD_WAIT(d)

    if (d->audiobuffer) {
        free(d->audiobuffer);
        d->audiobuffer = NULL;
    }
}

static gboolean
irix_open(void* dp)
{
    irix_driver* const d = dp;

    long chpars[] = { AL_OUTPUT_RATE, AL_RATE_48000 };

    ALseterrorhandler(0);
    ALsetparams(AL_DEFAULT_DEVICE, chpars, 2);

    if (!(d->sgi_config = ALnewconfig())) {
        error_error(_("ALnewconfig() failed."));
        goto out;
    }

    if (ALsetwidth(d->sgi_config, AL_SAMPLE_16) < 0) {
        error_error(_("16 Bit output not supported."));
        goto out;
    }
    d->sample_factor = 1;

    if (ALsetchannels(d->sgi_config, AL_STEREO) < 0) {
        error_error(_("Stereo output not supported."));
        goto out;
    }

    d->sgi_bufsize = d->sgi_fragsize * 2 * 2; // stereo, 16 bit

    alSetQueueSize(d->sgi_config, d->sgi_fragsize);
    if (!(d->sgi_port = ALopenport("soundtracker", "w", d->sgi_config))) {
        error_error(_("Couldn't open audio port."));
        goto out;
    }

    if (!(d->audiobuffer = calloc(1, d->sgi_bufsize)))
        goto out;

    alSetFillPoint(d->sgi_port, d->sgi_fragsize / 2);
    d->firstpoll = FALSE;
    d->outtime = 0.0;
    d->playtime = 0.0;

    DRIVER_THREAD_RESUME(d)
    return TRUE;

out:
    irix_release(dp);
    return FALSE;
}

static double
irix_get_play_time(void* dp)
{
    irix_driver* const d = dp;

    if (1) {
        struct timeval tv;
        double curtime;

        gettimeofday(&tv, NULL);
        curtime = tv.tv_sec + tv.tv_usec / 1e6;

        return d->playtime + curtime - d->outtime;
    }
}

static inline int
irix_get_play_rate(void* d)
{
    return 48000;
}

static void
irix_activate(void *dp, const gchar* group)
{
    irix_driver * const d = dp;

    DRIVER_THREAD_NEW(d, irix_playing, dp, "Playback")
}

static void
irix_deactivate(void *dp)
{
    irix_driver * const d = dp;

    DRIVER_THREAD_CANCEL(d)
}

static void
irix_commit (void* dp)
{
    irix_driver * const d = dp;

    DRIVER_THREAD_COMMIT(d)
}

st_driver driver_out_irix = {
    "IRIX Output",

    irix_new,
    irix_destroy,

    irix_open,
    irix_release,

    irix_getwidget,

    NULL,
    NULL,

    irix_activate,
    irix_deactivate,

    irix_commit,

    irix_get_play_time,
    irix_get_play_rate
};

#endif /* DRIVER_IRIX */
