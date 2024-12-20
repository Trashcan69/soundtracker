
/*
 * The Real SoundTracker - GUI configuration dialog
 *
 * Copyright (C) 1999-2019 Michael Krause
 * Copyright (C) 2020-2024 Yury Aliaev
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
#include <gtk/gtk.h>
#include <string.h>

#include "colors.h"
#include "extspinbutton.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "gui.h"
#include "instrument-editor.h"
#include "loop-factory.h"
#include "preferences.h"
#include "sample-editor.h"
#include "scope-group.h"
#include "track-editor.h"
#include "tracker-settings.h"

#define SECTION "settings"
#define SECTION_ALWAYS "settings-always"

gui_prefs gui_settings;

extern ScopeGroup* scopegroup;

static GtkWidget *vol_sym_check, *tone_porta_m_check, *vol_dec_check;

static void
prefs_scopesfreq_changed(GtkSpinButton *spin)
{
    gui_settings.scopes_update_freq = gtk_spin_button_get_value_as_int(spin);
    gui_set_channel_status_update_freq(gui_settings.scopes_update_freq);
}

static void
prefs_trackerfreq_changed(GtkSpinButton *spin)
{
    gui_settings.tracker_update_freq = gtk_spin_button_get_value_as_int(spin);
    tracker_set_update_freq(gui_settings.tracker_update_freq);
}

static void
gui_settings_double_changed(GtkSpinButton *spin, gdouble* val)
{
    *val = gtk_spin_button_get_value(spin);
}

static void
gui_settings_int_changed(GtkSpinButton *spin, gint* val)
{
    *val = gtk_spin_button_get_value_as_int(spin);
}

static void
gui_settings_asyncedit_toggled(GtkWidget* widget)
{
    gui_play_stop();
    gui_settings.asynchronous_editing = GTK_TOGGLE_BUTTON(widget)->active;
}

static void
gui_settings_trypoly_toggled(GtkWidget* widget, GtkWidget* repeat)
{
    gui_play_stop();
    gui_settings.try_polyphony = GTK_TOGGLE_BUTTON(widget)->active;
    gtk_widget_set_sensitive(repeat, gui_settings.try_polyphony);
}

static void
gui_settings_misc_toggled(GtkWidget* widget, gboolean* var)
{
    *var = GTK_TOGGLE_BUTTON(widget)->active;
}

static void
gui_settings_misc_toggled_inv(GtkWidget* widget, gboolean* var)
{
    *var = !GTK_TOGGLE_BUTTON(widget)->active;
}

static void
gui_settings_redraw_toggled(GtkWidget* widget, gboolean* var)
{
    *var = GTK_TOGGLE_BUTTON(widget)->active;
    tracker_redraw(tracker);
}

static void
gui_settings_ft2vol_toggled(GtkWidget* widget, gboolean* var)
{
    gboolean is_active = GTK_TOGGLE_BUTTON(widget)->active;

    *var = is_active;
    gtk_widget_set_sensitive(vol_sym_check, is_active);
    gtk_widget_set_sensitive(tone_porta_m_check, is_active);
    gtk_widget_set_sensitive(vol_dec_check, is_active);
    tracker_redraw(tracker);
}

static void
gui_settings_volsym_toggled(GtkWidget* widget, gboolean* var)
{
    gboolean is_active = GTK_TOGGLE_BUTTON(widget)->active;

    *var = is_active;
    gtk_widget_set_sensitive(tone_porta_m_check, is_active);
    tracker_redraw(tracker);
}

static void
gui_settings_scopebufsize_changed(GtkSpinButton* spin)
{
    double n = gtk_spin_button_get_value(spin);

    gui_settings.scopes_buffer_size = n * 1000000;
}

static void
gui_settings_clavierfont_changed(GtkFontButton* fb)
{
    gui_settings.clavier_font = gtk_font_button_get_font_name(fb);
    instrument_editor_update_clavier(gui_settings.clavier_font);
}

void gui_settings_highlight_rows_changed(GtkSpinButton* spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    gui_settings.highlight_rows_n = n;
    if (gui_settings.highlight_rows)
        tracker_redraw(tracker);
}

void gui_settings_highlight_rows_minor_changed(GtkSpinButton* spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    gui_settings.highlight_rows_minor_n = n;
    if (gui_settings.highlight_rows)
        tracker_redraw(tracker);
}

static void
gui_settings_tracker_line_note_modified(GtkEntry* entry)
{
    gchar* text = g_strdup(gtk_entry_get_text(entry));
    int i;

    for (i = 0; i < 3; i++) {
        if (!text[i]) {
            text[i] = ' ';
            text[i + 1] = 0;
        }
    }
    text[3] = 0;
    if (strncmp(gui_settings.tracker_line_format, text, 3)) {
        strncpy(gui_settings.tracker_line_format, text, 3);
        tracker_redraw(tracker);
    }
    g_free(text);
}

static void
gui_settings_tracker_line_ins_modified(GtkEntry* entry)
{
    gchar* text = g_strdup(gtk_entry_get_text(entry));
    int i;

    for (i = 0; i < 2; i++) {
        if (!text[i]) {
            text[i] = ' ';
            text[i + 1] = 0;
        }
    }
    text[2] = 0;
    if (strncmp(gui_settings.tracker_line_format + 3, text, 2)) {
        strncpy(gui_settings.tracker_line_format + 3, text, 2);
        tracker_redraw(tracker);
    }
    g_free(text);
}

static void
gui_settings_tracker_line_vol_modified(GtkEntry* entry)
{
    gchar* text = g_strdup(gtk_entry_get_text(entry));
    int i;

    for (i = 0; i < 2; i++) {
        if (!text[i]) {
            text[i] = ' ';
            text[i + 1] = 0;
        }
    }
    text[2] = 0;
    if (strncmp(gui_settings.tracker_line_format + 5, text, 2)) {
        strncpy(gui_settings.tracker_line_format + 5, text, 2);
        tracker_redraw(tracker);
    }
    g_free(text);
}

static void
gui_settings_tracker_line_effect_modified(GtkEntry* entry, GdkEventKey* event)
{
    gchar* text = g_strdup(gtk_entry_get_text(entry));
    int i;

    for (i = 0; i < 3; i++) {
        if (!text[i]) {
            text[i] = ' ';
            text[i + 1] = 0;
        }
    }
    text[3] = 0;
    if (strncmp(gui_settings.tracker_line_format + 7, text, 3)) {
        strncpy(gui_settings.tracker_line_format + 7, text, 3);
        tracker_redraw(tracker);
    }
    g_free(text);
}

static void
gui_settings_set_scopes_mode(GtkToggleButton* button, gpointer data)
{
    if (gtk_toggle_button_get_active(button)) {
        SampleDisplayMode i = GPOINTER_TO_INT(data);

        gui_settings.scopes_mode = i;
        scope_group_set_mode(scopegroup, i);
    }
}

static void
gui_settings_set_selection_mode(GtkComboBox* combo)
{
    gui_settings.selection_mode = gtk_combo_box_get_active(combo);
}

static void
gui_settings_set_editor_mode(GtkToggleButton* button, gpointer data)
{
    if (gtk_toggle_button_get_active(button)) {
        SampleDisplayMode i = GPOINTER_TO_INT(data);

        gui_settings.editor_mode = i;
        sample_editor_set_mode(SAMPLE_EDITOR_DISPLAY_EDITOR, i);
        loop_factory_set_mode(i);
    }
}

static void
gui_settings_set_sampling_mode(GtkToggleButton* button, gpointer data)
{
    if (gtk_toggle_button_get_active(button)) {
        SampleDisplayMode i = GPOINTER_TO_INT(data);

        gui_settings.sampling_mode = i;
        sample_editor_set_mode(SAMPLE_EDITOR_DISPLAY_SAMPLING, i);
    }
}

static void
modes_radio_create(GtkWidget* table,
    GtkWidget** array,
    const guint column,
    void (*sigfunc)(GtkToggleButton*, gpointer),
    SampleDisplayMode mode)
{
    guint i;
    GtkWidget* thing;

    for (i = 0; i < SAMPLE_DISPLAY_MODE_LAST; i++) {
        GtkWidget* alignment;

        alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        thing = array[i] =
            gtk_radio_button_new(i == 0 ? NULL : gtk_radio_button_get_group(GTK_RADIO_BUTTON(thing)));
        gtk_container_add(GTK_CONTAINER(alignment), thing);
        gtk_table_attach_defaults(GTK_TABLE(table), alignment, column, column + 1, i + 1, i + 2);
        if (i == mode)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), TRUE);
        g_signal_connect(thing, "clicked", G_CALLBACK(sigfunc), GINT_TO_POINTER(i));
    }
}

static void
fonts_dialog(GtkWidget* window)
{
    static GtkWidget* dialog = NULL;
    GtkWidget *mainbox, *thing;

    if (!dialog) {
        gchar* buf;

        buf = g_strdup_printf(_("%s fonts configuration"), PACKAGE_NAME);
        dialog = gtk_dialog_new_with_buttons(buf, GTK_WINDOW(window),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
        g_free(buf);
        gui_dialog_connect(dialog, NULL);
        mainbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

        /* The tracker widget settings */
        g_object_ref(G_OBJECT(trackersettings));
        gtk_box_pack_start(GTK_BOX(mainbox), trackersettings, TRUE, TRUE, 0);

        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 4);

        gtk_widget_show_all(dialog);
    } else
        gtk_window_present(GTK_WINDOW(dialog));
}

