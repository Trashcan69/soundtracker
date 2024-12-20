
/*
 * The Real SoundTracker - GUI support routines
 *
 * Copyright (C) 1998-2019 Michael Krause
 * Copyright (C) 2020-2023 Yury Aliaev
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
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "extspinbutton.h"
#include "gui-subs.h"
#include "gui.h"

const gint SIZES_MENU_TOOLBOX[] = {16, 22, 0};
const gint SIZES_MENU[] = {16, 0};
const gint SIZES_TOOLBOX[] = {22, 0};

int find_current_toggle(GtkWidget** widgets, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        if (GTK_TOGGLE_BUTTON(*widgets++)->active) {
            return i;
        }
    }
    return -1;
}

void add_empty_hbox(GtkWidget* tobox)
{
    GtkWidget* thing = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(tobox), thing, TRUE, TRUE, 0);
}

void add_empty_vbox(GtkWidget* tobox)
{
    GtkWidget* thing = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(tobox), thing, TRUE, TRUE, 0);
}

void make_radio_group_full_num(const char* labels[],
    GtkWidget* tobox,
    GtkWidget* saveptr[],
    gint t1,
    gint t2,
    void (*sigfunc)(void),
    gpointer data,
    gboolean end,
    guint num)
{
    guint i;
    GtkWidget* thing = NULL;

    for(i = 0; i < num; i++) {
        thing = gtk_radio_button_new_with_label((thing
                                                 ? gtk_radio_button_get_group(GTK_RADIO_BUTTON(thing))
                                                 : NULL),
            gettext(labels[end ? num - i - 1: i]));
        saveptr[end ? num - i - 1: i] = thing;
        gtk_widget_show(thing);
        (end ? gtk_box_pack_end : gtk_box_pack_start)(GTK_BOX(tobox), thing, t1, t2, 0);
        if (sigfunc) {
            g_signal_connect(thing, "clicked", G_CALLBACK(sigfunc), data);
        }
    }
}

GtkWidget*
make_labelled_radio_group_box_full_num(const char* title,
    const char* labels[],
    GtkWidget* saveptr[],
    void (*sigfunc)(),
    gpointer data,
    guint num)
{
    GtkWidget *box, *thing;

    box = gtk_hbox_new(FALSE, 4);

    thing = gtk_label_new(title);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);

    make_radio_group_full_num(labels, box, saveptr, FALSE, TRUE, sigfunc, data, FALSE, num);

    return box;
}

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
    gint* handler_id)
{
    GtkWidget *hbox, *thing;

    hbox = gtk_hbox_new(FALSE, 4);

    thing = gtk_label_new(title);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    *spin = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(def, min, max, step, page, 0.0)), 0, digits, in_mainwindow);
    gtk_box_pack_end(GTK_BOX(hbox), *spin, FALSE, TRUE, 0);
    gtk_widget_show(*spin);
    if (callback) {
        gint tmp_id = g_signal_connect(*spin, signal,
            G_CALLBACK(callback), callbackdata);

        if (handler_id)
            *handler_id = tmp_id;
    }

    return hbox;
}

GtkWidget*
gui_put_labelled_spin_button_with_adj(const gchar* title,
    GtkAdjustment* adj,
    GtkWidget* box,
    const gboolean end)
{
    GtkWidget *hbox, *thing;

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 0);
    thing = gtk_label_new(_(title));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    thing = extspinbutton_new(adj, 0, 0, TRUE);
    (end ? gtk_box_pack_end : gtk_box_pack_start)(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show_all(hbox);

    return thing;
}

GtkWidget*
gui_subs_create_slider_full(const gchar* title,
    const gdouble min,
    const gdouble max,
    void (*changedfunc)(),
    GtkAdjustment** adj,
    gboolean in_mainwindow,
    gint* tag)
{
    GtkWidget *thing, *box;
    gint id;

    box = gtk_hbox_new(FALSE, 4);

    if (title) {
        thing = gtk_label_new(title);
        gtk_widget_show(thing);
        gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
    }

    *adj = GTK_ADJUSTMENT(gtk_adjustment_new(min, min, max, 1.0, (max - min) / 10.0, 0.0));
    thing = gtk_hscale_new(*adj);
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);

    thing = extspinbutton_new(*adj, 0.0, 0, in_mainwindow);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    if (changedfunc) {
        id = g_signal_connect(*adj, "value_changed",
            G_CALLBACK(changedfunc), NULL);
        if (tag)
            *tag = id;
    }

    return box;
}

GtkWidget*
gui_stringlist_in_scrolled_window(const int n, const gchar* const* tp, GtkWidget* hbox, gboolean expandfill)
{
    GType* types;
    GtkWidget* list;
    guint i;

    types = g_new(GType, n);
    for (i = 0; i < n; i++)
        types[i] = G_TYPE_STRING;
    list = gui_list_in_scrolled_window(n, tp, hbox, types, NULL, NULL, GTK_SELECTION_BROWSE, expandfill, expandfill);
    g_free(types);
    return list;
}

inline void
gui_list_clear(GtkWidget* list)
{
    gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list))));
}

inline void
gui_list_clear_with_model(GtkTreeModel* model)
{
    gtk_list_store_clear(GTK_LIST_STORE(model));
}

GtkTreeModel*
gui_list_freeze(GtkWidget* list)
{
    GtkTreeModel* model;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    g_object_ref(model);
    gtk_tree_view_set_model(GTK_TREE_VIEW(list), NULL);

    return model;
}

void gui_list_thaw(GtkWidget* list, GtkTreeModel* model)
{
    gtk_tree_view_set_model(GTK_TREE_VIEW(list), model);
    g_object_unref(model);
}

static gboolean hover_changed(GtkTreeView* widget, GdkEvent* event, gpointer data)
{
    gboolean is_hover = data != NULL;
    gtk_tree_view_set_hover_selection(widget, is_hover);
    return FALSE;
}

GtkWidget*
gui_list_in_scrolled_window_full(const int n, const gchar* const* tp, GtkWidget* hbox,
    GType* types, const gfloat* alignments, const gboolean* expands,
    GtkSelectionMode mode, gboolean expand, gboolean fill, GtkWidget** scrolledwindow,
    GtkPolicyType hpolicy, GtkPolicyType vpolicy)
{
    GtkWidget* list;
    GtkWidget* sw;
    guint i;
    GtkListStore* list_store;
    GtkTreeViewColumn* column;
    GtkCellRenderer* renderer;
    GtkTreeSelection* sel;

    list_store = gtk_list_store_newv(n, types);
    list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    for (i = 0; i < n; i++) {
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_(tp[i]), renderer, "text", i, NULL);
        if (alignments) {
            g_object_set(G_OBJECT(renderer), "xalign", alignments[i], NULL);
            gtk_tree_view_column_set_alignment(column, alignments[i]);
        }
        g_object_set(G_OBJECT(renderer), "ypad", 0, NULL);
        if (expands)
            gtk_tree_view_column_set_expand(column, expands[i]);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
    }

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    gtk_tree_selection_set_mode(sel, mode);

    sw = gtk_scrolled_window_new(NULL, NULL);
    if (scrolledwindow)
        *scrolledwindow = sw;
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), hpolicy, vpolicy);
    gtk_container_add(GTK_CONTAINER(sw), list);

    /* Making the pointer following the cursor when the button is pressed (like in gtk+-1) */
    /* TODO: enabling autoscrolling when the pointer is moved up/down */
    g_signal_connect(list, "button-press-event", G_CALLBACK(hover_changed), GINT_TO_POINTER(TRUE));
    g_signal_connect(list, "button-release-event", G_CALLBACK(hover_changed), NULL);
    g_signal_connect(list, "leave-notify-event", G_CALLBACK(hover_changed), NULL);

    gtk_box_pack_start(GTK_BOX(hbox), sw, expand, fill, 0);
    /* According to Gtk+ documentation this is not recommended but lists are not strippy by default...*/
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(list), TRUE);

    return list;
}

