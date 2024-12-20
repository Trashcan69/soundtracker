
/*
 * The Real SoundTracker - User activity history (header)
 *
 * Copyright (C) 2019, 2020 Yury Aliaev
 * Copyright (C) 1998-2019 Michael Krause
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

#include <gtk/gtk.h>

typedef enum {
    HISTORY_ACTION_POINTER,
    HISTORY_ACTION_POINTER_NOFREE, /* Don't free argument on history record removal */
    HISTORY_ACTION_INT
} HistoryActionType;

typedef enum {
    HISTORY_STATUS_OK,
    HISTORY_STATUS_COLLATED,
    HISTORY_STATUS_NOMEM
} HistoryStatus;

enum {
    HISTORY_FLAG_COLLATABLE = 1 << 8, /* 8 lowest bits for parameter given through flags */
    HISTORY_FLAG_LOG_PAGE = 1 << 9,
    HISTORY_FLAG_LOG_INS = 1 << 10,
    HISTORY_FLAG_LOG_SMP = 1 << 11,
    HISTORY_FLAG_LOG_POS = 1 << 12,
    HISTORY_FLAG_LOG_PAT = 1 << 13,
    HISTORY_FLAG_FORCE_PAT = 1 << 14, /* Mutually exclusive with _FORCE_PAGE */
    HISTORY_FLAG_FORCE_PAGE = 1 << 15
};

#define HISTORY_FLAG_LOG_ALL (HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | HISTORY_FLAG_LOG_SMP)
#define HISTORY_SET_PAGE(page) (HISTORY_FLAG_FORCE_PAGE | page)
#define HISTORY_SET_PAT(pat) (HISTORY_FLAG_FORCE_PAT | pat)
/* Extra flags can be used for distinguishing between similarly looking actions */
#define HISTORY_EXTRA_FLAGS_MASK 0xffff0000
#define HISTORY_FLAG_PARAMETER_MASK 0xff
#define HISTORY_EXTRA_FLAGS(flags) (flags << 16)

extern gboolean history_skip;

void history_init(GtkBuilder* bd);
void history_clear(const gboolean set_modified);
void history_save(void);
gboolean history_get_modified(void);
gboolean history_check_size(gsize size);
gboolean _history_query_irreversible(GtkWidget *parent, const gchar* message);

static inline gboolean
history_query_oversized(GtkWidget *parent) {
    return _history_query_irreversible(parent,
        N_("The operation requres too much memory.\nYou will not be able to undo it.\n"
           "Do you want to continue?"));
}

static inline gboolean
history_query_irreversible(GtkWidget *parent) {
    return _history_query_irreversible(parent,
        N_("You are about to do some irreversible action.\nYou will not be able to undo it.\n"
           "Do you want to continue?"));
}

/* Checks if the specified action can be collated with the previous logged one */
gboolean history_test_collate(HistoryActionType type,
    const gint flags,
    gpointer data);

HistoryStatus history_log_action_full(HistoryActionType type,
    const gchar* title,
    const gint flags,
    void (*undo_func)(const gint ins, const gint smp, const gboolean redo,
        gpointer arg, gpointer data),
    void (*cleanup_func)(gpointer arg),
    gpointer data,
    gsize arg_size, ...); /* The last argument can have various type */
#define history_log_action(type, title, flags, undo_func, data, arg_size, arg)\
    history_log_action_full(type, title, flags, undo_func, NULL, data, arg_size, arg)

/* Convenience function for most common actions */
void history_log_spin_button(GtkSpinButton* sb,
    const gchar* title,
    const gint flags, /* Collatable is ignored (always TRUE) */
    const gint prev_value);

void history_log_entry(GtkEntry* en,
    const gchar* title,
    const gint maxlen,
    const gint flags, /* Also always collatable */
    const gchar* prev_value);

void history_log_toggle_button(GtkToggleButton* tb,
    const gchar* title,
    const gint flags,
    const gboolean prev_value);

void history_log_radio_group(GtkWidget** group,
    const gchar* title,
    const gint flags,
    const gint prev_value,
    const gint number);