#define TYPE_PATHS_DIALOG (paths_dialog_get_type())
#define PATHS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_PATHS_DIALOG, PathsDialog))

typedef struct _PathsDialog PathsDialog;
typedef struct _PathsDialogClass PathsDialogClass;

struct _PathsDialog {
    GtkDialog dialog;
    GtkWidget *list, *entry;
    GtkTreeSelection* sel;
    GtkTreeModel* model;
    StrArray *paths;
};

struct _PathsDialogClass {
    GtkDialogClass parent_class;
};

static void
paths_dialog_init(PathsDialog* d)
{
};

static void
paths_dialog_class_init(PathsDialogClass* c)
{
};

G_DEFINE_TYPE(PathsDialog, paths_dialog, GTK_TYPE_DIALOG);

static GtkTreeModel*
paths_list_populate(GtkWidget* list,
    StrArray* paths)
{
    gint i;
    GtkTreeIter iter;
    GtkTreeModel* mdl = gui_list_freeze(list);

    gui_list_clear_with_model(mdl);
    for (i = 0; i < paths->num; i++) {
        gtk_list_store_append(GTK_LIST_STORE(mdl), &iter);
        gtk_list_store_set(GTK_LIST_STORE(mdl), &iter, 0, paths->lines[i], -1);
    }

    gui_list_thaw(list, mdl);
    return mdl;
}

