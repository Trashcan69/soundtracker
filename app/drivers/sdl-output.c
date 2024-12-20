
/*
 * The Real SoundTracker - SDL output driver.
 *
 * Copyright (C) 2004 Markku Reunanen
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

#if DRIVER_SDL

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <SDL.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "audio-subs.h"
#include "driver.h"
#include "errors.h"
#include "gui-subs.h"
#include "mixer.h"
#include "preferences.h"

#define SDL_BUFSIZE 1024

typedef struct sdl_driver {
    GtkWidget* configwidget;

    int out_bits, out_channels, out_rate;
    int mf;
    gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat);

    double outtime;
    double playtime;
} sdl_driver;

void sdl_callback(void* udata, Uint8* stream, int len)
{
    sdl_driver* const d = udata;
    struct timeval tv;

    len >>= 2;
    d->callback(stream, len, d->out_rate, d->mf);

    gettimeofday(&tv, NULL);
    d->outtime = tv.tv_sec + tv.tv_usec / 1e6;
    d->playtime += (double)len / d->out_rate;

    SDL_Delay(1);
}

static void
sdl_make_config_widgets(sdl_driver* d)
{
    GtkWidget *thing, *mainbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);

    thing = gtk_label_new(_("Experimental SDL support."));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
}

static GtkWidget*
sdl_getwidget(void* dp)
{
    sdl_driver* const d = dp;

    return d->configwidget;
}

static void*
sdl_new(gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat))
{
    sdl_driver* d = g_new(sdl_driver, 1);

    d->out_bits = AUDIO_S16SYS;
    d->out_rate = 44100;
    d->out_channels = 2;
    d->callback = callback;

    sdl_make_config_widgets(d);

    return d;
}

static void
sdl_destroy(void* dp)
{
    sdl_driver* const d = dp;

    gtk_widget_destroy(d->configwidget);

    g_free(dp);
}

static void
sdl_release(void* dp)
{
    SDL_PauseAudio(1);
    SDL_Quit();
}

static gboolean
sdl_open(void* dp)
{
    sdl_driver* const d = dp;
    SDL_AudioSpec spec;

    SDL_Init(SDL_INIT_AUDIO);
    spec.freq = d->out_rate;
    spec.format = d->out_bits;
    spec.channels = d->out_channels;
    spec.samples = SDL_BUFSIZE;
    spec.callback = sdl_callback;
    spec.userdata = dp;
    SDL_OpenAudio(&spec, NULL);

#ifdef WORDS_BIGENDIAN
    d->mf = ST_MIXER_FORMAT_S16_BE;
#else
    d->mf = ST_MIXER_FORMAT_S16_LE;
#endif
    d->mf |= ST_MIXER_FORMAT_STEREO;

    d->playtime = 0.0;

    SDL_PauseAudio(0);
    return TRUE;
}

static double
sdl_get_play_time(void* dp)
{
    sdl_driver* const d = dp;
    struct timeval tv;
    double curtime;

    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec + tv.tv_usec / 1e6;

    return d->playtime + curtime - d->outtime - (double)SDL_BUFSIZE / d->out_rate * 2.0;
}

static int
sdl_get_play_rate(void* dp)
{
    sdl_driver* const d = dp;

    return d->out_rate;
}

static gboolean
sdl_loadsettings(void* dp,
    const gchar* f)
{
    return TRUE;
}

static gboolean
sdl_savesettings(void* dp,
    const gchar* f)
{
    return TRUE;
}

st_driver driver_out_sdl = {
    "SDL Output",

    sdl_new,
    sdl_destroy,

    sdl_open,
    sdl_release,

    sdl_getwidget,
    sdl_loadsettings,
    sdl_savesettings,

    NULL,
    NULL,

    NULL,

    sdl_get_play_time,
    sdl_get_play_rate
};

#endif /* DRIVER_SDL */