GtkTreeSelection*
gui_list_handle_selection(GtkWidget* list, GCallback handler, gpointer data)
{
    GtkTreeSelection* sel;

    sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    g_signal_connect_after(sel, "changed", handler, data);

    return sel;
}

gint gui_list_get_selection_index(GtkTreeSelection* sel)
{
    GtkTreeModel* mdl;
    GtkTreeIter iter;
    gchar* str;

    if (gtk_tree_selection_get_selected(sel, &mdl, &iter)) {
        gint row = atoi(str = gtk_tree_model_get_string_from_iter(mdl, &iter));
        g_free(str);

        return row;
    } else
        return -1;
}

void gui_string_list_set_text(GtkWidget* list, guint row, guint col, const gchar* string)
{
    GtkTreeIter iter;
    GtkTreeModel* tree_model;

    if (gui_list_get_iter(row, tree_model = GUI_GET_TREE_MODEL(list), &iter))
        gtk_list_store_set(GTK_LIST_STORE(tree_model), &iter, col, string, -1);
}

void gui_list_select(GtkWidget* list, guint row, gboolean use_align, gfloat align)
{
    gchar* path_string;
    GtkTreePath* path;
    GtkTreeIter iter;
    GtkTreeSelection* sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));

    if (!gui_list_get_iter(row, GUI_GET_TREE_MODEL(list), &iter))
        return;
    gtk_tree_selection_select_iter(sel, &iter);
    path_string = g_strdup_printf("%u", row);
    path = gtk_tree_path_new_from_string(path_string);
    gtk_tree_view_set_cursor(GTK_TREE_VIEW(list), path,
        gtk_tree_view_get_column(GTK_TREE_VIEW(list), 0), FALSE);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(list), path, NULL,
        use_align, align, 0.0);

    g_free(path_string);
    gtk_tree_path_free(path);
}