static gchar*
path_selection_run(PathsDialog* d)
{
    static GtkWidget *file_dialog = NULL, *fc;
    gint resp;

    if (!file_dialog) {
        GtkWidget* vbox;

        file_dialog = gtk_dialog_new_with_buttons(_("Folder selection"), GTK_WINDOW(d),
            GTK_DIALOG_MODAL, GTK_STOCK_OK, GTK_RESPONSE_OK,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
        g_object_set(file_dialog, "has-separator", TRUE, NULL);

        fc = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        gui_set_list_size(fc, 40, 40);
        vbox = gtk_dialog_get_content_area(GTK_DIALOG(file_dialog));
        gtk_box_set_spacing(GTK_BOX(vbox), 4);
        gtk_box_pack_start(GTK_BOX(vbox), fc, TRUE, TRUE, 0);
        gtk_widget_show_all(vbox);
    }

    resp = gtk_dialog_run(GTK_DIALOG(file_dialog));
    gtk_widget_hide(file_dialog);

    if (resp == GTK_RESPONSE_OK) {
        gchar* name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
        gchar* utf_name = gui_filename_to_utf8(name);

        g_free(name);
        return utf_name;
    } else
        return NULL;
}

static void
add_path(PathsDialog* d)
{
    gchar* path = path_selection_run(d);

    if (path) {
        gint row = gui_list_get_selection_index(d->sel);
        gint i, newnum = d->paths->num + 1;

        if (row == -1)
            row = 0;
        if (newnum > d->paths->num_allocated) {
            d->paths->num_allocated = newnum;
            d->paths->lines = g_renew(gchar*, d->paths->lines, newnum);
        }
        for (i = d->paths->num; i > row; i--)
            d->paths->lines[i] = d->paths->lines[i - 1];
        d->paths->lines[row] = path;

        d->paths->num = newnum;
        paths_list_populate(d->list, d->paths);
    }
}

static void
del_path(PathsDialog* d)
{
    gint row = gui_list_get_selection_index(d->sel);

    if (row != -1 && d->paths->num) {
        gint i, newnum = --d->paths->num;

        for (i = row; i < newnum; i++)
            d->paths->lines[i] = d->paths->lines[i + 1];
        paths_list_populate(d->list, d->paths);
    }
}

static void
path_entry_activate(PathsDialog* d)
{
    gint row = gui_list_get_selection_index(d->sel);

    if (row != -1) {
        GtkTreeIter iter;

        g_free(d->paths->lines[row]);
        d->paths->lines[row] = g_strdup(gtk_entry_get_text(GTK_ENTRY(d->entry)));
        if (!gui_list_get_iter(row, d->model, &iter))
            return; /* Some bullshit happens :-/ */
        gtk_list_store_set(GTK_LIST_STORE(d->model), &iter, 0, d->paths->lines[row], -1);
    }
}

static void
sel_path(PathsDialog* d)
{
    gchar* path = path_selection_run(d);

    if (path) {
        gtk_entry_set_text(GTK_ENTRY(d->entry), path);
        g_free(path);
        path_entry_activate(d);
    }
}

static void
paths_list_selected(GtkTreeSelection* sel,
    PathsDialog* d)
{
    gint row = gui_list_get_selection_index(sel);

    if (row != -1)
        gtk_entry_set_text(GTK_ENTRY(d->entry), d->paths->lines[row]);
}

static void
path_up(PathsDialog* d)
{
    gint row = gui_list_get_selection_index(d->sel);

    if (row > 0) {
        GtkTreeIter iter;
        gchar* tmp = d->paths->lines[row];

        d->paths->lines[row] = d->paths->lines[row - 1];
        d->paths->lines[row - 1] = tmp;
        if (!gui_list_get_iter(row, d->model, &iter))
            return;
        gtk_list_store_set(GTK_LIST_STORE(d->model), &iter, 0, d->paths->lines[row], -1);
        if (!gui_list_get_iter(row - 1, d->model, &iter))
            return;
        gtk_list_store_set(GTK_LIST_STORE(d->model), &iter, 0, d->paths->lines[row - 1], -1);
    }
}

static void
path_down(PathsDialog* d)
{
    gint row = gui_list_get_selection_index(d->sel);

    if (row != -1 && row < d->paths->num - 1) {
        GtkTreeIter iter;
        gchar* tmp = d->paths->lines[row];

        d->paths->lines[row] = d->paths->lines[row + 1];
        d->paths->lines[row + 1] = tmp;
        if (!gui_list_get_iter(row, d->model, &iter))
            return;
        gtk_list_store_set(GTK_LIST_STORE(d->model), &iter, 0, d->paths->lines[row], -1);
        if (!gui_list_get_iter(row + 1, d->model, &iter))
            return;
        gtk_list_store_set(GTK_LIST_STORE(d->model), &iter, 0, d->paths->lines[row + 1], -1);
    }
}

static void
paths_dialog_show(GtkWidget* window, StrArray *paths)
{
    static PathsDialog *dialog;
    const gchar* listtitles[1] = {N_("Paths")};

    if (!dialog) {
        GtkWidget *mainbox, *thing, *box;

        dialog = PATHS_DIALOG(g_object_new(paths_dialog_get_type(), "title", _("Sample Editor extensions folders"),
            "has-separator", TRUE, "modal", TRUE, "transient-for", GTK_WINDOW(window),
            "destroy-with-parent", TRUE, NULL));
        gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
        gui_dialog_connect(GTK_WIDGET(dialog), NULL);
        mainbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        gtk_box_set_spacing(GTK_BOX(mainbox), 2);
        dialog->paths = paths;

        dialog->list = gui_stringlist_in_scrolled_window(1, listtitles, mainbox, TRUE);
        gui_set_list_size(dialog->list, 30, 10);
        dialog->model = paths_list_populate(dialog->list, paths);
        dialog->sel = gui_list_handle_selection(dialog->list, G_CALLBACK(paths_list_selected), dialog);

        box = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box, FALSE, FALSE, 0);

        thing = gui_button(GTK_STOCK_ADD, G_CALLBACK(add_path), dialog, box, TRUE, FALSE);
        gtk_widget_set_tooltip_text(thing, _("Add folder"));
        thing = gui_button(GTK_STOCK_REMOVE, G_CALLBACK(del_path), dialog, box, TRUE, FALSE);
        gtk_widget_set_tooltip_text(thing, _("Delete folder"));
        thing = gui_button(GTK_STOCK_GO_UP, G_CALLBACK(path_up), dialog, box, TRUE, FALSE);
        gtk_widget_set_tooltip_text(thing, _("Move folder up"));
        thing = gui_button(GTK_STOCK_GO_DOWN, G_CALLBACK(path_down), dialog, box, TRUE, FALSE);
        gtk_widget_set_tooltip_text(thing, _("Move folder down"));

        dialog->entry = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(box), dialog->entry, TRUE, TRUE, 0);
        g_signal_connect_swapped(dialog->entry, "activate",
            G_CALLBACK(path_entry_activate), dialog);

        thing = gui_button(GTK_STOCK_OK, G_CALLBACK(path_entry_activate), dialog, box, TRUE, FALSE);
        gtk_widget_set_tooltip_text(thing, _("Apply changes"));
        thing = gui_button(GTK_STOCK_DIRECTORY, G_CALLBACK(sel_path), dialog, box, TRUE, FALSE);
        gtk_widget_set_tooltip_text(thing, _("Choose folder"));

        gtk_widget_show_all(mainbox);
    }

    gtk_dialog_run(GTK_DIALOG(dialog));
}

static void
s_e_ext_dialog_show(GtkWidget* w)
{
    paths_dialog_show(w, &gui_settings.se_extension_path);
}

