
/*
 * The Real SoundTracker - Audio configuration dialog
 *
 * Copyright (C) 1999-2019 Michael Krause
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

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include "audio-subs.h"
#include "audio.h"
#include "audioconfig.h"
#include "driver.h"
#include "gui-subs.h"
#include "gui.h"
#include "mixer.h"
#include "preferences.h"
#include "sample-editor.h"
#include "st-subs.h"

/* List of available output and input drivers in this compilation of
   SoundTracker. */

GList* drivers[2] = { NULL, NULL };
GList* mixers = NULL;
GtkWidget* configwindow = NULL;

static GtkWidget* audioconfig_mixer_list;
static st_mixer* audioconfig_current_mixer = NULL;
static gboolean audioconfig_disable_mixer_selection = FALSE;

typedef struct audio_object {
    const char* title;
    const char* shorttitle;
    const char* group; /* Currently is used for sharing the same Jack port */
    int type;
    void** driver_object;
    st_driver** driver;
    GtkWidget* drivernbook;
} audio_object;

static audio_object audio_objects[] = {
    { N_("Playback Output"),
        "playback",
        "",
        DRIVER_OUTPUT,
        &playback_driver_object,
        &playback_driver,
        NULL },
    { N_("Editing Output"),
        "editing",
        "editing",
        DRIVER_OUTPUT,
        &editing_driver_object,
        &editing_driver,
        NULL },
    { N_("Sampling"),
        "sampling",
        "",
        DRIVER_INPUT,
        &sampling_driver_object,
        &sampling_driver,
        NULL }
};

#define NUM_AUDIO_OBJECTS ARRAY_SIZE(audio_objects)

static void*** audio_driver_objects[NUM_AUDIO_OBJECTS];

static void
audioconfig_list_select(GtkTreeSelection* sel, gpointer data)
{
    gint row = gui_list_get_selection_index(sel);
    gint page = GPOINTER_TO_INT(data);

    if (row != -1) {
        audio_object* object = &audio_objects[page];
        st_driver* old_driver = *object->driver;
        st_driver* new_driver = g_list_nth_data(drivers[object->type], row);

        if (new_driver != old_driver) {
            // stop playing and sampling here
            sample_editor_stop_sampling();
            gui_play_stop();

            // get new driver object
            if (old_driver->deactivate)
                old_driver->deactivate(*object->driver_object);
            *object->driver_object = audio_driver_objects[page][row];
            *object->driver = new_driver;
            if (new_driver->activate)
                new_driver->activate(*object->driver_object, object->group);
        }
        gtk_notebook_set_current_page(GTK_NOTEBOOK(object->drivernbook), row);
    }
}

static void
audioconfig_mixer_selected(GtkTreeSelection* sel)
{
    gint row = gui_list_get_selection_index(sel);

    if (row != -1) {
        st_mixer* new_mixer = g_list_nth_data(mixers, row);

        if (!audioconfig_disable_mixer_selection && new_mixer != audioconfig_current_mixer) {
            audio_set_mixer(new_mixer);
            audioconfig_current_mixer = new_mixer;
        }
    }
}

static void
audioconfig_initialize_mixer_list(void)
{
    GList* l;
    int i, active = -1;
    GtkListStore* list_store = GUI_GET_LIST_STORE(audioconfig_mixer_list);
    GtkTreeIter iter;
    GtkTreeModel* model;

    audioconfig_disable_mixer_selection = TRUE;
    model = gui_list_freeze(audioconfig_mixer_list);
    gui_list_clear_with_model(model);
    for (i = 0, l = mixers; l; i++, l = l->next) {
        st_mixer* _mixer = l->data;
        if (_mixer == audioconfig_current_mixer) {
            active = i;
        }
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, (gchar*)_mixer->id,
            1, gettext((gchar*)_mixer->description), -1);
    }
    gui_list_thaw(audioconfig_mixer_list, model);
    audioconfig_disable_mixer_selection = FALSE;

    gui_list_select(audioconfig_mixer_list, active, FALSE, 0.0);
}