GtkWidget* gui_button(const gchar* stock,
    GCallback callback,
    gpointer userdata,
    GtkWidget* box,
    const gboolean small,
    const gboolean fill)
{
    GtkWidget* button;

    if (small) {
        GtkWidget* image = gtk_image_new_from_stock(stock, GTK_ICON_SIZE_MENU);

        button = gtk_button_new();
        gtk_container_add(GTK_CONTAINER(button), image);
    } else
        button = gtk_button_new_from_stock(stock);
    g_signal_connect_swapped(button, "clicked",
        G_CALLBACK(callback), userdata);

    if (box)
        gtk_box_pack_start(GTK_BOX(box), button, fill, fill, 0);

    return button;
}

void gui_message_dialog(GtkWidget** dialog, const gchar* text, GtkMessageType type, const gchar* title, gboolean need_update)
{
    if (!*dialog) {
        *dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, type,
            GTK_BUTTONS_CLOSE, "%s", text);
        gtk_window_set_title(GTK_WINDOW(*dialog), _(title));
    } else if (need_update) {
        gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(*dialog), text);
    }

    gtk_dialog_run(GTK_DIALOG(*dialog));
    gtk_widget_hide(*dialog);
}

void
gui_errno_dialog(GtkWidget *parent, const gchar *text, const int err)
{
    gchar *buf = g_strdup_printf("%s: %s", text, g_strerror(err));
    static GtkWidget *dialog = NULL;

    if (!dialog) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE, "%s", buf);
        gtk_window_set_title(GTK_WINDOW(dialog), _("Error!"));
    } else
        gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog), buf);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(dialog);
    g_free(buf);
}

gboolean
gui_ok_cancel_modal(GtkWidget* parent, const gchar* text)
{
    gint response;
    static GtkWidget* dialog = NULL;

    if (!dialog) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(parent), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
            NULL);
        gtk_window_set_title(GTK_WINDOW(dialog), _("Question"));
    }
    gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog), text);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(dialog);

    return (response == GTK_RESPONSE_OK);
}

void gui_oom_error(void)
{
    static GtkWidget* dialog = NULL;
    gui_error_dialog(&dialog, _("Out of memory error!"), FALSE);
}

FILE*
gui_fopen(const gchar* name, const gchar* utf_name, const gchar* mode)
{
    FILE* f;

    errno = 0;
    f = fopen(name, mode);
    if (!f) {
        gchar *buf = g_strdup_printf(_("Error while opening file %s"), utf_name);

        gui_errno_dialog(mainwindow, buf, errno);
        g_free(buf);
    }

    return f;
}

gchar*
gui_filename_from_utf8(const gchar* old_name)
{
    GtkWidget* dialog;

    GError* error = NULL;
    gchar* name = g_filename_from_utf8(old_name, -1, NULL, NULL, &error);

    if (!name) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
            _("An error occured when filename character set conversion:\n%s\n"
              "The file operation probably failed."),
            error->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_error_free(error);
    }

    return name;
}

gchar*
gui_filename_to_utf8(const gchar* old_name)
{
    GtkWidget* dialog;

    GError* error = NULL;
    gchar* name = g_filename_to_utf8(old_name, -1, NULL, NULL, &error);

    if (!name) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
            _("An error occured when filename character set conversion:\n%s\n"
              "The file operation probably failed."),
            error->message);
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_error_free(error);
    }

    return name;
}

GtkWidget*
gui_combo_new(GtkListStore* ls)
{
    GtkWidget* thing;
    GtkCellRenderer* cell;

    thing = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ls));
    g_object_unref(ls);

    cell = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(thing), cell, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(thing), cell, "text", 0, NULL);

    return thing;
}

