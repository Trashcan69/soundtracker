/*
 * The Real SoundTracker - JACK (output) driver
 *
 * Copyright (C) 2003 Anthony Van Groningen
 * Copyright (C) 2014 Yury Aliaev
 * Copyright (C) 2019 Michael Krause
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

/*
 * TODO: 
 * need better declicking?
 * slave transport was removed
 * should master transport always work? even for pattern? Can we determine this info anyway?
 * general thread safety: d->state should be wrapped in state_mx locks as a matter of principle
 *                        In practice this is needed only when we are waiting on state_cv.
 * XRUN counter
 */

#include <config.h>

#if DRIVER_JACK

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <jack/jack.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "driver.h"
#include "errors.h"
#include "gui-subs.h"
#include "mixer.h"
#include "preferences.h"

/* suggested by Erik de Castro Lopo */
#define INT16_MAX_float 32767.0f
static inline gfloat
sample_convert_s16_to_float(gint16 inval)
{
    return inval * (1.0 / (INT16_MAX_float + 1));
}

static inline gint16
sample_convert_float_to_s16(gfloat inval)
{
    return lrintf(inval * INT16_MAX_float);
}

typedef jack_default_audio_sample_t audio_t;
typedef jack_nframes_t nframes_t;

/* For sampling driver only Rolling and Stopped states are valid */
typedef enum {
    JackDriverStateIsRolling,
    JackDriverStateIsDeclicking,
    JackDriverStateIsStopping, /* declicking is done, we want to transition to be stopped */
    JackDriverStateIsStopped
} jack_driver_state;

/*
typedef enum {
    JackDriverTransportIsInternal = 0,
    JackDriverTransportIsMaster = 1,
    //	JackDriverTransportIsSlave = 2
} jack_driver_transport;
*/

typedef enum {
    JACK_FLAG_SAMPLING = 1,
    JACK_FLAG_AUTOSTART = 1 << 1
} jack_flags;

typedef struct _jack_driver_group jack_driver_group;
typedef struct jack_driver {
    /* prefs stuff */
    GtkWidget* configwidget;
    GtkWidget* client_name_label;
    GtkWidget* status_label;
//    GtkWidget* transport_check;
    guint transport_check_id, autostart_check_id;
    GtkWidget* autostart_check;
    gint flags;
    gboolean is_active; /* jack seems to be running fine */
    jack_driver_group* group;
    GMutex process_mx; /* try to lock this around process_core() */
    GCond state_cv; /* trigger after declicking if we have the lock */
    jack_driver_state state; /* internal state stuff */

    /* jack + audio stuff */
    nframes_t buffer_size;
    unsigned long sample_rate;
    char* client_name;
    jack_client_t* client;
    jack_port_t *left, *right;
    void* mixbuf; /* passed to audio_mix, big enough for stereo 16 bit nframes = nframes*4 */
    gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat);
    STMixerFormat mf;

    nframes_t position, lastframes; /* frames since ST called open() and the last fragment size */
    gdouble outtime; /* Time of the last output operation */
//    jack_driver_transport transport; /* who do we serve? */
} jack_driver;

typedef struct {
    jack_driver driver; /* Must be at the first place */

    GtkWidget* declick_check;
    gboolean do_declick;

    gboolean locked; /* set true if we get it. then we can trigger any CV's during process_core() */
} jack_driver_playback;

typedef struct {
    jack_driver driver; /* Must be at the first place */

    GMutex sampling_mx;
    GThread* tid;
} jack_driver_sampling;

struct _jack_driver_group {
    jack_driver *sampling, *playback;
    const gchar* name;
};

static jack_driver_group *groups = NULL;
static guint jack_driver_ref = 0;
static guint num_groups = 0, num_allocated = 2;

static inline float
jack_driver_declick_coeff(nframes_t total, nframes_t current)
{
    /* total = # of frames it takes from 1.0 to 0.0
	   current = total ... 0 */
    return (float)current / (float)total;
}

