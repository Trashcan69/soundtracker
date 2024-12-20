
/*
 * The Real SoundTracker - X keyboard handling
 *
 * This file involves heavy X hacking. It may not be beautiful, but
 * it's the solution I've come up with after nearly three years of
 * experimentation...
 *
 * Copyright (C) 1997-2019 Michael Krause
 * Copyright (C) 2000 Fabian Giesen (Win32 stuff)
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

#if !defined(_WIN32)

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "X11/Xlib.h"
#include <X11/keysym.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <glib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gui-settings.h"
#include "gui-subs.h"
#include "gui.h"
#include "keys.h"
#include "preferences.h"
#include "st-subs.h"

static Display* x_display;

enum { BLACK,
    WHITE };

#define NONE_TEXT _("<none>")
#define SECTION "keyboard"

static GtkWidget *cw_list,
    *cw_explabel,
    *cw_explabel2,
    *cw_combo,
    *cw_modtoggles[3],
    *cw_lb1, *cw_lb2, *cw_label3, *cw_uns;
static GtkListStore* ls;
static int cw_currentgroup = -1,
           cw_currentkey = -1;

typedef struct keys_key {
    gchar* title;
    gchar* explanation;
    gint meaning;

    KeySym xkeysym;
    gint xkeycode;
    gint hwcode;
    guint modifiers;
    gchar* local_title;
} keys_key;

static keys_key keys1[] = {
    { "UC-0", NULL, 12, 0, 0, -1},
    { "UC#0", NULL, 13, 0, 0, -1 },
    { "UD-0", NULL, 14, 0, 0, -1 },
    { "UD#0", NULL, 15, 0, 0, -1 },
    { "UE-0", NULL, 16, 0, 0, -1 },
    { "UF-0", NULL, 17, 0, 0, -1 },
    { "UF#0", NULL, 18, 0, 0, -1 },
    { "UG-0", NULL, 19, 0, 0, -1 },
    { "UG#0", NULL, 20, 0, 0, -1 },
    { "UA-0", NULL, 21, 0, 0, -1 },
    { "UA#0", NULL, 22, 0, 0, -1 },
    { "UH-0", NULL, 23, 0, 0, -1 },
    { "UC-1", NULL, 24, 0, 0, -1 },
    { "UC#1", NULL, 25, 0, 0, -1 },
    { "UD-1", NULL, 26, 0, 0, -1 },
    { "UD#1", NULL, 27, 0, 0, -1 },
    { "UE-1", NULL, 28, 0, 0, -1 },
    { "UF-1", NULL, 29, 0, 0, -1 },
    { "UF#1", NULL, 30, 0, 0, -1 },
    { "UG-1", NULL, 31, 0, 0, -1 },
    { NULL }
};

static keys_key keys2[] = {
    { "LC-0", NULL, 0, 0, 0, -1 },
    { "LC#0", NULL, 1, 0, 0, -1 },
    { "LD-0", NULL, 2, 0, 0, -1 },
    { "LD#0", NULL, 3, 0, 0, -1 },
    { "LE-0", NULL, 4, 0, 0, -1 },
    { "LF-0", NULL, 5, 0, 0, -1 },
    { "LF#0", NULL, 6, 0, 0, -1 },
    { "LG-0", NULL, 7, 0, 0, -1 },
    { "LG#0", NULL, 8, 0, 0, -1 },
    { "LA-0", NULL, 9, 0, 0, -1 },
    { "LA#0", NULL, 10, 0, 0, -1 },
    { "LH-0", NULL, 11, 0, 0, -1 },
    { "LC-1", NULL, 12, 0, 0, -1 },
    { "LC#1", NULL, 13, 0, 0, -1 },
    { "LD-1", NULL, 14, 0, 0, -1 },
    { "LD#1", NULL, 15, 0, 0, -1 },
    { "LE-1", NULL, 16, 0, 0, -1 },
    { NULL }
};

#define TYPE_FUNC KEYS_MEANING_TYPE_MAKE(KEYS_MEANING_FUNC)
static keys_key keys3[1 + KEY_LAST + 32 + 1] = {
    /* KeyOff, KEY_LAST fixed entries,
				     32 dynamically generated ones (see keys_generate_channel_explanations()),
				     1 NULL terminator */
    { N_("KOFF"), N_("The key that inserts the special keyoff note for FastTracker modules"),
        KEYS_MEANING_TYPE_MAKE(KEYS_MEANING_KEYOFF), 0, 0, -1, 0 },
    { N_("JMP+"), N_("The key that increases \"jump\" value"),
        KEY_JMP_PLUS + TYPE_FUNC, '`', 0, -1, 0 },
    { N_("JMP-"), N_("The key that decreases \"jump\" value"),
        KEY_JMP_MINUS + TYPE_FUNC, '~', 0, -1, ENCODE_MODIFIERS(1, 0, 0) },
    { N_("PlayMod"), N_("Play module"),
        KEY_PLAY_SNG + TYPE_FUNC, GDK_Control_R, 0, -1, 0 },
    { N_("RecMod"), N_("Play module + recording"),
        KEY_REC_SNG + TYPE_FUNC, GDK_Control_R, 0, -1, ENCODE_MODIFIERS(1, 0, 0) },
    { N_("PlayPat"), N_("Play pattern"),
        KEY_PLAY_PAT + TYPE_FUNC, GDK_Alt_R, 0, -1, 0 },
    { N_("RecPat"), N_("Play pattern + recording"),
        KEY_REC_PAT + TYPE_FUNC, GDK_Meta_R, 0, -1, ENCODE_MODIFIERS(1, 0, 0) },
    { N_("PlayCur"), N_("Play module from cursor"),
        KEY_PLAY_CUR + TYPE_FUNC, GDK_Mode_switch, 0, -1, 0 },
    { N_("RecCur"), N_("Play module from cursor + recording"),
        KEY_REC_CUR + TYPE_FUNC, GDK_Multi_key, 0, -1, ENCODE_MODIFIERS(1, 0, 0) },
    { N_("PlayRow"), N_("Play current pattern row"),
        KEY_PLAY_ROW + TYPE_FUNC, GDK_Menu, 0, -1, 0 },
    { N_("PlayBlk"), N_("Play selected block"),
        KEY_PLAY_BLK + TYPE_FUNC, GDK_Menu, 0, -1, ENCODE_MODIFIERS(1, 0, 0) },
    { N_("SmpTgle"), N_("Toggle sampling"),
        KEY_SMP_TOGGLE + TYPE_FUNC, GDK_Return, 0, -1, 0 },
    { N_("PrevPos"), N_("Go to the previous postion in the pattern order table"),
        KEY_PREV_POS + TYPE_FUNC, GDK_Page_Up, 0, -1, ENCODE_MODIFIERS(0, 1, 0) },
    { N_("PrevPosF"), N_("Go to the previous postion in the pattern order table (faster)"),
        KEY_PREV_POS_F + TYPE_FUNC, GDK_Page_Up, 0, -1, ENCODE_MODIFIERS(1, 1, 0) },
    { N_("NextPos"), N_("Go to the next postion in the pattern order table"),
        KEY_NEXT_POS + TYPE_FUNC, GDK_Page_Down, 0, -1, ENCODE_MODIFIERS(0, 1, 0) },
    { N_("NextPosF"), N_("Go to the next postion in the pattern order table (faster)"),
        KEY_NEXT_POS_F + TYPE_FUNC, GDK_Page_Down, 0, -1, ENCODE_MODIFIERS(1, 1, 0) }
};