static void
audioconfig_notebook_add_page(GtkNotebook* nbook, guint n)
{
    GtkWidget *label, *box1, *list, *widget, *dnbook, *alignment;
    GList* l;
    gint i, active = -1;
    const gchar* listtitles[1] = {N_("Driver Module")};
    GtkListStore* list_store;
    GtkTreeIter iter;

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(box1), 4);

    /* Driver selection list */
    list = gui_stringlist_in_scrolled_window(1, listtitles, box1, TRUE);
    gtk_widget_set_size_request(list, 200, -1);
    list_store = GUI_GET_LIST_STORE(list);

    /* Driver configuration widgets' notebook (with hidden tabs, as a multi-layer container) */
    audio_objects[n].drivernbook = dnbook = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(dnbook), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(dnbook), FALSE);
    gtk_box_pack_start(GTK_BOX(box1), dnbook, TRUE, TRUE, 0);

    for (i = 0, l = drivers[audio_objects[n].type]; l; i++, l = l->next) {
        st_driver* driver = l->data;
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, *((gchar**)l->data), -1);

        if (driver == *audio_objects[n].driver)
            active = i;

        widget = driver->getwidget(audio_driver_objects[n][i]);
        alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_container_add(GTK_CONTAINER(alignment), widget);
        gtk_notebook_append_page(GTK_NOTEBOOK(dnbook), alignment, NULL);
        gtk_widget_show_all(alignment);
    }

    gui_list_handle_selection(list, G_CALLBACK(audioconfig_list_select), GINT_TO_POINTER(n));
    if (active != -1)
        gui_list_select(list, active, FALSE, 0.0);
    else
        gui_list_select(list, 0, FALSE, 0.0);

    label = gtk_label_new(gettext(audio_objects[n].title));
    gtk_widget_show(label);

    gtk_notebook_append_page(nbook, box1, label);
}

void audioconfig_dialog(void)
{
    GtkWidget *mainbox, *thing, *nbook, *box2, *frame;
    const gchar* listtitles2[2] = {N_("Mixer Module"), N_("Description")};
    int i;

    if (configwindow != NULL) {
        gtk_window_present(GTK_WINDOW(configwindow));
        return;
    }

    configwindow = gtk_dialog_new_with_buttons(_("Audio Configuration"), GTK_WINDOW(mainwindow), 0,
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    gui_dialog_connect(configwindow, NULL);

    mainbox = gtk_dialog_get_content_area(GTK_DIALOG(configwindow));
    gui_dialog_adjust(configwindow, GTK_RESPONSE_CLOSE);

    // Each driver (playback,capture,editing,etc...) occupies the notebook page
    nbook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(mainbox), nbook, FALSE, TRUE, 0);
    for (i = 0; i < NUM_AUDIO_OBJECTS; i++) {
        audioconfig_notebook_add_page(GTK_NOTEBOOK(nbook), i);
    }

    // Mixer selection
    frame = gtk_frame_new(NULL);
    gtk_frame_set_label(GTK_FRAME(frame), _("Mixers"));
    gtk_box_pack_start(GTK_BOX(mainbox), frame, TRUE, TRUE, 0);

    box2 = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(frame), box2);
    gtk_container_set_border_width(GTK_CONTAINER(box2), 4);

    thing = gui_stringlist_in_scrolled_window(2, listtitles2, box2, TRUE);
    gui_list_handle_selection(thing, G_CALLBACK(audioconfig_mixer_selected), NULL);
    audioconfig_mixer_list = thing;
    audioconfig_initialize_mixer_list();

    gtk_widget_show_all(configwindow);
}