static void
jack_driver_process_core(nframes_t nframes, jack_driver* d)
{
    audio_t *lbuf, *rbuf;
    struct timeval tv;
    gint16* mix = d->mixbuf;
    nframes_t cnt = nframes;
    float gain = 1.0f;
    jack_driver_playback *dd = (jack_driver_playback *)d;
    jack_driver_state state = d->state;

    lbuf = (audio_t*)jack_port_get_buffer(d->left, nframes);
    rbuf = (audio_t*)jack_port_get_buffer(d->right, nframes);

    switch (state) {

    case JackDriverStateIsRolling:
        d->callback(mix, nframes, d->sample_rate, d->mf);
        d->position += nframes;
        d->lastframes = nframes;
        gettimeofday(&tv, NULL);
        d->outtime = (gdouble)tv.tv_sec + (gdouble)tv.tv_usec * 1.0e-6;
        while (cnt--) {
            *(lbuf++) = sample_convert_s16_to_float(*mix++);
            *(rbuf++) = sample_convert_s16_to_float(*mix++);
        }
        break;

    case JackDriverStateIsDeclicking:
        d->callback(mix, (gint)nframes, (gint)d->sample_rate, (gint)d->mf);
        d->position += nframes;
        d->lastframes = nframes;
        gettimeofday(&tv, NULL);
        d->outtime = (gdouble)tv.tv_sec + (gdouble)tv.tv_usec * 1.0e-6;
        while (cnt--) {
            gain = jack_driver_declick_coeff(nframes, cnt);
            *(lbuf++) = gain * sample_convert_s16_to_float(*mix++);
            *(rbuf++) = gain * sample_convert_s16_to_float(*mix++);
        }
        /* safe because ST shouldn't call open() with pending release() */
        d->state = JackDriverStateIsStopping;
        break;

    case JackDriverStateIsStopping:
        /* if locked, then trigger change of state, otherwise keep silent */
        if (dd->locked) {
            d->state = JackDriverStateIsStopped;
            g_cond_signal(&d->state_cv);
        }
        /* fall down */

    case JackDriverStateIsStopped:
    default:
        memset(lbuf, 0, nframes * sizeof(audio_t));
        memset(rbuf, 0, nframes * sizeof(audio_t));
    }
}

static int
jack_driver_process_wrapper(nframes_t nframes, void* arg)
{
    jack_driver *d = arg, *rd, *pd;

    g_assert(d->group != NULL);

    pd = d->group->playback;
    rd = d->group->sampling;

    if (pd && pd->is_active) {
        jack_driver_playback* dd = (jack_driver_playback *)pd;

        if (g_mutex_trylock(&pd->process_mx)) {
            dd->locked = TRUE;
            jack_driver_process_core(nframes, pd);
            g_mutex_unlock(&pd->process_mx);
        } else {
            dd->locked = FALSE;
            jack_driver_process_core(nframes, pd);
        }
    }

    if (rd && rd->is_active && rd->state == JackDriverStateIsRolling) {
        jack_driver_sampling* dd = (jack_driver_sampling *)rd;
        nframes_t cnt = nframes;
        audio_t *lbuf, *rbuf;
        gint16* mix;

        if (!g_mutex_trylock(&rd->process_mx))
            /* The driver is being stopped, exiting immediately */
            return 0;

        lbuf = (audio_t*)jack_port_get_buffer(rd->left, nframes);
        rbuf = (audio_t*)jack_port_get_buffer(rd->right, nframes);
        rd->position += nframes;

        g_mutex_lock(&dd->sampling_mx);
        mix = rd->mixbuf;
        while (cnt--) {
            *(mix++) = sample_convert_float_to_s16(*lbuf++);
            *(mix++) = sample_convert_float_to_s16(*rbuf++);
        }
        g_mutex_unlock(&dd->sampling_mx);
        g_mutex_unlock(&rd->process_mx);
        g_cond_signal(&rd->state_cv);
    }

    return 0;
}

