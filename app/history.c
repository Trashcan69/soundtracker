
/*
 * The Real SoundTracker - User activity history
 *
 * Copyright (C) 2019-2021 Yury Aliaev
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

#include <stdarg.h>

#include <glib/gi18n.h>

#include "gui-settings.h"
#include "gui.h"
#include "history.h"

typedef struct {
    HistoryActionType type;
    const gchar* title;
    gint page, ins, smp, pos, pat;
    gint extra_flags;
    union {
        gpointer arg_pointer;
        gint iarg;
    } arg;
    gsize arg_size;
    void (*undo_func)(const gint ins, const gint smp, const gboolean redo,
        gpointer arg, gpointer data);
    void (*cleanup_func)(gpointer arg);
    gpointer data;
} Action;

static GQueue historique = G_QUEUE_INIT;
static GList *current = NULL, *saved = NULL;

static GtkWidget *undo_menu, *redo_menu;

static gsize free_size = -1;
static gboolean force_modified = FALSE, in_history = FALSE;
gboolean history_skip = FALSE;

static void
update_menus(void)
{
    Action* element;
    GList* prev;
    gchar* label;

    if (current) {
        element = current->data;
        label = g_strdup_printf("%s: %s", _("_Undo"), element->title);
        gtk_menu_item_set_label(GTK_MENU_ITEM(undo_menu), label);
        g_free(label);
    } else
        gtk_menu_item_set_label(GTK_MENU_ITEM(undo_menu), _("_Undo"));
    gtk_widget_set_sensitive(undo_menu, current != NULL);

    prev = current ? current->prev : historique.tail;
    if (prev) {
        element = prev->data;
        label = g_strdup_printf("%s: %s", _("_Redo"), element->title);
        gtk_menu_item_set_label(GTK_MENU_ITEM(redo_menu), label);
        g_free(label);
    } else
        gtk_menu_item_set_label(GTK_MENU_ITEM(redo_menu), _("_Redo"));
    gtk_widget_set_sensitive(redo_menu, prev != NULL);
}

void
history_init(GtkBuilder* bd)
{
    free_size = gui_settings.undo_size << 20;

    undo_menu = gui_get_widget(bd, "edit_undo", UI_FILE);
    gtk_widget_set_sensitive(undo_menu, FALSE);
    redo_menu = gui_get_widget(bd, "edit_redo", UI_FILE);
    gtk_widget_set_sensitive(redo_menu, FALSE);
}

void
history_clear(const gboolean set_modified)
{
    Action* element;

    for (element = g_queue_pop_head(&historique);
        element;
        element = g_queue_pop_head(&historique)) {
        if (element->type == HISTORY_ACTION_POINTER) {
            if (element->cleanup_func)
                element->cleanup_func(element->arg.arg_pointer);
            g_free(element->arg.arg_pointer);
        }
        g_free(element);
    }
    current = NULL;
    saved = NULL;
    force_modified = set_modified;
    free_size = gui_settings.undo_size << 20;

    update_menus();
    gui_update_title(NULL);
}

void
history_save(void)
{
    saved = current;
    force_modified = FALSE;
    gui_update_title(NULL);
}

gboolean
history_get_modified(void)
{
    return force_modified || saved != current;
}

static void
undo_redo_common(Action* element, const gboolean redo)
{
    in_history = TRUE;

    if (element->page != -1)
        gui_go_to_page(element->page);
    if (element->ins != -1)
        gui_set_current_instrument(element->ins);
    if (element->smp != -1)
        gui_set_current_sample(element->smp);
    if (element->pos != -1)
        gui_set_current_position(element->pos);
    if (element->pat != -1)
        gui_set_current_pattern(element->pat, TRUE);

    /* We need all idle functions to do their work before
       changing some values */
    while (gtk_events_pending())
        gtk_main_iteration();

    switch(element->type) {
    case HISTORY_ACTION_POINTER:
    case HISTORY_ACTION_POINTER_NOFREE:
        element->undo_func(element->ins, element->smp, redo, element->arg.arg_pointer, element->data);
        break;
    case HISTORY_ACTION_INT:
        element->undo_func(element->ins, element->smp, redo, &(element->arg.iarg), element->data);
        break;
    default:
        g_assert_not_reached();
    }
    update_menus();
    gui_update_title(NULL);

    in_history = FALSE;
}

