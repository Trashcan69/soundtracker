
/*
 * The Real SoundTracker - GUI support routines (header)
 *
 * Copyright (C) 1998-2019 Michael Krause
 * Copyright (C) 2020-2022 Yury Aliaev
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

#ifndef _GUI_SUBS_H
#define _GUI_SUBS_H

#include <gtk/gtk.h>

/* values for status bar messages */
enum {
    STATUS_IDLE = 0,
    STATUS_PLAYING_SONG,
    STATUS_PLAYING_PATTERN,
    STATUS_LOADING_MODULE,
    STATUS_MODULE_LOADED,
    STATUS_SAVING_MODULE,
    STATUS_MODULE_SAVED,
    STATUS_LOADING_SAMPLE,
    STATUS_SAMPLE_LOADED,
    STATUS_SAVING_SAMPLE,
    STATUS_SAMPLE_SAVED,
    STATUS_LOADING_INSTRUMENT,
    STATUS_INSTRUMENT_LOADED,
    STATUS_SAVING_INSTRUMENT,
    STATUS_INSTRUMENT_SAVED,
    STATUS_SAVING_SONG,
    STATUS_SONG_SAVED,
};

struct menu_callback {
    const gchar* widget_name;
    void (*fn)();
    gpointer data;
};

extern const gint SIZES_MENU_TOOLBOX[];
extern const gint SIZES_MENU[];
extern const gint SIZES_TOOLBOX[];

typedef struct
{
    const gchar *gui_name, *file_name;
    const gint* sizes; /* 0-terminated */
} icon_set;

int find_current_toggle(GtkWidget** widgets,
    int count);

void add_empty_hbox(GtkWidget* tobox);
void add_empty_vbox(GtkWidget* tobox);

void make_radio_group_full_num(const char* labels[],
    GtkWidget* tobox,
    GtkWidget* saveptr[],
    gint t1,
    gint t2,
    void (*sigfunc)(),
    gpointer data,
    gboolean end,
    guint num);

#define make_radio_group_full_ext(labels, tobox, saveptr, t1, t2, sigfunc, data, end) \
make_radio_group_full_num(labels, tobox, saveptr, t1, t2, sigfunc, data, end, sizeof(labels) / sizeof(labels[0]))

#define make_radio_group_full(labels, tobox, saveptr, t1, t2, sigfunc, data) \
make_radio_group_full_num(labels, tobox, saveptr, t1, t2, sigfunc, data, FALSE, sizeof(labels) / sizeof(labels[0]))

#define make_radio_group(labels, tobox, saveptr, t1, t2, sigfunc) \
make_radio_group_full_num(labels, tobox, saveptr, t1, t2, sigfunc, NULL, FALSE, sizeof(labels) / sizeof(labels[0]))

GtkWidget*
make_labelled_radio_group_box_full_num(const char* title,
    const char* labels[],
    GtkWidget* saveptr[],
    void (*sigfunc)(),
    gpointer data,
    guint num);

#define make_labelled_radio_group_box_full(title, labels, saveptr, sigfunc, data) \
make_labelled_radio_group_box_full_num(title, labels, saveptr, sigfunc, data, sizeof(labels) / sizeof(labels[0]))

#define make_labelled_radio_group_box(title, labels, saveptr, sigfunc) \
make_labelled_radio_group_box_full_num(title, labels, saveptr, sigfunc, NULL, sizeof(labels) / sizeof(labels[0]))

GtkWidget*
gui_labelled_spin_button_new_full(const gchar* const title,
    gfloat def,
    gfloat min,
    gfloat max,
    gfloat step,
    gfloat page,
    gint digits,
    GtkWidget** spin,
    const gchar* const signal,
    void (*callback)(),
    void* callbackdata,
    gboolean in_mainwindow,
    gint* handler_id);

#define gui_labelled_spin_button_new(title, min, max, spin, callback, callbackdata, in_mainwindow, handler_id) \
gui_labelled_spin_button_new_full(title, min, min, max, 1.0, 5.0, 0, spin, \
"value-changed", callback, callbackdata, in_mainwindow, handler_id)

static inline gint
gui_put_labelled_spin_button(GtkWidget* destbox,
    const char* title,
    int min,
    int max,
    GtkWidget** spin,
    void (*callback)(),
    void* callbackdata,
    gboolean in_mainwindow)
{
    gint handler_id;
    GtkWidget *hbox = gui_labelled_spin_button_new(title, min, max, spin,
        callback, callbackdata, in_mainwindow, &handler_id);

    gtk_box_pack_start(GTK_BOX(destbox), hbox, FALSE, TRUE, 0);
    return handler_id;
}

GtkWidget*
gui_put_labelled_spin_button_with_adj(const gchar* title,
    GtkAdjustment* adj,
    GtkWidget* box,
    const gboolean end);

static inline void
gui_set_radio_active(GtkWidget** radiobutton, guint i)
{
    if (gtk_widget_is_sensitive(radiobutton[i]))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radiobutton[i]), TRUE);
}

GtkWidget*
gui_subs_create_slider_full(const gchar* title,
    const gdouble min,
    const gdouble max,
    void (*changedfunc)(),
    GtkAdjustment** adj,
    gboolean in_mainwindow,
    gint* tag);

static inline GtkWidget*
gui_subs_create_slider(const gchar* title,
    const gdouble min,
    const gdouble max,
    void (*changedfunc)(),
    GtkAdjustment** adj,
    gboolean in_mainwindow)
{
    return gui_subs_create_slider_full(title, min, max, changedfunc, adj, in_mainwindow, NULL);
}

GtkWidget* gui_combo_new(GtkListStore* ls);