static void*
sampling_thread(void* arg)
{
    jack_driver* d = arg;
    jack_driver_sampling* sd = arg;

    do {
        g_mutex_lock(&sd->sampling_mx);
        g_cond_wait(&d->state_cv, &sd->sampling_mx);
        g_mutex_unlock(&sd->sampling_mx);
        if (!d->is_active)
            break;

        if (d->callback(d->mixbuf, d->buffer_size << 2, d->sample_rate, d->mf)) {
            void* buf = malloc(d->buffer_size << 2);

            g_mutex_lock(&sd->sampling_mx);
            d->mixbuf = buf;
            g_mutex_unlock(&sd->sampling_mx);
        }
    } while(d->is_active);

    return NULL;
}
/*
static void
jack_driver_prefs_transport_callback(void* a, jack_driver* d)
{ //!!! Revise
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d->transport_check))) {
        if (d->is_active) {
            d->transport = JackDriverTransportIsMaster;
            return;
        } else {
            // reset
            // gtk_signal_handler_block (GTK_OBJECT(d->transport_check), d->transport_check_id);
            // gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(d->transport_check), FALSE);
            // gtk_signal_handler_unblock (GTK_OBJECT(d->transport_check), d->transport_check_id);
            return;
        }
    } else {
        d->transport = JackDriverTransportIsInternal;
    }
}
*/

static void
jack_driver_prefs_declick_callback(GtkToggleButton* widget, jack_driver_playback* d)
{
    d->do_declick = gtk_toggle_button_get_active(widget);
}

static int
jack_driver_sample_rate_callback(nframes_t nframes, void* arg)
{
    jack_driver *d = arg, *cp;

    g_assert(d->group != NULL);

    d->sample_rate = nframes;
    cp = (d->flags & JACK_FLAG_SAMPLING) ? d->group->playback : d->group->sampling;
    if (cp)
        cp->sample_rate = nframes;

    return 0;
}

static int
jack_driver_buffer_size_callback(nframes_t nframes, void* arg)
{
    jack_driver *d = arg, *cp;

    g_assert(d->group != NULL);

    if (nframes > d->buffer_size) {
        d->buffer_size = nframes;
        d->mixbuf = realloc(d->mixbuf, d->buffer_size << 2);
    }

    /* Counterpart, if exists */
    cp = (d->flags & JACK_FLAG_SAMPLING) ? d->group->playback : d->group->sampling;
    if (cp)
        if (nframes > cp->buffer_size) {
            cp->buffer_size = nframes;
            cp->mixbuf = realloc(cp->mixbuf, cp->buffer_size << 2);
        }

    return 0;
}

static void
jack_driver_prefs_update(jack_driver* d)
{
    gchar* status_buf;

    if (d->is_active) {
        status_buf = g_strdup_printf(_("Running at %d Hz with %d frames"), (int)d->sample_rate, (int)d->buffer_size);
        gtk_label_set_text(GTK_LABEL(d->client_name_label), d->client_name);
        gtk_label_set_text(GTK_LABEL(d->status_label), status_buf);
    } else {
        status_buf = g_strdup_printf(_("Jack server not running?"));
        gtk_label_set_text(GTK_LABEL(d->status_label), "");
        gtk_label_set_text(GTK_LABEL(d->client_name_label), status_buf);
    }
    g_free(status_buf);
}

static void
jack_driver_server_has_shutdown(void* arg)
{
    jack_driver *d = arg, *cp;

    g_assert(d->group != NULL);

    d->is_active = FALSE;
    d->client = NULL;
    jack_driver_ref = 0;
    jack_driver_prefs_update(d);

    /* Counterpart, if exists */
    cp = (d->flags & JACK_FLAG_SAMPLING) ? d->group->playback : d->group->sampling;
    if (cp) {
        cp->is_active = FALSE;
        cp->client = NULL;
        jack_driver_prefs_update(cp);
    }
}

static void
jack_driver_error(const char* msg)
{
    gchar* buf;
    buf = g_strdup_printf(_("Jack driver error:\n%s"), msg);
    error_error(buf);
    g_free(buf);
}

