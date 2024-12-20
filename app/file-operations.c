
/*
 * The Real SoundTracker - file operations page
 *
 * Copyright (C) 1999-2019 Michael Krause
 * Copyright (C) 2020, 2021 Yury Aliaev
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

#include <dirent.h>
#include <errno.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdktypes.h>
#include <glib/gi18n.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "file-operations.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "gui.h"
#include "keys.h"
#include "preferences.h"
#include "track-editor.h"

enum {
    FOP_IS_SAVE = 1 << 0,
    FOP_TRIGGER_A = 1 << 1,
    FOP_TRIGGER_B = 1 << 2,
};

struct _file_op {
    GtkWidget *dialog, *chooser;
    void (*callback)();
    gchar flags;
    gchar index; /* -1 for standalone dialogs */
    const gchar *title, *tip, *ext;
    gchar** path;
};

/* Some single-click magick to avoid artifacts when directory changes */
static gint current_subpage = 0, stored_subpage = 0;
static gint current_page = 0;
static gboolean need_return = FALSE, nostore_subpage = FALSE;

static GtkWidget *rightnb = NULL, *leftbox, *radio[DIALOG_LAST];
static GSList *fileops = NULL;

static inline file_op*
find_fileop(gint subpage)
{
    GSList *l;

    g_assert(fileops != NULL);
    g_assert(subpage >= 0);

    for (l = fileops; l; l = l->next) {
        file_op *fop = l->data;

        if (fop->index == subpage)
            return fop;
    }

    return NULL;
}

static void
typeradio_changed(GtkToggleButton* w, gpointer data)
{
    if (gtk_toggle_button_get_active(w)) {
        current_subpage = GPOINTER_TO_INT(data);
        if (!nostore_subpage)
            stored_subpage = current_subpage;
        gtk_notebook_set_current_page(GTK_NOTEBOOK(rightnb), current_subpage);
    }
}

static void
folder_changed(file_op* fop)
{
    if (fop->flags & FOP_TRIGGER_A)
        fop->flags &= ~FOP_TRIGGER_A;
    else
        fop->flags |= FOP_TRIGGER_B;
}

static gboolean
do_file_callback(file_op* fop, gchar* filename)
{
    gchar* utfpath = gui_filename_to_utf8(filename);
    gchar* localpath = filename;

    if (utfpath) {
        if (fop->flags & FOP_IS_SAVE) {
            if (gui_settings.add_extension) {
                gchar *fname = g_path_get_basename(utfpath);

                /* File can begin with '.'. One-letter name does need an extension */
                if (strlen(fname) == 1 || !strchr(&fname[1], '.')) {
                    gchar *fname_new = g_strconcat(utfpath, ".", fop->ext, NULL);

                    g_free(utfpath);
                    utfpath = fname_new;
                    localpath = gui_filename_from_utf8(utfpath);
                    if (!localpath) {
                        g_free(utfpath);
                        return FALSE;
                    }
                }
                g_free(fname);
            }
            if (g_file_test(localpath, G_FILE_TEST_EXISTS)) {
                gchar *mess = g_strdup_printf(_("The file named \"%s\" already exists.\nDo you want to replace it?"),
                                                g_utf8_next_char(g_strrstr(utfpath, "/")));
                gboolean answer = gui_ok_cancel_modal(mainwindow, mess);

                g_free(mess);
                if (!answer) {
                    g_free(utfpath);
                    return FALSE;
                }
            }
        }

        gui_settings_make_path(utfpath, fop->path);
        fop->callback(utfpath, localpath);

        g_free(utfpath);
        if (localpath != filename)
            g_free(localpath);
        return TRUE;
    }

    return FALSE;
}