GtkBuilder* gui_builder_from_file(const gchar* name, const struct menu_callback cb[])
{
    GtkBuilder* builder = gtk_builder_new();
    GError* error = NULL;
    guint i;

    if (!gtk_builder_add_from_file(builder, name, &error)) {
        g_critical(_("%s.\nLoading widgets' description from %s file failed!\n"),
            error->message, name);
        g_error_free(error);
        return NULL;
    }

    if (cb)
        for (i = 0; cb[i].widget_name; i++) {
            GtkWidget* w = GTK_WIDGET(gtk_builder_get_object(builder, cb[i].widget_name));

            if (w)
                g_signal_connect_swapped(w, "activate", G_CALLBACK(cb[i].fn), cb[i].data);
        }

    return builder;
}

gboolean
gui_delete_noop(void)
{
    /* For dialogs, Response callback already hides this window */
    return TRUE;
}

void gui_dialog_adjust(GtkWidget* dialog, gint default_id)
{
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 4);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), default_id);
}

void gui_set_escape_close(GtkWidget* window)
{
    GtkAccelGroup* group = gtk_accel_group_new();
    GClosure* closure = g_cclosure_new_swap(G_CALLBACK(gtk_widget_hide), window, NULL);

    gtk_accel_group_connect(group, GDK_Escape, 0, 0, closure);
    gtk_window_add_accel_group(GTK_WINDOW(window), group);
}

typedef struct _compare_data {
    guint value;
    gint number;
} compare_data;

static gboolean
compare_func(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, gpointer data)
{
    compare_data* cmp_data = (compare_data*)data;
    guint cur_val;

    gtk_tree_model_get(model, iter, 0, &cur_val, -1);

    if (cur_val == cmp_data->value) {
        gint* indices = gtk_tree_path_get_indices(path);

        cmp_data->number = indices[0];
        return TRUE; /* The desired element is found */
    }

    return FALSE;
}

gboolean
gui_set_active_combo_item(GtkWidget* combobox, GtkTreeModel* model, guint item)
{
    compare_data cmp_data;

    cmp_data.value = item;
    cmp_data.number = -1;
    gtk_tree_model_foreach(model, compare_func, &cmp_data);

    if (cmp_data.number >= 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), cmp_data.number);
        return TRUE;
    }

    return FALSE;
}

typedef struct _str_cmp_data {
    const gchar* str;
    gboolean success;
    GtkComboBoxText* combobox;
} str_cmp_data;

static gboolean
str_cmp_func(GtkTreeModel* model, GtkTreePath* path, GtkTreeIter* iter, gpointer data)
{
    gchar* item_str = NULL;
    str_cmp_data* scd = (str_cmp_data*)data;

    gtk_tree_model_get(model, iter, 0, &item_str, -1);
    if (!item_str)
        return TRUE; /* Aborting due to error */

    if (!g_ascii_strcasecmp(item_str, scd->str)) {
        scd->success = TRUE;
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(scd->combobox), iter);
        g_free(item_str);
        return TRUE;
    }

    g_free(item_str);
    return FALSE;
}

void gui_combo_box_prepend_text_or_set_active(GtkComboBoxText* combobox, const gchar* text, gboolean force_active)
{
    str_cmp_data scd;

    GtkTreeModel* model = gtk_combo_box_get_model(GTK_COMBO_BOX(combobox));

    scd.str = text;
    scd.success = FALSE;
    scd.combobox = combobox;
    gtk_tree_model_foreach(model, str_cmp_func, &scd);

    if (!scd.success) {
        gtk_combo_box_text_prepend_text(combobox, text);
        if (force_active)
            gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 0);
    }
}

gint gui_get_text_entry(int length,
    void (*changedfunc)(),
    GtkWidget** widget)
{
    GtkWidget* thing;

    thing = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(thing), length);

    *widget = thing;

    return g_signal_connect(thing, "changed",
        G_CALLBACK(changedfunc), NULL);
}

GtkWidget*
gui_get_widget(GtkBuilder *builder, const gchar* name, const gchar* file)
{
    GtkWidget* w = GTK_WIDGET(gtk_builder_get_object(builder, name));

    if (!w)
        g_critical(_("GUI creation error: Widget '%s' is not found in %s file."), name, file);

    return w;
}

static gboolean
call_menu(GtkWidget* widget, GdkEventButton* event, GtkMenu* menu)
{
    if (event->button == 3) {
        gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event->button, event->time);
        return TRUE;
    }

    return FALSE;
}