static void
jack_driver_activate_do(jack_driver* d)
{
    jack_status_t status;

    g_assert(d->group != NULL);

    if (!d->is_active) {
        gboolean just_opened = FALSE, r_c = FALSE, p_c = FALSE;

        if (d->group->sampling)
            r_c = (d->group->sampling->client != NULL);
        if (d->group->playback)
            p_c = (d->group->playback->client != NULL);
        /* We open a client only if no client for the group is opened */
        if (!r_c && !p_c) {
            jack_options_t options = (d->flags & JACK_FLAG_AUTOSTART) ? JackNullOption : JackNoStartServer;
            d->client = jack_client_open("soundtracker", options, &status, NULL);
            just_opened = TRUE;
        } else /* Looking for the counterpart's client */
            d->client = (d->flags & JACK_FLAG_SAMPLING) ?
                d->group->playback->client : d->group->sampling->client;

        if (d->client == NULL) {
            /* we've failed here, but we should have a working dummy driver
               because ST will segfault on NULL return */
            return;
        }

        d->client_name = jack_get_client_name(d->client);
        d->sample_rate = jack_get_sample_rate(d->client);
        d->buffer_size = jack_get_buffer_size(d->client);
        if (!d->mixbuf)
            d->mixbuf = malloc(d->buffer_size << 2);

        d->left = jack_port_register(d->client,
            (d->flags & JACK_FLAG_SAMPLING) ? "in_1" : "out_1",
            JACK_DEFAULT_AUDIO_TYPE,
            (d->flags & JACK_FLAG_SAMPLING) ? JackPortIsInput : JackPortIsOutput, 0);
        d->right = jack_port_register(d->client,
            (d->flags & JACK_FLAG_SAMPLING) ? "in_2" : "out_2",
            JACK_DEFAULT_AUDIO_TYPE,
            (d->flags & JACK_FLAG_SAMPLING) ? JackPortIsInput : JackPortIsOutput, 0);

        if (just_opened) {
            jack_set_process_callback(d->client, jack_driver_process_wrapper, d);
            jack_set_sample_rate_callback(d->client, jack_driver_sample_rate_callback, d);
            jack_set_buffer_size_callback(d->client, jack_driver_buffer_size_callback, d);
            jack_on_shutdown(d->client, jack_driver_server_has_shutdown, d);

            if (jack_activate(d->client)) {
                static GtkWidget* dialog = NULL;

                d->is_active = FALSE;
                gui_error_dialog(&dialog, _("Jack driver activation failed."), FALSE);
            } else {
                d->is_active = TRUE;

                if (jack_driver_ref == 0)
                    jack_set_error_function(jack_driver_error);
                jack_driver_ref++;
            }
        } else
            d->is_active = (d->flags & JACK_FLAG_SAMPLING) ?
                d->group->playback->is_active : d->group->sampling->is_active;
    }
    jack_driver_prefs_update(d);
}

static void
jack_driver_activate(void* dp, const gchar* group)
{
    guint i;
    jack_driver_group *cur_grp = NULL;

    jack_driver* d = dp;

    if (!groups)
        groups = g_new0(jack_driver_group, num_allocated);
    for (i = 0; i < num_groups; i++)
        if (!strcmp(groups[i].name, group)) {
            cur_grp = &groups[i];
            break;
        }
    /* No group with the specified name, creating a new one */
    if (!cur_grp) {
        /* Shoud be "==", but I'm a bit paranoiac $-) */
        if (num_groups >= num_allocated) {
            num_allocated *= 2;
            groups = g_renew(jack_driver_group, groups, num_allocated);
            memset(&groups[num_groups], 0, (num_allocated - num_groups) * sizeof(jack_driver_group));
        }
        cur_grp = &groups[num_groups++];
        cur_grp->name = group; /* The rest fields are already NULL */
    }
    d->group = cur_grp;
    if (d->flags & JACK_FLAG_SAMPLING) {
        cur_grp->sampling = d;
        ((jack_driver_sampling *)d)->tid = g_thread_new("Sampling", sampling_thread, dp);
    } else
        cur_grp->playback = d;

    jack_driver_activate_do(d);
}

static void
jack_driver_prefs_autostart_callback(GtkToggleButton* widget, jack_driver* d)
{
    gboolean autostart = gtk_toggle_button_get_active(widget);
    d->flags = autostart ? d->flags | JACK_FLAG_AUTOSTART : d->flags & ~JACK_FLAG_AUTOSTART;

    if (autostart && (!d->is_active))
        jack_driver_activate_do(d);
}