void gui_settings_dialog(void)
{
    static GtkWidget* configwindow = NULL;
    static GtkWidget *scopes_radio[SAMPLE_DISPLAY_MODE_LAST],
                     *editor_radio[SAMPLE_DISPLAY_MODE_LAST],
                     *sampling_radio[SAMPLE_DISPLAY_MODE_LAST];

    static gchar *sel_mode[] = {N_("Classic ST"), N_("FT2"), N_("Mixed")};

    GtkWidget *mainbox, *sw, *thing, *thing1, *box1, *vbox1, *spin, *table;
    GtkListStore* list_store;
    GtkTreeIter iter;
    GtkAllocation all;
    PangoContext *context;
    PangoLayout *layout;
    gchar stmp[5], *buf;
    gint wdth, hght;
    guint i;

    if (configwindow != NULL) {
        gtk_window_present(GTK_WINDOW(configwindow));
        return;
    }

    configwindow = gtk_dialog_new_with_buttons(_("GUI Configuration"), GTK_WINDOW(mainwindow), 0,
        GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    gui_dialog_connect(configwindow, NULL);
    gui_dialog_adjust(configwindow, GTK_RESPONSE_CLOSE);
    g_object_set(configwindow, "has-separator", TRUE, NULL);
    mainbox = gtk_dialog_get_content_area(GTK_DIALOG(configwindow));

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(mainbox), sw, TRUE, TRUE, 0);

    vbox1 = gtk_vbox_new(FALSE, 2);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(sw), vbox1);
    gtk_viewport_set_shadow_type(GTK_VIEWPORT(gtk_bin_get_child(GTK_BIN(sw))), GTK_SHADOW_NONE);

    thing = gui_labelled_spin_button_new(_("Tracker Frequency"), 1, 80,
        &spin, prefs_trackerfreq_changed, NULL, FALSE, NULL);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), gui_settings.tracker_update_freq);

    thing = gui_labelled_spin_button_new(_("Scopes Frequency"), 1, 80,
        &spin, prefs_scopesfreq_changed, NULL, FALSE, NULL);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), gui_settings.scopes_update_freq);

    thing = gtk_label_new(_("Sample displays' mode"));
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    table = gtk_table_new(3, 4, TRUE);
    thing = gtk_label_new(_("Scopes"));
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 1, 2, 0, 1);
    thing = gtk_label_new(_("Smpl. Ed."));
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 2, 3, 0, 1);
    thing = gtk_label_new(_("Sampling"));
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 3, 4, 0, 1);
    thing = gtk_label_new(_("Strobo"));
    gtk_widget_set_tooltip_text(thing,
        _("Fast, but not so much accurate method for waveforms' drawing"));
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 1, 2);
    thing = gtk_label_new(_("Minmax"));
    gtk_widget_set_tooltip_text(thing,
        _("More realistic waveform drawing method with higher CPU load"));
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 2, 3);

    modes_radio_create(table, scopes_radio, 1, gui_settings_set_scopes_mode, gui_settings.scopes_mode);
    modes_radio_create(table, editor_radio, 2, gui_settings_set_editor_mode, gui_settings.editor_mode);
    modes_radio_create(table, sampling_radio, 3, gui_settings_set_sampling_mode, gui_settings.sampling_mode);

    gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, TRUE, 0);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox1), box1, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Scopes buffer size [MB]"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, FALSE, 0);

    thing = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new((double)gui_settings.scopes_buffer_size / 1000000, 0.5, 5.0, 0.1, 1.0, 0.0)), 0, 0, FALSE);
    gtk_box_pack_end(GTK_BOX(box1), thing, FALSE, FALSE, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(thing), 1);
    g_signal_connect(thing, "value-changed",
        G_CALLBACK(gui_settings_scopebufsize_changed), NULL);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);

    thing = gtk_check_button_new_with_label(_("Hexadecimal row numbers"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.tracker_hexmode);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_redraw_toggled), &gui_settings.tracker_hexmode);

    thing = gtk_check_button_new_with_label(_("Use upper case letters for hex numbers"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.tracker_upcase);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_redraw_toggled), &gui_settings.tracker_upcase);

    thing = gtk_check_button_new_with_label(_("Use note name B instead of H"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.bh);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_redraw_toggled), &gui_settings.bh);

    thing = gtk_check_button_new_with_label(_("FT2-like volume column"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.tracker_ft2_volume);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_ft2vol_toggled), &gui_settings.tracker_ft2_volume);

    vol_sym_check = thing = gtk_check_button_new_with_label(_("Use symbols in the volume column"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.tracker_ft2_wide);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_volsym_toggled), &gui_settings.tracker_ft2_wide);

    tone_porta_m_check = thing = gtk_check_button_new_with_label(_("Leave Tone Porta as symbol \"m\""));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.tracker_ft2_tpm);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_redraw_toggled), &gui_settings.tracker_ft2_tpm);

    vol_dec_check = thing = gtk_check_button_new_with_label(_("Decimal volume representation"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.tracker_vol_dec);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_redraw_toggled), &gui_settings.tracker_vol_dec);

    thing = gtk_check_button_new_with_label(_("Asynchronous (IT-style) pattern editing"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.asynchronous_editing);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_asyncedit_toggled), NULL);

    thing = gtk_check_button_new_with_label(_("Change pattern length when inserting / deleting line"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.change_patlen);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.change_patlen);

    thing = gtk_check_button_new_with_label(_("Polyphonic try (non-editing) mode"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.try_polyphony);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);

    thing1 = gtk_check_button_new_with_label(_("Repeat same note on different channels"));
    gtk_widget_set_sensitive(thing1, gui_settings.try_polyphony);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing1), gui_settings.repeat_same_note);
    gtk_box_pack_start(GTK_BOX(vbox1), thing1, FALSE, TRUE, 0);
    g_signal_connect(thing1, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.repeat_same_note);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_trypoly_toggled), thing1);

    thing = gtk_check_button_new_with_label(_("Record keyreleases"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.insert_noteoff);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.insert_noteoff);

    thing = gtk_check_button_new_with_label(_("Record precise timings"));
    gtk_widget_set_tooltip_text(thing,
        _("Use FX to record note press/release timings with tick accuracy"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.precise_timing);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.precise_timing);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox1), box1, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Human-made delay compensation [s]"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, FALSE, 0);

    thing = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(gui_settings.delay_comp,
        0.0, 1.0, 0.01, 0.1, 0.0)), 0, 0, FALSE);
    gtk_box_pack_end(GTK_BOX(box1), thing, FALSE, FALSE, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(thing), 2);
    g_signal_connect(thing, "value-changed",
        G_CALLBACK(gui_settings_double_changed), &gui_settings.delay_comp);

    thing = gtk_check_button_new_with_label(_("Pressing and releasing Control opens Instrument Selector"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.ctrl_show_isel);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.ctrl_show_isel);

    thing = gtk_check_button_new_with_label(_("Fxx command updates Tempo/BPM sliders"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.tempo_bpm_update);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.tempo_bpm_update);

    thing = gtk_check_button_new_with_label(_("Emulate FastTracker Rxx bug"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.rxx_bug_emu);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.rxx_bug_emu);

    thing = gtk_check_button_new_with_label(_("Use filter effect (Qxx/Zxx)"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.use_filter);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.use_filter);

    thing = gtk_check_button_new_with_label(_("Automatically add file extensions"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.add_extension);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.add_extension);

    thing = gtk_check_button_new_with_label(_("Switch to tracker after loading/saving"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.auto_switch);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.auto_switch);

    thing = gtk_check_button_new_with_label(_("Check compatibility with FT2 before saving"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.check_xm_compat);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.check_xm_compat);

    thing = gtk_check_button_new_with_label(_("Preserve instrument name"));
    gtk_widget_set_tooltip_text(thing,
        _("Preserve old instrumen name on new instrument loading and instrument cleaning"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), !gui_settings.ins_name_overwrite);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled_inv), &gui_settings.ins_name_overwrite);

    thing = gtk_check_button_new_with_label(_("Preserve sample name(s)"));
    gtk_widget_set_tooltip_text(thing,
        _("Preserve old sample name(s) on new instrument / sample loading and instrument cleaning"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), !gui_settings.smp_name_overwrite);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled_inv), &gui_settings.smp_name_overwrite);

    thing = gtk_check_button_new_with_label(_("Save window geometry on exit"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.save_geometry);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.save_geometry);

    thing = gtk_check_button_new_with_label(_("Save and restore permanent channels"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.store_perm);
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);
    g_signal_connect(G_OBJECT(thing), "toggled",
        G_CALLBACK(gui_settings_misc_toggled), &gui_settings.store_perm);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox1), box1, FALSE, TRUE, 0);

    thing = gtk_label_new(_("* Undo pool size [MB]"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, FALSE, 0);

    thing = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(gui_settings.undo_size,
        1.0, 100.0, 1.0, 10.0, 0.0)), 0, 0, FALSE);
    gtk_box_pack_end(GTK_BOX(box1), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "value-changed",
        G_CALLBACK(gui_settings_int_changed), &gui_settings.undo_size);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox1), box1, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(box1,
        _("Classis ST: only Ctrl + B to start/stop marking a block;\n"
          "FT2: marking by SHIFT + arrows;\n"
          "Mixed: marking is started by SHIFT + arrows,\nstopped by Ctrl + B.\n"
          "In the FT2 and Mixed modes Ctrl + B\ncan also be used to start marking."));

    thing = gtk_label_new(_("Selection mode"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, FALSE, 0);

    list_store = gtk_list_store_new(1, G_TYPE_STRING);
    for (i = 0; i < SELECTION_LAST; i++) {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, _(sel_mode[i]), -1);
    }
    thing = gui_combo_new(list_store);
    gtk_combo_box_set_active(GTK_COMBO_BOX(thing), gui_settings.selection_mode);
    gtk_box_pack_end(GTK_BOX(box1), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "changed", G_CALLBACK(gui_settings_set_selection_mode), NULL);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), thing, FALSE, TRUE, 0);

    /* Track line format */
    box1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox1), box1, FALSE, TRUE, 0);
    thing = gtk_label_new(_("Track line format:"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);

    thing = gtk_entry_new();

    context = gtk_widget_create_pango_context(thing);
    layout = pango_layout_new(context);
    pango_layout_set_font_description(layout, gtk_widget_get_style(thing)->font_desc);
    /* With an extra symbol the width value becomes more appropriate */
    pango_layout_set_text(layout, "----", -1);
    pango_layout_get_pixel_size(layout, &wdth, &hght);
    gtk_widget_set_size_request(thing, wdth, -1);

    gtk_entry_set_max_length((GtkEntry*)thing, 3);
    strncpy(stmp, gui_settings.tracker_line_format, 3);
    stmp[3] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "changed",
        G_CALLBACK(gui_settings_tracker_line_note_modified), 0);

    thing = gtk_entry_new();
    pango_layout_set_text(layout, "000", -1);
    pango_layout_get_pixel_size(layout, &wdth, &hght);
    gtk_widget_set_size_request(thing, wdth, -1);
    gtk_entry_set_max_length((GtkEntry*)thing, 2);
    strncpy(stmp, gui_settings.tracker_line_format + 3, 2);
    stmp[2] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "changed",
        G_CALLBACK(gui_settings_tracker_line_ins_modified), 0);

    thing = gtk_entry_new();
    pango_layout_set_text(layout, "000", -1);
    pango_layout_get_pixel_size(layout, &wdth, &hght);
    gtk_widget_set_size_request(thing, wdth, -1);
    gtk_entry_set_max_length((GtkEntry*)thing, 2);
    strncpy(stmp, gui_settings.tracker_line_format + 5, 2);
    stmp[2] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "changed",
        G_CALLBACK(gui_settings_tracker_line_vol_modified), 0);

    thing = gtk_entry_new();
    pango_layout_set_text(layout, "0000", -1);
    pango_layout_get_pixel_size(layout, &wdth, &hght);
    gtk_widget_set_size_request(thing, wdth, -1);
    g_object_unref(layout);
    g_object_unref(context);

    gtk_entry_set_max_length((GtkEntry*)thing, 3);
    strncpy(stmp, gui_settings.tracker_line_format + 7, 3);
    stmp[3] = 0;
    gtk_entry_set_text((GtkEntry*)thing, stmp);
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "changed",
        G_CALLBACK(gui_settings_tracker_line_effect_modified), 0);

    thing1 = gtk_hbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(vbox1), thing1, FALSE, TRUE, 0);
    /* Tracker colors configuration dialog */
    thing = gtk_button_new_with_label(_("Color scheme"));
    gtk_widget_set_tooltip_text(thing, _("Tracker colors configuration"));
    gtk_box_pack_start(GTK_BOX(thing1), thing, TRUE, TRUE, 0);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(colors_dialog), configwindow);

    /* Tracker fonts configuration dialog */
    thing = gtk_button_new_with_label(_("Tracker fonts"));
    gtk_widget_set_tooltip_text(thing, _("Tracker fonts configuration"));
    gtk_box_pack_start(GTK_BOX(thing1), thing, TRUE, TRUE, 0);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(fonts_dialog), configwindow);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox1), box1, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Clavier font"));
    gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, FALSE, 0);

    thing = gtk_font_button_new_with_font(gui_settings.clavier_font);
    gtk_box_pack_end(GTK_BOX(box1), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "font-set",
        G_CALLBACK(gui_settings_clavierfont_changed), NULL);

    thing1 = gtk_hbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(vbox1), thing1, FALSE, TRUE, 0);

    thing = gtk_button_new_with_label(_("Paths and folders"));
    gtk_box_pack_start(GTK_BOX(thing1), thing, FALSE, FALSE, 0);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(s_e_ext_dialog_show), configwindow);

    buf = g_strdup_printf(_("* Restart %s to actualize settings marked with asterisk"), PACKAGE_NAME);
    thing = gtk_label_new(buf);
    g_free(buf);
    gtk_misc_set_alignment(GTK_MISC(thing), 0.0, 0.5);
    gtk_box_pack_end(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

    gtk_widget_show_all(configwindow);
    /* Some compromise to find optimal settings window size */
    gtk_widget_get_allocation(vbox1, &all);
    hght = gdk_screen_get_height(gdk_screen_get_default());
    gtk_widget_set_size_request(sw, -1,
        MIN(all.height, hght - 100));
    gtk_window_set_position(GTK_WINDOW(configwindow), GTK_WIN_POS_CENTER_ALWAYS);
}