typedef struct keys_group {
    const char* title;
    const char* explanation;
    keys_key* keys;
    keys_key* keys_edit;
} keys_group;

static keys_group groups[] = {
    { N_("Upper Octave Keys..."),
        N_("These are the keys on the upper half of the keyboard. "
           "The c key is normally the key to the right of the TAB key. "
           "The rest of the keys should be ordered in a piano keyboard fashion, including "
           "the number keys row above."),
        keys1 },
    { N_("Lower Octave Keys..."),
        N_("These are the keys on the lower half of the keyboard. "
           "The c key is normally the first character key to the right of the left Shift key. "
           "The rest of the keys should be ordered in a piano keyboard fashion, including "
           "the row above."),
        keys2 },
    { N_("Other Keys..."),
        N_("Various other keys"),
        keys3 }
};

#define NUM_KEY_GROUPS ARRAY_SIZE(groups)

typedef struct xkey {
    gchar* xname;
    KeySym xkeysym;
} xkey;

static xkey* xkeymap;
static int xkeymaplen;
static int symspercode;
static int xmin;

static gchar** keyname;

static int capturing = 0, capturing_all;

static gint
keys_keys_array_length(keys_key* keys)
{
    int i = 0;

    while ((keys++)->title)
        i++;

    return i;
}

static keys_key*
keys_duplicate_keys_array(keys_key* keys)
{
    int l = keys_keys_array_length(keys);
    keys_key* copy;

    copy = g_new(keys_key, l + 1);
    memcpy(copy, keys, l * sizeof(keys_key));
    copy[l].title = NULL;

    return copy;
}