static void
jack_driver_make_config_widgets(jack_driver* d)
{
    GtkWidget *thing, *mainbox, *hbox;

    d->configwidget = mainbox = gtk_vbox_new(FALSE, 2);

    d->client_name_label = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    d->status_label = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Update"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    g_signal_connect_swapped(thing, "clicked", G_CALLBACK(jack_driver_activate_do), d);
    gtk_widget_show(thing);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = d->autostart_check = gtk_check_button_new_with_label(_("Jack autostart"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    d->autostart_check_id = g_signal_connect(thing, "clicked", G_CALLBACK(jack_driver_prefs_autostart_callback), d);
    gtk_widget_show(thing);
/*
    thing = d->transport_check = gtk_check_button_new_with_label(_("transport master"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    d->transport_check_id = g_signal_connect(thing, "clicked", G_CALLBACK(jack_driver_prefs_transport_callback), d);
    gtk_widget_show(thing);
*/
    if (!(d->flags & JACK_FLAG_SAMPLING)) {
        thing = ((jack_driver_playback *)d)->declick_check = gtk_check_button_new_with_label(_("declick"));
        gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
        g_signal_connect(thing, "clicked", G_CALLBACK(jack_driver_prefs_declick_callback), d);
    }
    gtk_widget_show(thing);
}

static void
jack_driver_init(jack_driver* d)
{
#ifdef WORDS_BIGENDIAN
    d->mf = ST_MIXER_FORMAT_S16_BE | ST_MIXER_FORMAT_STEREO;
#else
    d->mf = ST_MIXER_FORMAT_S16_LE | ST_MIXER_FORMAT_STEREO;
#endif
    d->state = JackDriverStateIsStopped;
    g_mutex_init(&d->process_mx);
    g_cond_init(&d->state_cv);
    jack_driver_make_config_widgets(d);
}

static void*
jack_driver_new_out(gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat))
{
    jack_driver_playback* d = g_new0(jack_driver_playback, 1);

//    d->transport = JackDriverTransportIsInternal;
    ((jack_driver *)d)->callback = callback;
    jack_driver_init((jack_driver *)d);
    return d;
}

static void*
jack_driver_new_in(gboolean (*callback)(void *buf, guint32 count, gint mixfreq, gint mixformat))
{
    jack_driver_sampling* d = g_new0(jack_driver_sampling, 1);

    ((jack_driver *)d)->flags |= JACK_FLAG_SAMPLING;
    ((jack_driver *)d)->callback = callback;
    g_mutex_init(&d->sampling_mx);
    jack_driver_init((jack_driver *)d);
    return d;
}

static gboolean
jack_driver_open(void* dp)
{
    jack_driver* d = dp;

    if (!d->is_active) {
        error_warning(_("Jack server is not running or some error occured."));
        return FALSE;
    }
    d->position = 0;
    d->lastframes = 0;
    d->outtime = 0.0;
    d->state = JackDriverStateIsRolling;

    return TRUE;
}

static void
jack_driver_release(void* dp)
{
    jack_driver* d = dp;

    g_mutex_lock(&d->process_mx); /* Aviod concurrency with processing callback */
    if (d->flags & JACK_FLAG_SAMPLING) {
        d->state = JackDriverStateIsStopped;
    } else {
        jack_driver_playback* dd = dp;

        if (dd->do_declick) {
            d->state = JackDriverStateIsDeclicking;
        } else {
            d->state = JackDriverStateIsStopping;
        }
        g_cond_wait(&d->state_cv, &d->process_mx);
        /* at this point process() has set state to stopped */
    }
    g_mutex_unlock(&d->process_mx);
}

static void
jack_driver_deactivate(void* dp)
{
    jack_driver* d = dp;
    if (d->is_active) {
        jack_client_t* old_client = d->client;
        gboolean r_c = FALSE, p_c = FALSE;

        d->is_active = FALSE;
        d->client = NULL;
        if (d->flags & JACK_FLAG_SAMPLING) {
            g_cond_signal(&d->state_cv);
            g_thread_join(((jack_driver_sampling *)d)->tid);
        }
        jack_port_unregister(old_client, d->left);
        jack_port_unregister(old_client, d->right);

        if (d->group->sampling)
            r_c = (d->group->sampling->client != NULL);
        if (d->group->playback)
            p_c = (d->group->playback->client != NULL);
        /* No active drivers, close the client */
        if (!r_c && !p_c) {
            jack_driver_ref--;
            if (jack_driver_ref == 0)
                jack_set_error_function(NULL);

            jack_client_close(old_client);
            jack_driver_prefs_update(d);
        }
    }
}

static void
jack_driver_destroy(void* dp)
{
    jack_driver* d = dp;

    gtk_widget_destroy(d->configwidget);
    if (d->mixbuf != NULL) {
        free(d->mixbuf);
        d->mixbuf = NULL;
    }
    g_mutex_clear(&d->process_mx);
    if (d->flags & JACK_FLAG_SAMPLING)
        g_mutex_clear(&((jack_driver_sampling *)d)->sampling_mx);
    g_cond_clear(&d->state_cv);
    g_free(d);
}

static GtkWidget*
jack_driver_getwidget(void* dp)
{
    jack_driver* d = dp;
    jack_driver_prefs_update(d);
    return d->configwidget;
}

//!!! Transport master?
static gboolean
jack_driver_loadsettings(void* dp, const gchar* f)
{
    jack_driver* d = dp;

    if (!(d->flags & JACK_FLAG_SAMPLING)) {
        jack_driver_playback* dd = dp;

        dd->do_declick = prefs_get_bool(f, "jack-declick", TRUE);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->declick_check), dd->do_declick);
    }

    d->flags = prefs_get_bool(f, "jack-autostart", FALSE) ?
        d->flags | JACK_FLAG_AUTOSTART : d->flags & ~JACK_FLAG_AUTOSTART;
    /* To prevent Jack server preliminary starting */
    g_signal_handler_block(G_OBJECT(d->autostart_check), d->autostart_check_id);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d->autostart_check), d->flags & JACK_FLAG_AUTOSTART);
    g_signal_handler_unblock(G_OBJECT(d->autostart_check), d->autostart_check_id);

    return TRUE;
}