void
history_undo(void)
{
    Action* element;

    if (!current)
        return;

    element = current->data;
    current = current->next;
    undo_redo_common(element, FALSE);
}

void
history_redo(void)
{
    if (current) {
        if (!current->prev)
            return;
        current = current->prev;
    } else {
        if (!historique.tail)
            return;
        current = historique.tail;
    }

    undo_redo_common(current->data, TRUE);
}

static void
selection_changed(GtkTreeSelection* sel, gint* current_row)
{
    gint row = gui_list_get_selection_index(sel);

    if (row < *current_row) {
        for(; row < *current_row; (*current_row)--)
            history_undo();
    } else if (row > *current_row) {
        for(; row > *current_row; (*current_row)++)
            history_redo();
    }
}

void
history_show_dialog(void)
{
    static GtkWidget *dialog = NULL, *history_list;
    static GtkListStore* ls;
    static GtkTreeSelection* sel;
    static gint tag, current_row;
    GtkTreeIter iter, sel_iter;
    GtkTreeModel* tm;
    GList* l;
    gint i;
    const gchar* titles[] = {N_("Operation"), N_("Saved")};
    GType types[] = {G_TYPE_STRING, G_TYPE_STRING};
    const gfloat alignments[] = {0.0, 0.5};
    const gboolean expands[] = {FALSE, FALSE};

    if (!dialog) {
        dialog = gtk_dialog_new_with_buttons(_("Undo History"), GTK_WINDOW(mainwindow),
            GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
        gui_dialog_connect(dialog, NULL);
        gui_dialog_adjust(dialog, GTK_RESPONSE_CLOSE);

        history_list = gui_list_in_scrolled_window_full(2, titles,
            gtk_dialog_get_content_area(GTK_DIALOG(dialog)),
            types, alignments, expands, GTK_SELECTION_BROWSE, TRUE, TRUE,
            NULL, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
        sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(history_list));
        ls = GUI_GET_LIST_STORE(history_list);
        tag = g_signal_connect_after(sel, "changed",
            G_CALLBACK(selection_changed), &current_row);

        gtk_widget_set_size_request(history_list, -1, 100);
        gtk_widget_show_all(dialog);
    }

    g_signal_handler_block(G_OBJECT(sel), tag);
    gui_list_clear(history_list);
    tm = gui_list_freeze(history_list);
    gtk_list_store_append(ls, &iter);
    gtk_list_store_set(ls, &iter, 0, _("[Initial State]"),
        1, (saved == NULL && !force_modified) ? "*" : "", -1);
    if (!current) {
        sel_iter = iter;
        current_row = 0;
    }
    for (i = 1, l = historique.tail; l; l = l->prev, i++) {
        gtk_list_store_append(ls, &iter);
        gtk_list_store_set(ls, &iter, 0, ((Action *)l->data)->title,
            1, (saved == l && !force_modified) ? "*" : "", -1);
        if (l == current) {
            sel_iter = iter;
            current_row = i;
        }
    }
    gui_list_thaw(history_list, tm);

    gtk_tree_selection_select_iter(sel, &sel_iter);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(history_list),
        gtk_tree_model_get_path(tm, &sel_iter), NULL, TRUE, 0.5, 0.5);
    g_signal_handler_unblock(G_OBJECT(sel), tag);

    gtk_window_present(GTK_WINDOW(dialog));
}

gboolean
history_test_collate(HistoryActionType type,
    const gint flags,
    gpointer data)
{
    gint ins, smp, pos, pat;
    Action* element;

    if (history_skip || in_history)
        return TRUE;
    if (historique.head != current)
        return FALSE;
    element = g_queue_peek_head(&historique);
    if (!element)
        return FALSE;

    /* After the saved state we have to begin a new operation */
    if (current == saved)
        return FALSE;

    ins = (flags & HISTORY_FLAG_LOG_INS) ? gui_get_current_instrument() : -1;
    smp = (flags & HISTORY_FLAG_LOG_SMP) ? gui_get_current_sample() : -1;
    pos = (flags & HISTORY_FLAG_LOG_POS) ? gui_get_current_position() : -1;
    pat = (flags & HISTORY_FLAG_LOG_PAT) ? gui_get_current_pattern() : -1;
    if (element->type == type && element->data == data &&
        element->ins == ins && element->smp == smp &&
        element->pos == pos && element->pat == pat &&
        element->extra_flags == (flags & HISTORY_EXTRA_FLAGS_MASK))
        return TRUE;

    return FALSE;
}