void audioconfig_load_config(void)
{
    gchar* buf;
    GList* l;
    guint i, n[NUM_AUDIO_OBJECTS];

    memset(n, 0, sizeof(n));
    for (i = 0; i < NUM_AUDIO_OBJECTS; i++) {
        guint j;

        if ((buf = prefs_get_string("audio-objects", audio_objects[i].shorttitle, NULL))) {
            for (j = 0, l = drivers[audio_objects[i].type]; l; l = l->next, j++) {
                if (!strcmp(*((gchar**)l->data), buf)) {
                    *audio_objects[i].driver = l->data;
                    n[i] = j;
                    break;
                }
            }
            g_free(buf);
        }
    }

    for (i = 0; i < NUM_AUDIO_OBJECTS; i++) {
        guint j;
        gchar* buf;
        st_driver* d = *audio_objects[i].driver;

        buf = g_strdup_printf("audio-object-%s", audio_objects[i].shorttitle);
        audio_driver_objects[i] = g_new(void**,
            g_list_length(drivers[audio_objects[i].type]));
        for (j = 0, l = drivers[audio_objects[i].type]; l; j++, l = l->next) {
            st_driver* driver = l->data;
            // create driver instance
            void* a_d_o = audio_driver_objects[i][j] =
                driver->new(audio_objects[i].type == DRIVER_OUTPUT ?
                audio_request_data : sample_editor_sampled);

            if (driver->loadsettings)
                driver->loadsettings(a_d_o, buf);
        }
        g_free(buf);

        if (!d) {
            // set the first driver as current if none has been configured
            if (drivers[audio_objects[i].type] != NULL) {
                d = *audio_objects[i].driver = drivers[audio_objects[i].type]->data;
            }
        }

        // set the current driver's object and activate it if required
        if (d) {
            *audio_objects[i].driver_object = audio_driver_objects[i][n[i]];
            if (d->activate)
                d->activate(audio_driver_objects[i][n[i]], audio_objects[i].group);
        }
    }
}

void audioconfig_load_mixer_config(void)
{
    gchar* buf;
    GList* l;

    if ((buf = prefs_get_string("mixer", "mixer", NULL))) {
        for (l = mixers; l; l = l->next) {
            st_mixer* m = l->data;
            if (!strcmp(m->id, buf)) {
                mixer = m;
                audioconfig_current_mixer = m;
            }
        }
        g_free(buf);
    }

    if (!audioconfig_current_mixer) {
        mixer = mixers->data;
        audioconfig_current_mixer = mixers->data;
    }
}

void audioconfig_save_config(void)
{
    guint i, j;
    GList* l;

    // Write the driver module names
    for (i = 0; i < NUM_AUDIO_OBJECTS; i++) {
        prefs_put_string("audio-objects", audio_objects[i].shorttitle,
            (*audio_objects[i].driver)->name);
    }

    /* Write the driver module's configurations */
    for (i = 0; i < NUM_AUDIO_OBJECTS; i++) {
        gchar* buf = g_strdup_printf("audio-object-%s", audio_objects[i].shorttitle);

        for (j = 0, l = drivers[audio_objects[i].type]; l; l = l->next, j++) {
            st_driver* driver = l->data;
            gboolean (*savesettings)(void*, const gchar*) = driver->savesettings;

            if (savesettings)
                savesettings(audio_driver_objects[i][j], buf);
        }
        g_free(buf);
    }

    prefs_put_string("mixer", "mixer", audioconfig_current_mixer->id);
}

void audioconfig_shutdown(void)
{
    GList* l;
    guint i;

    if (playback_driver->deactivate)
        playback_driver->deactivate(playback_driver_object);
    if (editing_driver->deactivate)
        editing_driver->deactivate(editing_driver_object);
    if (sampling_driver->deactivate)
        sampling_driver->deactivate(sampling_driver_object);

    for (i = 0; i < NUM_AUDIO_OBJECTS; i++) {
        guint j;

        for (j = 0, l = drivers[audio_objects[i].type]; l; j++, l = l->next) {
            st_driver* driver = l->data;
            driver->destroy(audio_driver_objects[i][j]);
        }
    }
}
