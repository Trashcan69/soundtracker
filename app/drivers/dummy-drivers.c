
/*
 * The Real SoundTracker - Dummy drivers
 *
 * Copyright (C) 2001-2019 Michael Krause
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

#include <stdlib.h>
#include <sys/time.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "driver.h"
#include "mixer.h"

#ifdef WORDS_BIGENDIAN
const STMixerFormat format = ST_MIXER_FORMAT_S16_BE;
#else
const STMixerFormat format = ST_MIXER_FORMAT_S16_LE;
#endif
const gint bufsize = 1024;
const gint rate = 44100;
const gint prebuffered = 2;

typedef struct dummy_driver {
    GtkWidget* configwidget;
    void* buf;
    gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat);
    double outtime;
    double playtime;
    gint prebuf;
} dummy_driver;

static void
dummy_make_config_widgets(dummy_driver* d)
{
    GtkWidget *thing, *mainbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);

    thing = gtk_label_new(_("No driver available for your system."));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
}

static GtkWidget*
dummy_getwidget(void* dp)
{
    dummy_driver* const d = dp;

    return d->configwidget;
}

static void*
dummy_new(gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat))
{
    dummy_driver* d = g_new(dummy_driver, 1);

    d->callback = callback;
    dummy_make_config_widgets(d);

    return d;
}

static void
dummy_destroy(void* dp)
{
    dummy_driver* const d = dp;

    gtk_widget_destroy(d->configwidget);
    g_free(dp);
}

static void
dummy_release(void* dp)
{
    dummy_driver* const d = dp;

    free(d->buf);
}

static gboolean
dummy_open(void* dp)
{
    dummy_driver* const d = dp;
    struct timeval tv;

    /* Buffer for fake rendering necessary for working scopes, time events and so on */
    d->buf = calloc(mixer_get_resolution(format) << mixer_is_format_stereo(format), bufsize);

    gettimeofday(&tv, NULL);
    d->playtime = 0.0;
    d->prebuf = prebuffered;

    d->callback(d->buf, bufsize, rate, format);

    return TRUE;
}

static void
dummy_commit(void* dp)
{
    dummy_driver* const d = dp;
    struct timeval tv;
    const struct timespec ts = {0, 900000000LL * bufsize / rate};

    if (d->prebuf)
        d->prebuf--;
    else
        nanosleep(&ts, NULL);
    d->callback(d->buf, bufsize, rate, format);

    gettimeofday(&tv, NULL);
    d->outtime = tv.tv_sec + tv.tv_usec / 1e6;
    d->playtime += (double)bufsize / rate;
}

static double
dummy_get_play_time(void* dp)
{
    dummy_driver* const d = dp;
    struct timeval tv;
    double playtime;

    gettimeofday(&tv, NULL);
    playtime = d->playtime + tv.tv_sec + tv.tv_usec / 1e6 - d->outtime -
        (double)bufsize / rate;

    return playtime > 0.0 ? playtime : 0.0;
}

st_driver driver_out_dummy = {
    "No Output",

    dummy_new,
    dummy_destroy,

    dummy_open,
    dummy_release,

    dummy_getwidget,
    NULL,
    NULL,

    NULL,
    NULL,

    dummy_commit,

    dummy_get_play_time,
    NULL,
};

st_driver driver_in_dummy = {
    "No Input",

    dummy_new,
    dummy_destroy,

    dummy_open,
    dummy_release,

    dummy_getwidget,
    NULL,
    NULL,

    NULL,
    NULL,

    NULL,

    NULL,
    NULL,
};