HistoryStatus
history_log_action_full(HistoryActionType type,
    const gchar* title,
    const gint flags,
    void (*undo_func)(const gint ins, const gint smp, const gboolean redo,
        gpointer arg, gpointer data),
    void (*cleanup_func)(gpointer arg),
    gpointer data,
    gsize arg_size, ...) /* The last argument can have various type */
{
    Action* element;
    va_list ap;
    gboolean collatable = flags & HISTORY_FLAG_COLLATABLE;
    gint ins, smp, pos, pat;

    /* Sanity check to make debugging easier */
    g_assert(history_check_size(arg_size));

    if (history_skip || in_history)
        return HISTORY_STATUS_OK;

    /* Current state is not the history head.
       Elements newer than current state should be removed */
    while (historique.head != current) {
        /* Only the newest action in the list can be collated with the current one */
        collatable = FALSE;
        if (historique.head == saved) {
            /* Saved state is lost */
            saved = NULL;
            force_modified = TRUE;
        }
        element = g_queue_pop_head(&historique);
        free_size += element->arg_size;
        if (element->type == HISTORY_ACTION_POINTER) {
            if (element->cleanup_func)
                element->cleanup_func(element->arg.arg_pointer);
            g_free(element->arg.arg_pointer);
        }
        g_free(element);
    }

    ins = (flags & HISTORY_FLAG_LOG_INS) ? gui_get_current_instrument() : -1;
    smp = (flags & HISTORY_FLAG_LOG_SMP) ? gui_get_current_sample() : -1;
    pos = (flags & HISTORY_FLAG_LOG_POS) ? gui_get_current_position() : -1;
    if (flags & HISTORY_FLAG_FORCE_PAT)
        pat = flags & HISTORY_FLAG_PARAMETER_MASK;
    else
        pat = (flags & HISTORY_FLAG_LOG_PAT) ? gui_get_current_pattern() : -1;
    element = g_queue_peek_head(&historique);
    if (collatable && element && current != saved)
        if (element->type == type && element->data == data &&
            element->ins == ins && element->smp == smp &&
            element->pos == pos && element->pat == pat &&
            element->extra_flags == (flags & HISTORY_EXTRA_FLAGS_MASK))
        /* Collation, nothing to do */
            return HISTORY_STATUS_COLLATED;

    if (arg_size > gui_settings.undo_size << 20) {
        /* Argument is too big, undo is not possible.
           Caller must check size before logging to avoid this case
           otherwise argument has to be freed */
        force_modified = TRUE;
        return HISTORY_STATUS_NOMEM;
    }
    /* Freeing oldest elements if necessary */
    while (free_size < arg_size) {
        if (historique.tail == saved)
            /* Saved state is lost */
            saved = NULL;
        /* If saved state has already been NULL (initial state), it
           anyway will be lost if any tail element will be deleted */
        if (!saved)
            force_modified = TRUE;

        element = g_queue_pop_tail(&historique);
        free_size += element->arg_size;
        if (element->type == HISTORY_ACTION_POINTER) {
            if (element->cleanup_func)
                element->cleanup_func(element->arg.arg_pointer);
            g_free(element->arg.arg_pointer);
        }
        g_free(element);
    }

    element = g_new(Action, 1);
    element->type = type;
    element->title = title;
    element->extra_flags = flags & HISTORY_EXTRA_FLAGS_MASK;
    if (flags & HISTORY_FLAG_FORCE_PAGE)
        element->page = flags & HISTORY_FLAG_PARAMETER_MASK;
    else
        element->page = (flags & HISTORY_FLAG_LOG_PAGE) ? notebook_current_page : -1;
    element->ins = ins;
    element->smp = smp;
    element->pos = pos;
    element->pat = pat;

    va_start(ap, arg_size);
    switch (type) {
    case HISTORY_ACTION_POINTER:
    case HISTORY_ACTION_POINTER_NOFREE:
        element->arg.arg_pointer = va_arg(ap, gpointer);
        break;
    case HISTORY_ACTION_INT:
        element->arg.iarg = va_arg(ap, gint);
        break;
    default:
        g_assert_not_reached();
    }
    va_end(ap);

    element->arg_size = arg_size;
    element->undo_func = undo_func;
    element->cleanup_func = cleanup_func;
    element->data = data;
    g_queue_push_head(&historique, element);
    free_size -= element->arg_size;

    current = historique.head;
    update_menus();
    gui_update_title(NULL);
    return HISTORY_STATUS_OK;
}