static void
keys_initialize_editing(void)
{
    int i;

    for (i = 0; i < NUM_KEY_GROUPS; i++) {
        groups[i].keys_edit = keys_duplicate_keys_array(groups[i].keys);
        g_assert(groups[i].keys_edit != NULL);
    }
}

static void
keys_finish_editing(void)
{
    int i;

    for (i = 0; i < NUM_KEY_GROUPS; i++) {
        g_free(groups[i].keys_edit);
        groups[i].keys_edit = NULL;
    }
}

static void
keys_apply(void)
{
    int i;

    for (i = 0; i < NUM_KEY_GROUPS; i++) {
        g_assert(groups[i].keys_edit != NULL);
        g_assert(groups[i].keys != NULL);
        memcpy(groups[i].keys, groups[i].keys_edit, keys_keys_array_length(groups[i].keys) * sizeof(keys_key));
    }
}

static void
keys_lb_switch(int enablebuttons)
{
    if (enablebuttons) {
        gtk_widget_show(cw_lb1);
        gtk_widget_show(cw_lb2);
        gtk_widget_show(cw_uns);
        gtk_widget_hide(cw_label3);
    } else {
        gtk_widget_hide(cw_lb1);
        gtk_widget_hide(cw_lb2);
        gtk_widget_hide(cw_uns);
        gtk_widget_show(cw_label3);
    }
}

static void
stop_learning(void)
{
    if (capturing) {
        capturing = capturing_all = 0;
        keys_lb_switch(1);
    }
}

static void
keys_response(GtkWidget* dialog, gint response, gpointer data)
{
    stop_learning();

    switch (response) {
    case GTK_RESPONSE_APPLY:
        keys_apply();
        break;
    case GTK_RESPONSE_OK:
        keys_apply();
    default:
        gtk_widget_hide(dialog);
        keys_finish_editing();
        break;
    }
}

static gboolean
keys_encode_assignment(gchar** line,
    guint modifiers,
    gint xkeysym,
    gint hwcode)
{
    if (xkeysym != 0) {
        guint k;

        for (k = 0; k < xkeymaplen; k++) {
            if (xkeymap[k].xname && xkeymap[k].xkeysym == xkeysym) {
                gchar* newline = g_strdup_printf("%s%s%s%s",
                    modifiers & 1 ? "Shift+" : "",
                    modifiers & 2 ? "Ctrl+" : "",
                    modifiers & 4 ? "Meta+" : "",
                    xkeymap[k].xname);
                if (hwcode == -1)
                    *line = newline;
                else {
                    *line = g_strdup_printf("%s %d", newline, hwcode);
                    g_free(newline);
                }
                return TRUE;
            }
        }
    }

    *line = g_strdup_printf(NONE_TEXT);
    return FALSE;
}

static gboolean
keys_decode_assignment(gchar* code,
    KeySym* keysym,
    gint* keycode,
    guint* mod,
    gint* hwcode)
{
    int k;
    gchar** name_hwcode;
    gboolean retcode = FALSE;

    // Decode modifiers from assignment string
    *mod = 0;
    while (1) {
        if (!strncasecmp("shift+", code, 6)) {
            code += 6;
            *mod |= 1;
        } else if (!strncasecmp("ctrl+", code, 5)) {
            code += 5;
            *mod |= 2;
        } else if (!strncasecmp("meta+", code, 5)) {
            code += 5;
            *mod |= 4;
        } else {
            break;
        }
    }

    name_hwcode = g_strsplit(code, " ", 2);

    // Search for X key description and return appropriate KeySym
    for (k = 0; k < xkeymaplen; k++) {
        if (xkeymap[k].xname && !strcmp(xkeymap[k].xname, name_hwcode[0])) {
            *keycode = xmin + k / symspercode;
            *keysym = xkeymap[k].xkeysym;
            if (name_hwcode[1] != NULL)
                *hwcode = atoi(name_hwcode[1]);
            retcode = TRUE;
            break;
        }
    }

    g_strfreev(name_hwcode);
    return retcode;
}

static void
keys_key_group_changed(GtkComboBox* w)
{
    GtkListStore* list_store = GUI_GET_LIST_STORE(cw_list);
    GtkTreeIter iter;
    GtkTreeModel* model;

    guint n = gtk_combo_box_get_active(w);
    keys_key* kpt;
    gchar* line;

    g_assert(n < NUM_KEY_GROUPS);
    cw_currentgroup = -1;

    // Set explanation
    gtk_label_set_text(GTK_LABEL(cw_explabel), gettext(groups[n].explanation));

    model = gui_list_freeze(cw_list);
    gui_list_clear_with_model(model);
    for (kpt = groups[n].keys_edit; kpt->title; kpt++) {
        keys_encode_assignment(&line, kpt->modifiers, kpt->xkeysym, -1);
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0,
            kpt->local_title ? kpt->local_title : _(kpt->title),
            1, line, -1);
        g_free(line);
    }
    gui_list_thaw(cw_list, model);

    cw_currentgroup = n;
    gui_list_select(cw_list, 0, FALSE, 0.0);
}

