
/*
 * The Real SoundTracker - GTK+ Tracker widget font settings
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

#include <gtk/gtk.h>

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

#include "gui-settings.h"
#include "gui-subs.h"
#include "preferences.h"
#include "tracker-settings.h"

/* A global font list is kept here, so that when (at some time in the
   future) we have to deal with multiple Tracker widgets, this can be
   easily adapted. */

static GList* trackersettings_fontlist;

static void
trackersettings_gui_add_font(TrackerSettings* ts,
    gchar* fontname,
    int position)
{
    gchar* insertbuf;
    GtkListStore* list_store;
    GtkTreeIter iter;

    trackersettings_fontlist = g_list_insert(trackersettings_fontlist, fontname, position);

    insertbuf = fontname;

    list_store = GUI_GET_LIST_STORE(ts->list);
    gtk_list_store_insert(list_store, &iter, position);
    gtk_list_store_set(list_store, &iter, 0, insertbuf, -1);
}

static void
trackersettings_gui_delete_font(TrackerSettings* ts,
    int position)
{
    GtkTreeIter iter;
    GtkTreeModel* tree_model;
    GList* ll = g_list_nth(trackersettings_fontlist, position);

    g_free(ll->data);
    trackersettings_fontlist = g_list_remove_link(trackersettings_fontlist, ll);
    if (!gui_list_get_iter(position, tree_model = GUI_GET_TREE_MODEL(ts->list), &iter))
        return; /* Not found, sorry :-\ */
    gtk_list_store_remove(GTK_LIST_STORE(tree_model), &iter);
    ts->clist_selected_row = -1;
}

static void
trackersettings_gui_exchange_font(TrackerSettings* ts,
    int p1,
    int p2)
{
    GtkTreeIter iter1, iter2;
    GtkTreeModel* tree_model = GUI_GET_TREE_MODEL(ts->list);
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ts->list));

    if (!gui_list_get_iter(p1, tree_model, &iter1) || !gui_list_get_iter(p2, tree_model, &iter2))
        return;

    gtk_list_store_swap(GTK_LIST_STORE(tree_model), &iter1, &iter2);
    gtk_tree_selection_select_iter(sel, &iter1);
    ts->clist_selected_row = p2;
}

static guint
trackersettings_gui_populate_clist(TrackerSettings* ts)
{
    gchar* insertbuf;
    GList* l;
    GtkListStore* list_store;
    GtkTreeIter iter;
    guint i = 0, ret = 0;

    list_store = GUI_GET_LIST_STORE(ts->list);
    for (l = trackersettings_fontlist; l != NULL; l = l->next) {
        insertbuf = (gchar*)l->data;
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, insertbuf, -1);
        if (!strcmp(insertbuf, gui_settings.tracker_font))
            ret = i;
        i++;
    }
    return ret;
}

static void
trackersettings_clist_selected(GtkTreeSelection* sel,
    TrackerSettings* ts)
{
    gint row = gui_list_get_selection_index(sel);

    if (row != -1)
        ts->clist_selected_row = row;
}

static void
trackersettings_add_font(GtkWidget* widget,
    TrackerSettings* ts)
{
    gtk_widget_show(ts->fontsel_dialog);
}

static void
trackersettings_add_font_ok(GtkWidget* widget,
    TrackerSettings* ts)
{
    int row;

    gtk_widget_hide(ts->fontsel_dialog);

    if (gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(ts->fontsel_dialog))) {
        row = ts->clist_selected_row;
        if (row == -1) {
            row = 0;
        }

        trackersettings_gui_add_font(ts,
            gtk_font_selection_dialog_get_font_name(GTK_FONT_SELECTION_DIALOG(ts->fontsel_dialog)),
            row);
    }
}

static void
trackersettings_add_font_cancel(GtkWidget* widget,
    TrackerSettings* ts)
{
    gtk_widget_hide(ts->fontsel_dialog);
}

static void
trackersettings_delete_font(GtkWidget* widget,
    TrackerSettings* ts)
{
    int sel = ts->clist_selected_row;

    if (sel != -1) {
        trackersettings_gui_delete_font(ts, sel);
    }
}

void trackersettings_apply_font(TrackerSettings* ts)
{
    int sel = ts->clist_selected_row;

    if (sel != -1) {
        gchar* font = g_list_nth_data(trackersettings_fontlist, sel);
        tracker_set_font(ts->tracker, font);
        if (strcmp(gui_settings.tracker_font, font)) {
            g_free(gui_settings.tracker_font);
            gui_settings.tracker_font = g_strdup(font);
        }
    }
}

void trackersettings_cycle_font_forward(TrackerSettings* t)
{
    int len = g_list_length(trackersettings_fontlist);

    t->current_font++;
    t->current_font %= len;

    tracker_set_font(t->tracker, g_list_nth_data(trackersettings_fontlist, t->current_font));
}

void trackersettings_cycle_font_backward(TrackerSettings* t)
{
    int len = g_list_length(trackersettings_fontlist);

    t->current_font--;
    if (t->current_font < 0) {
        t->current_font = len - 1;
    }

    tracker_set_font(t->tracker, g_list_nth_data(trackersettings_fontlist, t->current_font));
}