gboolean
history_check_size(gsize size)
{
    return size <= gui_settings.undo_size << 20;
}

gboolean
_history_query_irreversible(GtkWidget *parent, const gchar* message)
{
    gboolean res = gui_ok_cancel_modal(parent, _(message));

    if (res) {
        /* Provided that some irreversible action will be done.
           Since this point all previous changes cannot be reverted. */
        history_clear(TRUE);
    }

    return res;
}

/* Most often used logging and undo/redo functions */

static void
spin_button_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    gint tmp_value;
    gint* value = arg;

    g_assert(GTK_IS_SPIN_BUTTON(data));

    tmp_value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(data));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(data), *value);
    *value = tmp_value;
}

void
history_log_spin_button(GtkSpinButton* sb,
    const gchar* title,
    const gint flags,
    const gint prev_value)
{
    history_log_action(HISTORY_ACTION_INT, title, flags | HISTORY_FLAG_COLLATABLE, spin_button_undo,
        sb, 0, prev_value);
}

static void
toggle_button_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    gboolean tmp_value;
    gboolean* value = arg;

    g_assert(GTK_IS_TOGGLE_BUTTON(data));

    tmp_value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data), *value);
    *value = tmp_value;
}

void
history_log_toggle_button(GtkToggleButton* tb,
    const gchar* title,
    const gint flags,
    const gboolean prev_value)
{
    history_log_action(HISTORY_ACTION_INT, title, flags, toggle_button_undo,
        tb, 0, prev_value);
}

struct EntryArg {
    gint maxlen;
    gchar data[1];
};

static void
entry_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    gchar* tmp_value;
    struct EntryArg* ea = arg;

    g_assert(GTK_IS_ENTRY(data));

    tmp_value = alloca(ea->maxlen);
    strncpy(tmp_value, gtk_entry_get_text(GTK_ENTRY(data)), ea->maxlen);
    gtk_entry_set_text(GTK_ENTRY(data), ea->data);
    strncpy(ea->data, tmp_value, ea->maxlen);
}

void
history_log_entry(GtkEntry* en,
    const gchar* title,
    const gint maxlen,
    const gint flags,
    const gchar* prev_value)
{
    const gsize arg_size = sizeof(struct EntryArg) + sizeof(gchar) * maxlen - 1;
    struct EntryArg* arg;

    if (history_test_collate(HISTORY_ACTION_POINTER, flags, en))
        return;

    arg = g_malloc(arg_size);
    arg->maxlen = maxlen;
    strncpy(arg->data, prev_value, maxlen);
    history_log_action(HISTORY_ACTION_POINTER, title, flags, entry_undo,
        en, arg_size, arg);
}

static void
radio_group_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    gint *index = arg;
    GtkWidget** buttons = data;
    gint tmp_value = find_current_toggle(buttons, *index >> 16);

    g_assert(GTK_IS_TOGGLE_BUTTON(buttons[0]));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(buttons[*index & 0xffff]), TRUE);
    *index = (*index & 0xffff0000) | tmp_value;
}

void
history_log_radio_group(GtkWidget** group,
    const gchar* title,
    const gint flags,
    const gint prev_value,
    const gint number)
{
    history_log_action(HISTORY_ACTION_INT, title, flags,
        /* I (yaliaev) don't want to allocate devoted argument in heap, so
           I store 2 integers just inside the history queue provided that
           we have less than 2^16 radio buttons in a group :-) */
        radio_group_undo, group, 0, prev_value | number << 16);
}