static void
keys_list_select(GtkTreeSelection* sel)
{
    GtkTreeModel* mdl;
    GtkTreeIter iter;
    gchar* str;
    gint row;

    static gboolean needs_set = TRUE;

    stop_learning();

    if (gtk_tree_selection_get_selected(sel, &mdl, &iter)) {
        guint code, h;
        gchar* active = NULL;
        int mod, i;

        row = atoi(str = gtk_tree_model_get_string_from_iter(mdl, &iter));
        g_free(str);

        if (cw_currentgroup == -1)
            return;

        cw_currentkey = -1;

        // Set explanation
        gtk_label_set_text(GTK_LABEL(cw_explabel2), gettext(groups[cw_currentgroup].keys_edit[row].explanation));

        // Find active combo box entry
        code = 0;
        h = groups[cw_currentgroup].keys_edit[row].xkeysym;
        for (i = 0; h != 0 && i < xkeymaplen; i++) {
            if (xkeymap[i].xkeysym == h) {
                active = xkeymap[i].xname;
                break;
            }
        }

        // Set combo box list
        for (i = 0; i <= xkeymaplen && keyname[i]; i++) {
            if (needs_set) {
                gtk_list_store_append(ls, &iter);
                gtk_list_store_set(ls, &iter, 0, keyname[i], -1);
            }
            if (active && !strcmp(active, keyname[i]))
                code = i;
        }

        if (needs_set)
            needs_set = FALSE;

        gtk_combo_box_set_active(GTK_COMBO_BOX(cw_combo), code);

        // Set modifier toggles
        mod = groups[cw_currentgroup].keys_edit[row].modifiers;
        for (i = 0; i <= 2; i++) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cw_modtoggles[i]), mod & (1 << i));
        }

        cw_currentkey = row;
    }
}

static void
keys_assignment_changed(void)
{
    gchar* line;
    int i, keysym;

    if (cw_currentgroup == -1 || cw_currentkey == -1)
        return;

    groups[cw_currentgroup].keys_edit[cw_currentkey].modifiers = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cw_modtoggles[0]))
        + 2 * gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cw_modtoggles[1]))
        + 4 * gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cw_modtoggles[2]));

    keysym = 0;
    for (i = 0; i < xkeymaplen; i++) {
        if (xkeymap[i].xname && !strcmp(xkeymap[i].xname, keyname[gtk_combo_box_get_active(GTK_COMBO_BOX(cw_combo))])) {
            keysym = xkeymap[i].xkeysym;
            break;
        }
    }

    groups[cw_currentgroup].keys_edit[cw_currentkey].xkeysym = keysym;

    keys_encode_assignment(&line,
        groups[cw_currentgroup].keys_edit[cw_currentkey].modifiers,
        keysym, -1);
    gui_string_list_set_text(cw_list, cw_currentkey, 1, line);
    g_free(line);
}

static inline gboolean
is_modifier_key(int keysym)
{
    /* Regard only left group of modifiers. Hint: all codes from the right group are even */
    return IsModifierKey(keysym) && (keysym & 1);
}

static gboolean
keys_keyevent(GtkWidget* widget,
    GdkEventKey* event)
{
    int keysym = event->keyval;

    if (capturing && !is_modifier_key(keysym)) {
        int mod;
        gchar* line;

        mod = ((event->state & GDK_SHIFT_MASK) != 0)
            + 2 * ((event->state & GDK_CONTROL_MASK) != 0)
            + 4 * ((event->state & GDK_MOD1_MASK) != 0);

        /* If this doesn't succeed, the keymap contains logical errors */
        g_assert(keys_encode_assignment(&line, mod, keysym, -1));

        groups[cw_currentgroup].keys_edit[cw_currentkey].xkeysym = keysym;
        groups[cw_currentgroup].keys_edit[cw_currentkey].hwcode = event->hardware_keycode;
        groups[cw_currentgroup].keys_edit[cw_currentkey].modifiers = mod;

        gui_string_list_set_text(cw_list, cw_currentkey, 1, line);
        g_free(line);

        if (capturing_all) {
            int nextkey = cw_currentkey + 1;
            if (groups[cw_currentgroup].keys_edit[nextkey].title) {
                gui_list_select(cw_list, nextkey, TRUE, 0.5);
            } else {
                keys_list_select(gtk_tree_view_get_selection(GTK_TREE_VIEW(cw_list)));
                capturing = capturing_all = 0;
                keys_lb_switch(1);
            }
        } else {
            keys_list_select(gtk_tree_view_get_selection(GTK_TREE_VIEW(cw_list)));
            capturing = capturing_all = 0;
            keys_lb_switch(1);
        }

        g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
        return TRUE;
    }

    return FALSE;
}