static GList*
my_g_list_swap(GList* l,
    int p1,
    int p2)
{
    GList *l1, *l2;
    gpointer l1d, l2d;

    if (p1 > p2) {
        int p3 = p2;
        p2 = p1;
        p1 = p3;
    }

    l1 = g_list_nth(l, p1);
    l2 = g_list_nth(l, p2);
    l1d = l1->data;
    l2d = l2->data;

    l = g_list_remove_link(l, l1);
    l = g_list_remove_link(l, l2);
    l = g_list_insert(l, l2d, p1);
    l = g_list_insert(l, l1d, p2);

    return l;
}

static void
trackersettings_font_exchange(TrackerSettings* ts,
    int p1,
    int p2)
{
    trackersettings_fontlist = my_g_list_swap(trackersettings_fontlist, p1, p2);
    trackersettings_gui_exchange_font(ts, p1, p2);
}

static void
trackersettings_font_up(TrackerSettings* ts)
{
    if (ts->clist_selected_row > 0) {
        trackersettings_font_exchange(ts, ts->clist_selected_row, ts->clist_selected_row - 1);
    }
}

static void
trackersettings_font_down(TrackerSettings* ts)
{
    if (ts->clist_selected_row != -1 && ts->clist_selected_row < g_list_length(trackersettings_fontlist) - 1) {
        trackersettings_font_exchange(ts, ts->clist_selected_row, ts->clist_selected_row + 1);
    }
}

GtkWidget*
trackersettings_new(void)
{
    TrackerSettings* ts;
    const gchar* clisttitles[] = { N_("Font list") };
    GtkWidget *hbox1, *thing;
    guint selected;

    ts = g_object_new(trackersettings_get_type(), NULL);
    GTK_BOX(ts)->spacing = 2;
    GTK_BOX(ts)->homogeneous = FALSE;

    ts->clist_selected_row = -1;
    ts->current_font = 0;

    ts->list = gui_stringlist_in_scrolled_window(1, clisttitles, GTK_WIDGET(ts), TRUE);
    gui_list_handle_selection(ts->list,
        G_CALLBACK(trackersettings_clist_selected), ts);

    selected = trackersettings_gui_populate_clist(ts);
    gui_list_select(ts->list, selected, TRUE, 0.5);

    hbox1 = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(ts), hbox1, FALSE, TRUE, 0);
    gtk_widget_show(hbox1);

    thing = ts->add_button = gtk_button_new_with_label(_("Add font"));
    gtk_box_pack_start(GTK_BOX(hbox1), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(trackersettings_add_font), ts);

    thing = ts->delete_button = gtk_button_new_with_label(_("Delete font"));
    gtk_box_pack_start(GTK_BOX(hbox1), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(trackersettings_delete_font), ts);

    thing = ts->apply_button = gtk_button_new_with_label(_("Apply font"));
    gtk_box_pack_start(GTK_BOX(hbox1), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(trackersettings_apply_font), ts);

    hbox1 = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(ts), hbox1, FALSE, TRUE, 0);

    ts->up_button = gui_button(GTK_STOCK_GO_UP,
        G_CALLBACK(trackersettings_font_up), ts, hbox1, FALSE, TRUE);

    ts->down_button = gui_button(GTK_STOCK_GO_DOWN,
        G_CALLBACK(trackersettings_font_down), ts, hbox1, FALSE, TRUE);
    gtk_widget_show_all(hbox1);

    ts->fontsel_dialog = gtk_font_selection_dialog_new(_("Select font..."));
    gtk_window_set_modal(GTK_WINDOW(ts->fontsel_dialog), TRUE);
    g_signal_connect(GTK_FONT_SELECTION_DIALOG(ts->fontsel_dialog)->ok_button, "clicked",
        G_CALLBACK(trackersettings_add_font_ok), ts);
    g_signal_connect(GTK_FONT_SELECTION_DIALOG(ts->fontsel_dialog)->cancel_button, "clicked",
        G_CALLBACK(trackersettings_add_font_cancel), ts);
    g_signal_connect_swapped(ts->list, "row-activated", G_CALLBACK(trackersettings_apply_font), ts);

    return GTK_WIDGET(ts);
}

void trackersettings_set_tracker_widget(TrackerSettings* ts,
    Tracker* t)
{
    ts->tracker = t;

    tracker_set_font(t, g_list_nth_data(trackersettings_fontlist, ts->current_font));
}

static void
trackersettings_read_fontlist(void)
{
    gchar** fontlist; /* Don't g_strfreev() it because strings are used directly in the font list */
    gsize length, i;

    trackersettings_fontlist = NULL;

    fontlist = prefs_get_str_array("settings", "fonts", &length);
    for (i = 0; i < length; i++)
        trackersettings_fontlist = g_list_append(trackersettings_fontlist, fontlist[i]);

    if (!length)
        trackersettings_fontlist = g_list_append(trackersettings_fontlist, g_strdup("Monospace 10"));
    g_free(fontlist);
}

void trackersettings_write_settings(void)
{
    GList* l;
    GString* tmp = g_string_new("");

    /* It's simplier here to combine fonts string ourselves and put is as a single string rather than an array */
    for (l = trackersettings_fontlist; l != NULL; l = l->next) {
        g_string_append_printf(tmp, "%s;", (gchar*)l->data);
    }

    prefs_put_string("settings", "fonts", tmp->str);
    g_string_free(tmp, TRUE);
}

static void
trackersettings_init (TrackerSettings *self)
{
}

static void
trackersettings_class_init(TrackerSettingsClass* class)
{
    trackersettings_read_fontlist();
}

G_DEFINE_TYPE(TrackerSettings, trackersettings, GTK_TYPE_VBOX)