void gui_settings_load_config(void)
{
    gui_settings.st_window_x = prefs_get_int(SECTION, "st-window-x", -666);
    gui_settings.st_window_y = prefs_get_int(SECTION, "st-window-y", 0);
    gui_settings.st_window_w = prefs_get_int(SECTION, "st-window-w", 0);
    gui_settings.st_window_h = prefs_get_int(SECTION, "st-window-h", 0);

    gui_settings.tracker_hexmode = prefs_get_bool(SECTION, "gui-use-hexadecimal-numbers", TRUE);
    gui_settings.tracker_upcase = prefs_get_bool(SECTION, "gui-use-upper-case", FALSE);
    gui_settings.tracker_ft2_volume = prefs_get_bool(SECTION, "gui-ft2-volume", FALSE);
    gui_settings.tracker_ft2_wide = prefs_get_bool(SECTION, "gui-ft2-wide", TRUE);
    gui_settings.tracker_ft2_tpm = prefs_get_bool(SECTION, "gui-ft2-tone-porta-m", FALSE);
    gui_settings.tracker_vol_dec = prefs_get_bool(SECTION, "gui-volume-decimal", FALSE);
    gui_settings.advance_cursor_in_fx_columns = prefs_get_bool(SECTION, "gui-advance-cursor-in-fx-columns", FALSE);
    gui_settings.asynchronous_editing = prefs_get_bool(SECTION, "gui-asynchronous-editing", FALSE);
    gui_settings.change_patlen = prefs_get_bool(SECTION, "gui-change-pattern-length", FALSE);
    gui_settings.try_polyphony = prefs_get_bool(SECTION, "gui-try-polyphony", TRUE);
    gui_settings.repeat_same_note = prefs_get_bool(SECTION, "gui-repeat-same-note", FALSE);
    gui_settings.insert_noteoff = prefs_get_bool(SECTION, "gui-insert-noteoff", TRUE);
    gui_settings.precise_timing = prefs_get_bool(SECTION, "gui-precise-timing", FALSE);
    gui_settings.ctrl_show_isel = prefs_get_bool(SECTION, "gui-ctrl-show-instr-sel", FALSE);
    gui_settings.delay_comp = prefs_get_double(SECTION, "gui-delay-compensation", 0.0);
    gui_settings.looped = prefs_get_bool(SECTION, "gui-playback-looped", TRUE);
    gui_settings.tracker_line_format = prefs_get_string(SECTION, "gui-tracker-line-format", "---0000000");
    gui_settings.tracker_font = prefs_get_string(SECTION, "tracker-font", "");
    gui_settings.tempo_bpm_update = prefs_get_bool(SECTION, "gui-tempo-bpm-update", TRUE);
    gui_settings.rxx_bug_emu = prefs_get_bool(SECTION, "tracker-rxx-bug-emulate", FALSE);
    gui_settings.use_filter = prefs_get_bool(SECTION, "player-use-filter", TRUE);
    gui_settings.auto_switch = prefs_get_bool(SECTION, "gui-auto-switch", FALSE);
    gui_settings.add_extension = prefs_get_bool(SECTION, "gui-add-extension", TRUE);
    gui_settings.check_xm_compat = prefs_get_bool(SECTION, "gui-check-compat", FALSE);
    gui_settings.ins_name_overwrite = prefs_get_bool(SECTION, "gui-ins-name-overwrite", TRUE);
    gui_settings.smp_name_overwrite = prefs_get_bool(SECTION, "gui-smp-name-overwrite", TRUE);
    gui_settings.gui_display_scopes = prefs_get_bool(SECTION, "gui-display-scopes", TRUE);
    gui_settings.gui_use_backing_store = prefs_get_bool(SECTION, "gui-use-backing-store", TRUE);
    gui_settings.undo_size = prefs_get_int(SECTION, "gui-undo-size", 20);
    gui_settings.highlight_rows = prefs_get_bool(SECTION, "tracker-highlight-rows", TRUE);
    gui_settings.highlight_rows_n = prefs_get_int(SECTION, "tracker-highlight-rows-n", 16);
    gui_settings.highlight_rows_minor_n = prefs_get_int(SECTION, "tracker-highlight-rows-minor-n", 8);
    gui_settings.clavier_colors_gtk = prefs_get_bool(SECTION, "clavier-colors-gtk", TRUE);
    gui_settings.clavier_font = prefs_get_string(SECTION, "clavier-font", "Monospace 8");

    gui_settings.save_geometry = prefs_get_bool(SECTION, "save-geometry", TRUE);
    gui_settings.save_settings_on_exit = prefs_get_bool(SECTION, "save-settings-on-exit", TRUE);
    gui_settings.tracker_update_freq = prefs_get_int(SECTION, "tracker-update-frequency", 50);
    gui_settings.scopes_update_freq = prefs_get_int(SECTION, "scopes-update-frequency", 40);
    gui_settings.scopes_buffer_size = prefs_get_int(SECTION, "scopes-buffer-size", 500000);
    gui_settings.show_ins_smp = prefs_get_bool(SECTION, "scopes-show-ins-smp", FALSE);
    gui_settings.sharp = prefs_get_bool(SECTION, "sharp", TRUE);
    gui_settings.bh = prefs_get_bool(SECTION, "bh", FALSE);
    /* Masking disabled, all columns for copy / paste, transparency off */
    gui_settings.cpt_mask = prefs_get_int(SECTION, "cpt-mask", 0x3ff);
    gui_settings.store_perm = prefs_get_bool(SECTION, "store-permanent", TRUE);
    gui_settings.scopes_mode = prefs_get_int(SECTION, "sample-display-scopes-mode", SAMPLE_DISPLAY_MODE_STROBO);
    gui_settings.editor_mode = prefs_get_int(SECTION, "sample-display-editor-mode", SAMPLE_DISPLAY_MODE_STROBO);
    gui_settings.sampling_mode = prefs_get_int(SECTION, "sample-display-sampling-mode", SAMPLE_DISPLAY_MODE_STROBO);
    gui_settings.selection_mode = prefs_get_int(SECTION, "selection-mode", SELECTION_CLASSIC);
    gui_settings.sample_play_loop = prefs_get_bool(SECTION, "sample-play-loop", FALSE);

    if (gui_settings.store_perm)
        gui_settings.permanent_channels = prefs_get_int(SECTION, "permanent-channels", 0);

    gui_settings.file_out_channels = prefs_get_int(SECTION, "file-channels", 2);
    gui_settings.file_out_mixfreq = prefs_get_int(SECTION, "file-mixfreq", 44100);
    gui_settings.file_out_resolution = prefs_get_int(SECTION, "file-resolution", 16);

    gui_settings.se_extension_path.lines = prefs_get_str_array(SECTION, "sample-editor-extension-paths",
        &gui_settings.se_extension_path.num);
    gui_settings.se_extension_path.num_allocated = gui_settings.se_extension_path.num;

    gui_settings.loadmod_path = prefs_get_string(SECTION_ALWAYS, "loadmod-path", "~");
    gui_settings.savemod_path = prefs_get_string(SECTION_ALWAYS, "savemod-path", "~");
    gui_settings.savemodaswav_path = prefs_get_string(SECTION_ALWAYS, "savemodaswav-path", "~");
    gui_settings.savesongasxm_path = prefs_get_string(SECTION_ALWAYS, "savesongasxm-path", "~");
    gui_settings.loadsmpl_path = prefs_get_string(SECTION_ALWAYS, "loadsmpl-path", "~");
    gui_settings.savesmpl_path = prefs_get_string(SECTION_ALWAYS, "savesmpl-path", "~");
    gui_settings.loadinstr_path = prefs_get_string(SECTION_ALWAYS, "loadinstr-path", "~");
    gui_settings.saveinstr_path = prefs_get_string(SECTION_ALWAYS, "saveinstr-path", "~");
    gui_settings.loadpat_path = prefs_get_string(SECTION_ALWAYS, "loadpat-path", "~");
    gui_settings.savepat_path = prefs_get_string(SECTION_ALWAYS, "savepat-path", "~");

    gui_settings.rm_path = prefs_get_string(SECTION_ALWAYS, "rm-path", "rm");
    gui_settings.unzip_path = prefs_get_string(SECTION_ALWAYS, "unzip-path", "unzip");
    gui_settings.lha_path = prefs_get_string(SECTION_ALWAYS, "lha-path", "lha");
    gui_settings.gz_path = prefs_get_string(SECTION_ALWAYS, "gz-path", "zcat");
    gui_settings.bz2_path = prefs_get_string(SECTION_ALWAYS, "bz2-path", "bunzip2");

    gui_settings.gui_disable_splash = prefs_get_bool(SECTION_ALWAYS, "gui-disable-splash", FALSE);
}