static gboolean
jack_driver_savesettings(void* dp, const gchar* f)
{
    jack_driver* d = dp;

    if (!(d->flags & JACK_FLAG_SAMPLING))
        prefs_put_bool(f, "jack-declick", ((jack_driver_playback *)d)->do_declick);

    prefs_put_bool(f, "jack-autostart", (d->flags & JACK_FLAG_AUTOSTART) != 0);
    return TRUE;
}

static double
jack_driver_get_play_time(void* dp)
{
    jack_driver* const d = dp;
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return (double)(d->position - d->lastframes) / (double)d->sample_rate +
        (gdouble)tv.tv_sec + (gdouble)tv.tv_usec * 1.0e-6 - d->outtime;
}

static inline int
jack_driver_get_play_rate(void* d)
{
    jack_driver* const dp = d;
    return (int)dp->sample_rate;
}

st_driver driver_out_jack = {
    "JACK Output",
    jack_driver_new_out, /* create new instance of this driver class */
    jack_driver_destroy, /* destroy instance of this driver class */
    jack_driver_open, /* open the driver */
    jack_driver_release, /* close the driver, release audio */
    jack_driver_getwidget, /* get pointer to configuration widget */
    jack_driver_loadsettings, /* load configuration from provided preferences section */
    jack_driver_savesettings, /* save configuration to specified preferences section */
    jack_driver_activate, /* create ports, run client and optionally the server if needed */
    jack_driver_deactivate, /* close ports, close the client if no ports exist anymore */
    NULL, /* No need to commit data arrival */
    jack_driver_get_play_time, /* get time offset since first sound output */
    jack_driver_get_play_rate /* get current play rate */
};

st_driver driver_in_jack = {
    "JACK Input",
    jack_driver_new_in,
    jack_driver_destroy,
    jack_driver_open,
    jack_driver_release,
    jack_driver_getwidget,
    jack_driver_loadsettings,
    jack_driver_savesettings,
    jack_driver_activate,
    jack_driver_deactivate,
    NULL,
    jack_driver_get_play_time,
    jack_driver_get_play_rate
};

#endif /* DRIVER_JACK */