static void
keys_unset_key_clicked(void)
{
    groups[cw_currentgroup].keys_edit[cw_currentkey].xkeysym = 0;
    gui_string_list_set_text(cw_list, cw_currentkey, 1, NONE_TEXT);
}

static void
keys_learn_key_clicked(void)
{
    capturing = 1;
    capturing_all = 0;
    keys_lb_switch(0);
}

static void
keys_learn_all_keys_clicked(void)
{
    capturing = 1;
    capturing_all = 1;
    gui_list_select(cw_list, 0, TRUE, 0.5);
    keys_lb_switch(0);
}

void keys_dialog(void)
{
    static GtkWidget* configwindow = NULL;

    GtkWidget *mainbox, *box1, *box2, *box3, *box4, *thing, *frame, *gc;
    int i;
    const gchar* listtitles[2] = {
        N_("Function"),
        N_("Assignment")
    };

    if (configwindow != NULL) {
        gtk_window_present(GTK_WINDOW(configwindow));
        keys_initialize_editing();
        return;
    }

    configwindow = gtk_dialog_new_with_buttons(_("Keyboard Configuration"), GTK_WINDOW(mainwindow), 0,
        GTK_STOCK_OK, GTK_RESPONSE_OK,
        GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
    gui_dialog_adjust(configwindow, GTK_RESPONSE_OK);
    gui_dialog_connect(configwindow, G_CALLBACK(keys_response));

    mainbox = gtk_dialog_get_content_area(GTK_DIALOG(configwindow));

    keys_initialize_editing();
    capturing = 0;
    capturing_all = 0;

    ls = gtk_list_store_new(1, G_TYPE_STRING);

    // Key Group Selector
    for (i = 0; i < NUM_KEY_GROUPS; i++) {
        GtkTreeIter iter;

        gtk_list_store_append(ls, &iter);
        gtk_list_store_set(ls, &iter, 0, _(groups[i].title), -1);
    }
    gc = gui_combo_new(ls);
    gtk_box_pack_start(GTK_BOX(mainbox), gc, FALSE, FALSE, 0);
    gtk_combo_box_set_active(GTK_COMBO_BOX(gc), 0);
    g_signal_connect(gc, "changed",
        G_CALLBACK(keys_key_group_changed), NULL);
    gtk_widget_show(gc);

    box1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), box1, TRUE, TRUE, 4);

    // List at the left side of the window
    thing = gui_stringlist_in_scrolled_window(2, listtitles, box1, FALSE);
    gtk_widget_set_size_request(thing, 200, 50);
    gui_list_handle_selection(thing, G_CALLBACK(keys_list_select), NULL);
    cw_list = thing;

    box2 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);

    // Explaining Text
    frame = gtk_frame_new(_("Key Group Explanation"));
    gtk_box_pack_start(GTK_BOX(box2), frame, TRUE, TRUE, 0);

    box4 = gtk_vbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(box4), 4);
    gtk_container_add(GTK_CONTAINER(frame), box4);

    cw_explabel = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(cw_explabel), GTK_JUSTIFY_FILL);
    gtk_label_set_line_wrap(GTK_LABEL(cw_explabel), TRUE);
    gtk_box_pack_start(GTK_BOX(box4), cw_explabel, TRUE, TRUE, 0);

    // Explaining Text
    frame = gtk_frame_new(_("Key Explanation"));
    gtk_box_pack_start(GTK_BOX(box2), frame, TRUE, TRUE, 0);

    box4 = gtk_vbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(box4), 4);
    gtk_container_add(GTK_CONTAINER(frame), box4);

    cw_explabel2 = gtk_label_new("");
    gtk_label_set_justify(GTK_LABEL(cw_explabel2), GTK_JUSTIFY_FILL);
    gtk_label_set_line_wrap(GTK_LABEL(cw_explabel2), TRUE);
    gtk_box_pack_start(GTK_BOX(box4), cw_explabel2, TRUE, TRUE, 0);

    // Key Selection Combo Box
    ls = gtk_list_store_new(1, G_TYPE_STRING);
    cw_combo = gui_combo_new(ls);
    /* To set appropriate style property */
    gtk_widget_set_name(cw_combo, "keyconfig_combo");
    gtk_box_pack_start(GTK_BOX(box2), cw_combo, FALSE, FALSE, 4);
    g_signal_connect(cw_combo, "changed",
        G_CALLBACK(keys_assignment_changed), NULL);

    // Modifier Group
    box3 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, FALSE, 0);

    thing = gtk_label_new(_("Modifiers:"));
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, FALSE, 0);

    thing = cw_modtoggles[2] = gtk_check_button_new_with_label("Meta");
    gtk_box_pack_end(GTK_BOX(box3), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(keys_assignment_changed), NULL);

    thing = cw_modtoggles[1] = gtk_check_button_new_with_label("Ctrl");
    gtk_box_pack_end(GTK_BOX(box3), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(keys_assignment_changed), NULL);

    thing = cw_modtoggles[0] = gtk_check_button_new_with_label("Shift");
    gtk_box_pack_end(GTK_BOX(box3), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(keys_assignment_changed), NULL);

    /* Learn / Unset Buttons */
    cw_lb1 = thing = gtk_button_new_with_label(_("Learn selected key"));
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(keys_learn_key_clicked), NULL);

    cw_uns = thing = gtk_button_new_with_label(_("Unset selected key"));
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(keys_unset_key_clicked), NULL);

    cw_lb2 = thing = gtk_button_new_with_label(_("Learn all keys"));
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(keys_learn_all_keys_clicked), NULL);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 4);

    gtk_widget_show_all(mainbox);

    cw_label3 = gtk_label_new(_("Please press the desired key combination!\nClick into left list to cancel"));
    gtk_label_set_justify(GTK_LABEL(cw_label3), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(box2), cw_label3, TRUE, TRUE, 0);

    keys_key_group_changed(GTK_COMBO_BOX(gc));
    keys_list_select(gtk_tree_view_get_selection(GTK_TREE_VIEW(cw_list)));

    g_signal_connect(configwindow, "key_press_event",
        G_CALLBACK(keys_keyevent), NULL);

    gtk_widget_show(configwindow);
}