static void
file_chosen(file_op* fops)
{
    gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fops->chooser));

    /* Silently ignore errors, because filechooser sometimes returns NULL with the "selection-changed" signal */
    if (!filename)
        return;

    if (filename[0] != '\0') {
        /* Please don't ask me anything about the further lines. In order to implement single-click
           operation with the Gtk+-2 file chooser one have to do something pervetred unnatural... */

        /* Change directory by single click for both save and load */
        if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
            fops->flags |= FOP_TRIGGER_A;
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fops->chooser), filename);
        } else {
            fops->flags &= ~FOP_TRIGGER_A;
            if ((!(fops->flags & FOP_IS_SAVE)) &&
                g_file_test(filename, G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_SYMLINK)) {
                /* File activating by single click only for load */
                if (do_file_callback(fops, filename) && need_return) {
                    need_return = FALSE;
                    gui_go_to_page(current_page);
                }
            }
        }
    }
    g_free(filename);
}

static void
load_clicked(file_op* fops)
{
    fops->flags &= ~(FOP_TRIGGER_A & FOP_TRIGGER_B);
    file_chosen(fops);
}

static void
save_clicked(file_op* fop)
{
    gchar* name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fop->chooser));

    if (!name)
        return;

    if (name[0] != '\0') {
        if (do_file_callback(fop, name) && need_return) {
            gui_go_to_page(current_page);
            need_return = FALSE;
        }
    }
    g_free(name);
}

static void set_filepath(GtkWidget* fc, const gchar* path)
{
    gchar* newname = gui_filename_from_utf8(path);

    if (!newname)
        return;
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fc), newname);
    g_free(newname);
}

static void
add_filters(GtkFileChooser* fc, const char** formats[])
{
    GtkFileFilter *current, *omni;
    const gchar** format;
    guint i = 0;

    omni = gtk_file_filter_new();
    gtk_file_filter_set_name(omni, _("All supported types"));

    while ((format = formats[i])) {
        guint j = 1;

        current = gtk_file_filter_new();
        gtk_file_filter_set_name(current, _(format[0]));
        while (format[j]) {
            gtk_file_filter_add_pattern(current, format[j]);
            gtk_file_filter_add_pattern(omni, format[j]);
            j++;
        }
        gtk_file_chooser_add_filter(fc, current);
        i++;
    }

    gtk_file_chooser_add_filter(fc, omni);
    gtk_file_chooser_set_filter(fc, omni);

    omni = gtk_file_filter_new();
    gtk_file_filter_set_name(omni, _("All files"));
    gtk_file_filter_add_pattern(omni, "*");
    gtk_file_chooser_add_filter(fc, omni);
}

static gboolean
brelease(GtkWidget* w, GdkEventButton* ev, file_op* fop)
{
    if (ev->button == 1) {
        if (fop->flags & FOP_TRIGGER_B)
            fop->flags |= FOP_TRIGGER_A;
        else
            file_chosen(fop);

        fop->flags &= ~FOP_TRIGGER_B;
    }
    return FALSE;
}

static void
filesel_dialog_response(file_op* fop)
{
    gtk_dialog_response(GTK_DIALOG(fop->dialog), GTK_RESPONSE_ACCEPT);
}

/* Two-step procedure since file dialogs can be declared at random order,
   but shoud appear as notebook pages as intended */