void gui_settings_save_config(void)
{
    if (gui_settings.save_geometry) {
        GtkAllocation alloc;

        gtk_widget_get_allocation(mainwindow, &alloc);
        gui_settings.st_window_w = alloc.width;
        gui_settings.st_window_h = alloc.height;
        gdk_window_get_root_origin(mainwindow->window,
            &gui_settings.st_window_x,
            &gui_settings.st_window_y);
    }

    prefs_put_int(SECTION, "st-window-x", gui_settings.st_window_x);
    prefs_put_int(SECTION, "st-window-y", gui_settings.st_window_y);
    prefs_put_int(SECTION, "st-window-w", gui_settings.st_window_w);
    prefs_put_int(SECTION, "st-window-h", gui_settings.st_window_h);

    prefs_put_bool(SECTION, "gui-use-hexadecimal-numbers", gui_settings.tracker_hexmode);
    prefs_put_bool(SECTION, "gui-use-upper-case", gui_settings.tracker_upcase);
    prefs_put_bool(SECTION, "gui-ft2-volume", gui_settings.tracker_ft2_volume);
    prefs_put_bool(SECTION, "gui-ft2-wide", gui_settings.tracker_ft2_wide);
    prefs_put_bool(SECTION, "gui-ft2-tone-porta-m", gui_settings.tracker_ft2_tpm);
    prefs_put_bool(SECTION, "gui-volume-decimal", gui_settings.tracker_vol_dec);
    prefs_put_bool(SECTION, "gui-advance-cursor-in-fx-columns", gui_settings.advance_cursor_in_fx_columns);
    prefs_put_bool(SECTION, "gui-asynchronous-editing", gui_settings.asynchronous_editing);
    prefs_put_bool(SECTION, "gui-change-pattern-length", gui_settings.change_patlen);
    prefs_put_bool(SECTION, "gui-try-polyphony", gui_settings.try_polyphony);
    prefs_put_bool(SECTION, "gui-repeat-same-note", gui_settings.repeat_same_note);
    prefs_put_bool(SECTION, "gui-insert-noteoff", gui_settings.insert_noteoff);
    prefs_put_bool(SECTION, "gui-precise-timing", gui_settings.precise_timing);
    prefs_put_double(SECTION, "gui-delay-compensation", gui_settings.delay_comp);
    prefs_put_bool(SECTION, "gui-ctrl-show-instr-sel", gui_settings.ctrl_show_isel);
    prefs_put_bool(SECTION, "gui-playback-looped", gui_settings.looped);
    prefs_put_string(SECTION, "gui-tracker-line-format", gui_settings.tracker_line_format);
    prefs_put_string(SECTION, "tracker-font", gui_settings.tracker_font);
    prefs_put_bool(SECTION, "gui-tempo-bpm-update", gui_settings.tempo_bpm_update);
    prefs_put_bool(SECTION, "tracker-rxx-bug-emulate", gui_settings.rxx_bug_emu);
    prefs_put_bool(SECTION, "player-use-filter", gui_settings.use_filter);
    prefs_put_bool(SECTION, "gui-auto-switch", gui_settings.auto_switch);
    prefs_put_bool(SECTION, "gui-add-extension", gui_settings.add_extension);
    prefs_put_bool(SECTION, "gui-check-compat", gui_settings.check_xm_compat);
    prefs_put_bool(SECTION, "gui-ins-name-overwrite", gui_settings.ins_name_overwrite);
    prefs_put_bool(SECTION, "gui-smp-name-overwrite", gui_settings.smp_name_overwrite);
    prefs_put_bool(SECTION, "gui-display-scopes", gui_settings.gui_display_scopes);
    prefs_put_bool(SECTION, "gui-use-backing-store", gui_settings.gui_use_backing_store);
    prefs_put_int(SECTION, "gui-undo-size", gui_settings.undo_size);
    prefs_put_bool(SECTION, "tracker-highlight-rows", gui_settings.highlight_rows);
    prefs_put_int(SECTION, "tracker-highlight-rows-n", gui_settings.highlight_rows_n);
    prefs_put_int(SECTION, "tracker-highlight-rows-minor-n", gui_settings.highlight_rows_minor_n);
    prefs_put_bool(SECTION, "clavier-colors-gtk", gui_settings.clavier_colors_gtk);
    prefs_put_string(SECTION, "clavier-font", gui_settings.clavier_font);
    prefs_put_bool(SECTION, "save-geometry", gui_settings.save_geometry);
    prefs_put_bool(SECTION, "save-settings-on-exit", gui_settings.save_settings_on_exit);
    prefs_put_int(SECTION, "tracker-update-frequency", gui_settings.tracker_update_freq);
    prefs_put_int(SECTION, "scopes-update-frequency", gui_settings.scopes_update_freq);
    prefs_put_int(SECTION, "scopes-buffer-size", gui_settings.scopes_buffer_size);
    prefs_put_bool(SECTION, "scopes-show-ins-smp", gui_settings.show_ins_smp);
    prefs_put_bool(SECTION, "sharp", gui_settings.sharp);
    prefs_put_bool(SECTION, "bh", gui_settings.bh);
    prefs_put_int(SECTION, "cpt-mask", gui_settings.cpt_mask);
    prefs_put_bool(SECTION, "store-permanent", gui_settings.store_perm);
    prefs_put_int(SECTION, "sample-display-scopes-mode", gui_settings.scopes_mode);
    prefs_put_int(SECTION, "sample-display-editor-mode", gui_settings.editor_mode);
    prefs_put_int(SECTION, "sample-display-sampling-mode", gui_settings.sampling_mode);
    prefs_put_int(SECTION, "selection-mode", gui_settings.selection_mode);
    prefs_put_bool(SECTION, "sample-play-loop", gui_settings.sample_play_loop);
    colors_save(SECTION);

    if (gui_settings.store_perm)
        prefs_put_int(SECTION, "permanent-channels", gui_settings.permanent_channels);

    prefs_put_int(SECTION, "file-channels", gui_settings.file_out_channels);
    prefs_put_int(SECTION, "file-mixfreq", gui_settings.file_out_mixfreq);
    prefs_put_int(SECTION, "file-resolution", gui_settings.file_out_resolution);

    prefs_put_str_array(SECTION, "sample-editor-extension-paths",
        gui_settings.se_extension_path.lines, gui_settings.se_extension_path.num);
}