static void
keys_load_config(void)
{
    gchar **titles = NULL, **values = NULL;
    gsize length, i;
    guint g;
    keys_key* k;
    gboolean processed;

    length = prefs_get_pairs(SECTION, &titles, &values);
    for (i = 0; i < length; i++) {
        processed = FALSE;
        for (g = 0; g < NUM_KEY_GROUPS && !processed; g++) {
            for (k = groups[g].keys; k->title; k++) {
                if (!strcmp(k->title, titles[i])) {
                    if (!keys_decode_assignment(values[i],
                        &k->xkeysym, &k->xkeycode, &k->modifiers, &k->hwcode)) {
                        fprintf(stderr, "*** Can't find key '%s'; removed\n", values[i]);
                        prefs_remove_key(SECTION, titles[i]);
                    }
                    processed = TRUE;
                    break;
                }
            }
        }
        /* Leave keys with unknown functions as is, maybe this config is sent us from future :-) */
    }

    g_strfreev(titles);
    g_strfreev(values);
    return;
}

void keys_save_config(void)
{
    gchar* buf;
    guint g;
    keys_key* k;

    for (g = 0; g < NUM_KEY_GROUPS; g++) {
        for (k = groups[g].keys; k->title; k++) {
            if (k->xkeysym != 0) {
                if (keys_encode_assignment(&buf, k->modifiers, k->xkeysym, k->hwcode))
                    prefs_put_string(SECTION, k->title, buf);
                g_free(buf);
            }
        }
    }
}

static int
find_keysym(KeySym k)
{
    int i;

    for (i = 0; i < xkeymaplen; i++) {
        if (xkeymap[i].xname && xkeymap[i].xkeysym == k)
            return i;
    }

    return -1;
}

static int
keys_try_automatic_config(KeySym first,
    int key_offset,
    int count,
    int note_offset,
    int color,
    keys_key* nk)
{
    int key;

    key = find_keysym(first);
    if (key == -1)
        return 0;

    key += key_offset * symspercode;

    while (count) {
        if (key < 0 || key >= xkeymaplen)
            return 0;

        g_assert(nk->meaning == note_offset);
        nk->xkeysym = xkeymap[key].xkeysym;
        nk->xkeycode = xmin + key / symspercode;
        nk->modifiers = 0;

        if (color == WHITE) {
            switch (note_offset % 12) {
            case 4:
            case 11:
                note_offset += 1;
                nk += 1;
                break;
            default:
                note_offset += 2;
                nk += 2;
                break;
            }
            key += 1 * symspercode;
        } else {
            switch (note_offset % 12) {
            case 3:
            case 10:
                note_offset += 3;
                nk += 3;
                key += 2 * symspercode;
                break;
            default:
                note_offset += 2;
                nk += 2;
                key += 1 * symspercode;
                break;
            }
        }

        count--;
    }

    return 1;
}