file_op* fileops_dialog_create(const guint index, const gchar* title, gchar** path, void (*callback)(),
    const gboolean is_embedded, const gboolean is_save,
    const gchar** formats[], const gchar* tip, const gchar* ext)
{
    GtkWidget *fc;
    file_op *fop = g_new0(file_op, 1);

    g_assert(fop != NULL);

    fop->callback = callback;
    fop->title = title;
    fop->tip = tip;
    fop->path = path;
    fop->ext = ext;
    if (is_save)
        fop->flags = FOP_IS_SAVE;

    if (is_embedded) {
#ifdef GTK_HACKS
        GList* list;

            /* Even for Save action we create filechooser in OPEN mode and explicetly
               enable CREATE_DIR button and add filename entry. Native SAVE mode leads
               to creation of too many place taking unneccessary widgets */
        fc = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);
#else
        /* Traditional way for those who don't like hacking */
        fc = gtk_file_chooser_widget_new(is_save ? GTK_FILE_CHOOSER_ACTION_SAVE
                                                 : GTK_FILE_CHOOSER_ACTION_OPEN);
#endif
        g_signal_connect_swapped(fc, "file-activated",
            G_CALLBACK(is_save ? save_clicked : file_chosen), fop);
        g_signal_connect_swapped(fc, "current-folder-changed", G_CALLBACK(folder_changed), fop);
        g_signal_connect(fc, "button-release-event", G_CALLBACK(brelease), fop);

#ifdef GTK_HACKS
        if (is_save) { /* Create Directory button */
            GtkWidget *widget;

            list = gtk_container_get_children(GTK_CONTAINER(fc));
            widget = GTK_WIDGET(g_list_nth_data(list, 0)); /* FileChooserDefault */
            g_list_free(list);
            list = gtk_container_get_children(GTK_CONTAINER(widget));
            widget = GTK_WIDGET(g_list_nth_data(list, 0)); /* vbox */
            g_list_free(list);
            list = gtk_container_get_children(GTK_CONTAINER(widget));
            widget = GTK_WIDGET(g_list_nth_data(list, 0)); /* browse_path_bar_box */
            g_list_free(list);
            list = gtk_container_get_children(GTK_CONTAINER(widget)); /* Create Folder button :) */
            widget = GTK_WIDGET(g_list_nth_data(list, g_list_length(list) - 1));
            g_list_free(list);

            gtk_widget_show(widget);
         }
#endif
    } else { /* Standalone */
        GtkWidget* vbox;

        /* Some work around the GtkFileChooserDialog bug: all files selected by
           it get into the recent list. In ST standalone dialogs are used for
           rather auxiliary things like pattern saving / loading, and these files
           should not be in the recent list which is for modules only. So we
           create out own dialog. */
        fc = gtk_file_chooser_widget_new(is_save ? GTK_FILE_CHOOSER_ACTION_SAVE
                                                 : GTK_FILE_CHOOSER_ACTION_OPEN);
        fop->dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(mainwindow), 0,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            is_save ? GTK_STOCK_SAVE : GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
        g_signal_connect_swapped(fc, "file-activated",
            G_CALLBACK(filesel_dialog_response), fop);

        gui_set_list_size(fc, 40, 40);
        gtk_container_set_border_width(GTK_CONTAINER(fop->dialog), 5);
        gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_action_area(GTK_DIALOG(fop->dialog))), 5);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(fop->dialog));
        gtk_box_set_spacing(GTK_BOX(vbox), 2);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
        gtk_box_pack_start(GTK_BOX(vbox), fc, TRUE, TRUE, 0);
        gtk_widget_show(fc);

        fop->index = -1;
    }

    fop->chooser = fc;
    if (formats)
        add_filters(GTK_FILE_CHOOSER(fc), formats);

    set_filepath(fc, *path);

    fileops = g_slist_append(fileops, fop);
    return fop;
}