gboolean gui_set_active_combo_item(GtkWidget* combobox,
    GtkTreeModel* model,
    guint item);

void gui_combo_box_prepend_text_or_set_active(GtkComboBoxText* combobox,
    const gchar* text,
    gboolean force_active);

GtkWidget* gui_list_in_scrolled_window_full(const int n,
    const gchar* const* tp,
    GtkWidget* hbox,
    GType* types,
    const gfloat* alignments,
    const gboolean* expands,
    GtkSelectionMode mode,
    gboolean expand,
    gboolean fill,
    GtkWidget** scrolledwindow,
    GtkPolicyType hpolicy,
    GtkPolicyType vpolicy);

static inline GtkWidget* gui_list_in_scrolled_window(const int n,
    const gchar* const* tp,
    GtkWidget* hbox,
    GType* types,
    const gfloat* alignments,
    const gboolean* expands,
    GtkSelectionMode mode,
    gboolean expand,
    gboolean fill)
{
    return gui_list_in_scrolled_window_full(n, tp, hbox, types,
        alignments, expands, mode, expand, fill, NULL,
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
}

GtkWidget* gui_stringlist_in_scrolled_window(const int n,
    const gchar* const* tp,
    GtkWidget* hbox, gboolean expandfill);

void gui_list_clear(GtkWidget* list);
void gui_list_clear_with_model(GtkTreeModel* model);
GtkTreeModel* gui_list_freeze(GtkWidget* list);
void gui_list_thaw(GtkWidget* list,
    GtkTreeModel* model);

#define GUI_GET_LIST_STORE(list) GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)))
#define GUI_GET_TREE_MODEL(list) gtk_tree_view_get_model(GTK_TREE_VIEW(list))

GtkTreeSelection* gui_list_handle_selection(GtkWidget* list,
    GCallback handler,
    gpointer data);
gint gui_list_get_selection_index(GtkTreeSelection* sel);

static inline gboolean
gui_list_get_iter(guint n, GtkTreeModel* tree_model, GtkTreeIter* iter)
{
    gchar* path;
    gboolean result;

    path = g_strdup_printf("%u", n);
    result = gtk_tree_model_get_iter_from_string(tree_model, iter, path);
    g_free(path);
    return result;
}

void gui_string_list_set_text(GtkWidget* list,
    guint row,
    guint col,
    const gchar* string);

void gui_list_select(GtkWidget* list,
    guint row,
    gboolean use_align,
    gfloat align);

GtkWidget* gui_button(const gchar* stock,
    GCallback callback,
    gpointer userdata,
    GtkWidget* box,
    const gboolean small,
    const gboolean fill);

gboolean gui_delete_noop(void);
void gui_set_escape_close(GtkWidget* window);
gboolean gui_ok_cancel_modal(GtkWidget* window, const gchar* text);
void gui_errno_dialog (GtkWidget* window, const gchar* text, const int err);
void gui_message_dialog(GtkWidget** dialog, const gchar* text, GtkMessageType type, const gchar* title, gboolean need_update);
#define gui_warning_dialog(dialog, text, need_update) gui_message_dialog(dialog, text, GTK_MESSAGE_WARNING, N_("Warning"), need_update)
#define gui_error_dialog(dialog, text, need_update) gui_message_dialog(dialog, text, GTK_MESSAGE_ERROR, N_("Error!"), need_update)
#define gui_info_dialog(dialog, text, need_update) gui_message_dialog(dialog, text, GTK_MESSAGE_INFO, N_("Information"), need_update)
void gui_dialog_adjust(GtkWidget* dialog, gint default_id);
FILE* gui_fopen(const gchar* name, const gchar* utf_name, const gchar* mode);
void gui_oom_error(void);

static inline void
gui_dialog_connect_data(GtkWidget* dialog, GCallback resp_cb, gpointer data)
{
    g_signal_connect(dialog, "delete_event",
        G_CALLBACK(gui_delete_noop), NULL);
    g_signal_connect(dialog, "response",
        resp_cb ? resp_cb : G_CALLBACK(gtk_widget_hide), data);
}

#define gui_dialog_connect(dialog, resp_cb) gui_dialog_connect_data(dialog, resp_cb, NULL)

gchar* gui_filename_to_utf8(const gchar* old_name);
gchar* gui_filename_from_utf8(const gchar* old_name);

GtkBuilder* gui_builder_from_file(const gchar* name, const struct menu_callback cb[]);

gint gui_get_text_entry(int length,
    void (*changedfunc)(),
    GtkWidget** widget);
GtkWidget* gui_get_widget(GtkBuilder *builder, const gchar* name, const gchar* file);
void gui_popup_menu_attach(GtkWidget* menu, GtkWidget* widget, gpointer* user_data);
void gui_get_pixel_size(GtkWidget* w, const char* text, gint* width, gint* height);
void gui_set_list_size(GtkWidget* w, const gint nsym_x, const gint nsym_y);
GdkPixbuf* gui_pixbuf_new_from_file(const gchar* path);
/* icon_set should be NULL-terminated */
void gui_add_icons(const icon_set* icons);

static inline void
_gui_block_signals(GObject* objs[], const gint tags[], const gint number, gboolean block)
{
    gint i;

    for (i = 0; i < number; i++)
        (block ? g_signal_handler_block : g_signal_handler_unblock)(objs[i], tags[i]);
}
#define gui_block_signals(objs, tags, block)\
    _gui_block_signals((GObject **)objs, tags, sizeof(objs) / sizeof(objs[0]), block)
void gui_translate_keyevents(GtkWidget* w, GtkWidget* mainwindow, const gboolean translate_releases);

#endif /* _GUI_SUBS_H */