void gui_settings_save_config_always(void)
{
    prefs_put_string(SECTION_ALWAYS, "loadmod-path", gui_settings.loadmod_path);
    prefs_put_string(SECTION_ALWAYS, "savemod-path", gui_settings.savemod_path);
    prefs_put_string(SECTION_ALWAYS, "savemodaswav-path", gui_settings.savemodaswav_path);
    prefs_put_string(SECTION_ALWAYS, "savesongasxm-path", gui_settings.savesongasxm_path);
    prefs_put_string(SECTION_ALWAYS, "loadsmpl-path", gui_settings.loadsmpl_path);
    prefs_put_string(SECTION_ALWAYS, "savesmpl-path", gui_settings.savesmpl_path);
    prefs_put_string(SECTION_ALWAYS, "loadinstr-path", gui_settings.loadinstr_path);
    prefs_put_string(SECTION_ALWAYS, "saveinstr-path", gui_settings.saveinstr_path);
    prefs_put_string(SECTION_ALWAYS, "loadpat-path", gui_settings.loadpat_path);
    prefs_put_string(SECTION_ALWAYS, "savepat-path", gui_settings.savepat_path);

    prefs_put_string(SECTION_ALWAYS, "rm-path", gui_settings.rm_path);
    prefs_put_string(SECTION_ALWAYS, "unzip-path", gui_settings.unzip_path);
    prefs_put_string(SECTION_ALWAYS, "lha-path", gui_settings.lha_path);
    prefs_put_string(SECTION_ALWAYS, "gz-path", gui_settings.gz_path);
    prefs_put_string(SECTION_ALWAYS, "bz2-path", gui_settings.bz2_path);

    prefs_put_bool(SECTION_ALWAYS, "gui-disable-splash", gui_settings.gui_disable_splash);
}

void gui_settings_make_path(const gchar* fn,
    gchar** store)
{
    gchar* dn = g_path_get_dirname(fn);

    if (*store)
        g_free(*store);

    *store = g_strconcat(dn, "/", NULL);
    g_free(dn);
}