void gui_popup_menu_attach(GtkWidget* menu, GtkWidget* widget, gpointer* user_data)
{
    gtk_menu_attach_to_widget(GTK_MENU(menu), widget, NULL);
    g_signal_connect(menu, "deactivate", G_CALLBACK(gtk_widget_hide), NULL);
    g_signal_connect(widget, "button-press-event", G_CALLBACK(call_menu), menu);
}

void
gui_get_pixel_size(GtkWidget* w, const char* text, gint* width, gint* height)
{
    PangoContext* context = gtk_widget_create_pango_context(w);
    PangoLayout* layout = pango_layout_new(context);

    pango_layout_set_font_description(layout, w->style->font_desc);
    pango_layout_set_text(layout, text, -1);
    pango_layout_get_pixel_size(layout, width, height);

    g_object_unref(layout);
    g_object_unref(context);
}

GdkPixbuf*
gui_pixbuf_new_from_file(const gchar* path)
{
    GError* error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &error);

    if (error) {
        static GtkWidget *error_dialog = NULL;

        gui_error_dialog(&error_dialog, error->message, TRUE);
        g_error_free(error);
        pixbuf = NULL;
    }

    return pixbuf;
}

void
gui_add_icons(const icon_set* icons)
{
    gint i;

    for (i = 0; icons[i].gui_name; i++) {
        gint j;

        for (j = 0; icons[i].sizes[j]; j++) {
            gchar *icon_path = g_strdup_printf(DATADIR "/" PACKAGE "/%s-%i.png",
                icons[i].file_name, icons[i].sizes[j]);
            GdkPixbuf* pixbuf = gui_pixbuf_new_from_file(icon_path);

            g_free(icon_path);
            if (pixbuf) {
                gtk_icon_theme_add_builtin_icon(icons[i].gui_name, gdk_pixbuf_get_width(pixbuf), pixbuf);
                g_object_unref(G_OBJECT(pixbuf));
            }
        }
    }
}

void
gui_set_list_size(GtkWidget* w, const gint nsym_x, const gint nsym_y)
{
    gint font_size;
    GdkScreen *screen;
    gdouble resolution;

    /* Code for determination of the file chooser size is
       borrowed from Gtk+-2 sources (gtkfilechooserdefault.c).
       Can be used with any list widget */
    screen = gtk_widget_get_screen(w);
    if (screen) {
        resolution = gdk_screen_get_resolution(screen);
        if (resolution < 0.0) /* will be -1 if the resolution is not defined in the GdkScreen */
            resolution = 96.0;
    } else
        resolution = 96.0;

    font_size = pango_font_description_get_size(gtk_widget_get_style(w)->font_desc);
    font_size = PANGO_PIXELS(font_size) * resolution / 72.0;
    gtk_widget_set_size_request(w, nsym_x > 0 ? font_size * nsym_x : -1,
        nsym_y > 0 ? font_size * nsym_y : -1);
}

static gboolean
subwindow_keyevent(GtkWidget* w,
    GdkEvent* event,
    GtkWidget* mw)
{
    GtkWidget* focus_widget = GTK_WINDOW(w)->focus_widget;
    guint keyval = ((GdkEventKey*)event)->keyval;

    if (GTK_IS_ENTRY(focus_widget) || keyval == ' ' || keyval == GDK_KEY_Escape ||
        keyval == GDK_KEY_Tab || keyval == GDK_KEY_ISO_Left_Tab || keyval == GDK_KEY_Return ||
        keyval == GDK_KEY_Up || keyval == GDK_KEY_Down ||
        keyval == GDK_KEY_Left || keyval == GDK_KEY_Right ||
        keyval == GDK_KEY_Page_Up || keyval == GDK_KEY_Page_Down) {
        /* Keyboard navigation and manipulating with entries */
        return FALSE;
    } else {
        /* Other events are transmitted to the main window */
        gtk_widget_event(mw, event);
        return TRUE;
    }
}

void
gui_translate_keyevents(GtkWidget* w,
    GtkWidget* mw,
    const gboolean trans_rel)
{
    g_assert(GTK_IS_WINDOW(w));

    if (trans_rel) {
        gtk_widget_add_events(w, GDK_KEY_RELEASE_MASK);
        g_signal_connect(w, "key-release-event",
            G_CALLBACK(subwindow_keyevent), mw);
    }
    g_signal_connect(w, "key-press-event",
        G_CALLBACK(subwindow_keyevent), mw);
}