/* This function tries to assign channel mute key combinations automatically */
static int
keys_ch_try_automatic_config(KeySym first,
    int key_offset,
    int count,
    keys_key* nk,
    int modifiers)
{
    int key;

    key = find_keysym(first);
    if (key == -1)
        return 0;

    key += key_offset * symspercode;

    while (count) {
        if (key < 0 || key >= xkeymaplen)
            return 0;

        nk->xkeysym = xkeymap[key].xkeysym;
        nk->xkeycode = xmin + key / symspercode;
        nk->modifiers = modifiers;

        nk += 1;
        key += 1 * symspercode;

        count--;
    }

    return 1;
}

static int
keys_qsort_compare_func(const void* string1,
    const void* string2)
{
    const char* s1 = *(char**)string1;
    const char* s2 = *(char**)string2;

    if (!s1)
        return -1;
    return strcmp(s1, s2);
}

static void
keys_make_xkeys(void)
{
    int i, k;

    keyname = g_new0(gchar*, xkeymaplen + 1);

    for (i = 0, k = 1; i < xkeymaplen; i++) {
        if (xkeymap[i].xname && !is_modifier_key(xkeymap[i].xkeysym))
            keyname[k++] = xkeymap[i].xname;
    }

    qsort(keyname + 1, k - 1, sizeof(char*), keys_qsort_compare_func);

    keyname[0] = NONE_TEXT;
}

static void
keys_fixup_xkeymap(void)
{
    /* The problem are custom keymaps that contain lines such as:
           keycode 0x18 =  Q
       instead of a correct
           keycode 0x18 =  q Q
       Fix these.
    */

    int i;
    KeySym k1, k2;
    gchar a[2] = { 0, 0 };

    if (symspercode < 2)
        return;

    for (i = 0; i < xkeymaplen / symspercode; i++) {
        k1 = xkeymap[i * symspercode + 0].xkeysym;
        k2 = xkeymap[i * symspercode + 1].xkeysym;

        if (k2 == 0) {
            if (k1 >= 'A' && k1 <= 'Z') {
                fprintf(stderr, "*** keys_fixup_xkeymap: %c -> ", (int)k1);
                k2 = k1;
                k1 += 'a' - 'A';
                fprintf(stderr, "%c %c\n", (int)k1, (int)k2);

                a[0] = k1;
                xkeymap[i * symspercode + 1].xname = xkeymap[i * symspercode + 0].xname;
                xkeymap[i * symspercode + 0].xname = g_strdup(a);

                xkeymap[i * symspercode + 0].xkeysym = k1;
                xkeymap[i * symspercode + 1].xkeysym = k2;
            }
        }
    }
}

/* This function fills title and explanation fields of channel selecting key combinations */
static void
keys_generate_channel_explanations(keys_key* array,
    int count)
{
    int i = 0;

    for (i = 0; i < count; i++) {
        array[i].title = g_strdup_printf("CH%02d", i + 1);
        array[i].local_title = g_strdup_printf(_("CH%02d"), i + 1);
        array[i].explanation = g_strdup_printf(_("Fast jump to channel %d"), i + 1);
        array[i].meaning = KEYS_MEANING_TYPE_MAKE(KEYS_MEANING_CH) + i;
        array[i].hwcode = -1;
    }
}