void fileops_page_post_create(void)
{
    gint num = 0;
    GSList *l;

    for (l = fileops; l; l = l->next) {
        file_op *fop = l->data;

        if (fop->index != -1) { /* Embedded */
            GtkWidget *widget, *box, *buttonbox;

            /* Radio buttons */
            radio[num] = gtk_radio_button_new_with_label(num ? gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio[0])) : NULL,
                fop->title);
            if (fop->tip)
                gtk_widget_set_tooltip_text(radio[num], _(fop->tip));
            gtk_widget_show(radio[num]);
            gtk_box_pack_start(GTK_BOX(leftbox), radio[num], FALSE, FALSE, 0);
            g_signal_connect(radio[num], "toggled", G_CALLBACK(typeradio_changed), GINT_TO_POINTER(num));

            /* File selector with action button */
            box = gtk_vbox_new(FALSE, 2);
            gtk_box_pack_start(GTK_BOX(box), fop->chooser, TRUE, TRUE, 0);

            buttonbox = gtk_hbutton_box_new();
            gtk_button_box_set_layout(GTK_BUTTON_BOX(buttonbox), GTK_BUTTONBOX_END);
            gtk_box_pack_start(GTK_BOX(box), buttonbox, FALSE, FALSE, 0);
            widget = gtk_button_new_from_stock(fop->flags & FOP_IS_SAVE ? GTK_STOCK_SAVE : GTK_STOCK_OPEN);
            gtk_box_pack_end(GTK_BOX(buttonbox), widget, FALSE, FALSE, 0);
            g_signal_connect_swapped(widget, "clicked",
                G_CALLBACK(fop->flags & FOP_IS_SAVE ? save_clicked : load_clicked),
                fop);

            gtk_notebook_append_page(GTK_NOTEBOOK(rightnb), box, NULL);
            fop->index = num++;
        }
    }

    gtk_widget_show_all(rightnb);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(rightnb), 0);
}

void fileops_page_create(GtkNotebook* nb)
{
    GtkWidget *hbox, *vbox, *thing;

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);
    gtk_notebook_append_page(nb, hbox, gtk_label_new(_("File")));

    leftbox = vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

    thing = rightnb = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(thing), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(thing), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), rightnb, TRUE, TRUE, 0);
    gtk_widget_show_all(hbox);
}

gboolean
fileops_page_handle_keys(int shift,
    int ctrl,
    int alt,
    guint32 keyval,
    gboolean pressed)
{
    int i;

    i = keys_get_key_meaning(keyval, ENCODE_MODIFIERS(shift, ctrl, alt), -1);
    if (i != -1 && KEYS_MEANING_TYPE(i) == KEYS_MEANING_NOTE) {
        track_editor_do_the_note_key(i, pressed, keyval, ENCODE_MODIFIERS(shift, ctrl, alt), TRUE);
        return TRUE;
    }

    return FALSE;
}

/* This function is used when the file operations are initiated by menus and buttons
   rather than selection of the file tab by user*/
void fileops_open_dialog(file_op *item)
{
    g_assert(item->chooser != NULL);

    if (item->index != -1) {
        current_page = notebook_current_page;
        nostore_subpage = TRUE;
        gui_set_radio_active(radio, item->index);
        nostore_subpage = FALSE;
        need_return = TRUE;
        gui_go_to_fileops_page();
    } else {
        gboolean file_op_ok;

        do {
            file_op_ok = TRUE;
            gint response = gtk_dialog_run(GTK_DIALOG(item->dialog));

            if (response == GTK_RESPONSE_ACCEPT) {
                gchar* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(item->chooser));

                /* Silently ignore errors */
                if (!filename)
                    break;
                if (filename[0] != '\0')
                    file_op_ok = do_file_callback(item, filename);
                g_free(filename);
            }
        } while(!file_op_ok);

        gtk_widget_hide(item->dialog);
    }
}

/* Restore previous manually set subpage after moving away from the file page */
void fileops_restore_subpage(void)
{
    nostore_subpage = TRUE;
    gui_set_radio_active(radio, stored_subpage);
    nostore_subpage = FALSE;
}

void fileops_enter_pressed(void)
{
    file_op *item = find_fileop(current_subpage);

    if (item) {
        item->flags &= ~(FOP_TRIGGER_A & FOP_TRIGGER_B);
        (item->flags & FOP_IS_SAVE) ? save_clicked(item) : file_chosen(item);
    }
}

void fileops_tmpclean(void)
{
    DIR* dire;
    struct dirent* entry;
    static char tname[1024], fname[1024];

    strcpy(tname, prefs_get_prefsdir());
    strcat(tname, "/tmp/");

    if (!(dire = opendir(tname))) {
        return;
    }

    while ((entry = readdir(dire))) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            strcpy(fname, tname);
            strcat(fname, entry->d_name);
            unlink(fname);
        }
    }

    closedir(dire);
}