int keys_init(void)
{
    static GtkWidget* dialog = NULL;
    int max;
    KeySym* servsyms;
    int i, j;
    GdkDisplay* st_gdk_display = gdk_display_get_default();

    if (!st_gdk_display) {
        fprintf(stderr, "Can't open GDK display.\n");
        return 0;
    }

    x_display = GDK_DISPLAY_XDISPLAY(st_gdk_display);

    XDisplayKeycodes(x_display, &xmin, &max);
    if (xmin < 8 || max > 255) {
        fprintf(stderr, "Sorry, insane X keycode numbers (min/max out of range).\n");
        return 0;
    }

    servsyms = XGetKeyboardMapping(x_display, xmin, max - xmin + 1, &symspercode);
    if (!servsyms) {
        fprintf(stderr, "Can't retrieve X keyboard mapping.\n");
        return 0;
    }

    if (symspercode < 1) {
        fprintf(stderr, "Sorry, can't handle your X keyboard (symspercode < 1).\n");
        return 0;
    }

    xkeymaplen = symspercode * (max - xmin + 1);
    xkeymap = g_new(xkey, xkeymaplen);

    for (i = 0; i < xkeymaplen; i++) {
        char* name = XKeysymToString(servsyms[i]);

        xkeymap[i].xname = NULL;
        xkeymap[i].xkeysym = 0;

        if (name) {
            // Test if this key has already been stored
            for (j = 0; j < i; j++) {
                if (xkeymap[j].xname && !strcmp(xkeymap[j].xname, name)) {
                    break;
                }
            }
            if (j == i) {
                // No, add it
                xkeymap[i].xname = g_strdup(name);
                xkeymap[i].xkeysym = servsyms[i];
            }
        }
    }

    XFree(servsyms);

    keys_fixup_xkeymap();
    keys_make_xkeys();
    keys_generate_channel_explanations(keys3 + KEY_LAST + 1, 32);
    keys3[ARRAY_SIZE(keys3) - 1].title = NULL;

    /* First try automatic config to obtain defaults and than overwrite with user-defined values */
    if (!keys_try_automatic_config('e', -2, 12, 12, WHITE, keys1)
        || !keys_try_automatic_config('x', -1, 10, 0, WHITE, keys2)
        || !keys_try_automatic_config('2', 0, 8, 13, BLACK, keys1 + 1)
        || !keys_try_automatic_config('s', 0, 7, 1, BLACK, keys2 + 1)
        || !keys_ch_try_automatic_config('E', -2, 8, keys3 + KEY_LAST + 1, ENCODE_MODIFIERS(1, 0, 0))
        || !keys_ch_try_automatic_config('S', -1, 8, keys3 + KEY_LAST + 1 + 8, ENCODE_MODIFIERS(1, 0, 0))
        || !keys_ch_try_automatic_config('E', -2, 8, keys3 + KEY_LAST + 1 + 16, ENCODE_MODIFIERS(1, 0, 1))
        || !keys_ch_try_automatic_config('S', -1, 8, keys3 + KEY_LAST + 1 + 24, ENCODE_MODIFIERS(1, 0, 1))) {
        // Automatic key configuration unsuccessful. Popup requester.
        gui_warning_dialog(&dialog, _("Automatic key configuration unsuccessful.\nPlease use the Keyboard Configuration dialog\n"
                                      "in the Settings menu."),
            FALSE);
    }
    keys_load_config();

    return 1;
}

guint32
keys_get_key_meaning(guint32 keysym,
    guint modifiers, gint hwcode)
{
    int g;
    keys_key* k;

    for (g = 0; g < NUM_KEY_GROUPS; g++) {
        for (k = groups[g].keys; k->title; k++) {
            if (k->xkeysym == keysym && k->modifiers == modifiers &&
                (hwcode == -1 || k->hwcode == -1 || k->hwcode == hwcode)) {
                return k->meaning;
            }
        }
    }

    return -1;
}

/* Yeah! Let's fake around X's stupid auto-repeat! X sends a
   KeyRelease before the KeyPress event of an auto-repeat. */
gboolean
keys_is_key_pressed(guint32 keysym,
    int modifiers)
{
    int g;
    keys_key* k;
    char array[32];

    for (g = 0; g < NUM_KEY_GROUPS; g++) {
        for (k = groups[g].keys; k->title; k++) {
            if (k->xkeysym == keysym && k->modifiers == modifiers) {
                XQueryKeymap(x_display, array);
                return 0 != (array[k->xkeycode / 8] & (1 << (k->xkeycode % 8)));
            }
        }
    }

    return FALSE;
}

#else /* !defined(_WIN32) */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include <gdk/gdkx.h>
//#include "X11/Xlib.h"
//#include <X11/keysym.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gui-subs.h"
#include "gui.h"
#include "keys.h"
#include "preferences.h"

char* lowkey = "<>aAyYsSxXcCfFvVgGbBhHnNmMkK,;lL.:";

void keys_dialog(void)
{
}

void keys_save_config(void)
{
}

int keys_init(void)
{
    return 1;
}

guint32
keys_get_key_meaning(guint32 keysym,
    guint modifiers, gint hwcode)

{
    char* c;

    for (c = lowkey; *c; c++)
        if (*c == keysym)
            return (c - lowkey) >> 1;

    return -1;
}

/* Yeah! Let's fake around X's stupid auto-repeat! X sends a
   KeyRelease before the KeyPress event of an auto-repeat. */
gboolean
keys_is_key_pressed(guint32 keysym,
    int modifiers)
{
    return FALSE;
}

#endif /* !defined(_WIN32) */
