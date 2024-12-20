
/*
 * The Real SoundTracker - track editor (header)
 *
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

#ifndef _TRACK_EDITOR_H
#define _TRACK_EDITOR_H

#include <gtk/gtk.h>

#include "tracker-settings.h"
#include "tracker.h"

typedef enum {
    SELECTION_CLASSIC = 0,
    SELECTION_FT2,
    SELECTION_MIXED,
    SELECTION_LAST
} SelectionMode;

#define MASK_COLS 4
#define MASK_ROWS 5

enum {
    MASK_CUT = 0,
    MASK_NOTE = 0,
    MASK_INSTR = 1,
    MASK_VOLUME = 2,
    MASK_FXTYPE = 3,
    MASK_FXPARAM = 4,
    MASK_COPY = 5,
    MASK_PASTE = 10,
    MASK_TRANSP = 15,
    MASK_ENABLE = 1 << (MASK_COLS * MASK_ROWS)
};

/* Note, Insrument, Volume, Eff type, Eff parameter */
static const guint cpt_masks[] = {
    1, 1 << 1, 1 << 2, 1 << 3, 1 << 4, /* Cut */
    1 << 5, 1 << 6, 1 << 7, 1 << 8, 1 << 9, /* Copy */
    1 << 10, 1 << 11, 1 << 12, 1 << 13, 1 << 14, /* Paste */
    1 << 15, 1 << 16, 1 << 17, 1 << 18, 1 << 19 /* Transparency */
};

extern Tracker* tracker;
extern GtkWidget* trackersettings;
extern GtkWidget* vscrollbar;

void tracker_page_create(GtkNotebook* nb);

gboolean track_editor_handle_keys(int shift,
    int ctrl,
    int alt,
    guint32 keyval,
    gboolean pressed);
gboolean track_editor_do_the_note_key(int note,
    gboolean pressed,
    guint32 xkeysym,
    int modifiers,
    gboolean always_poly);
void track_editor_toggle_jazz_edit(GtkCheckMenuItem* b);

void track_editor_set_num_channels(int n);
void track_editor_set_patpos(gint row);

void track_editor_load_config(void);
void track_editor_save_config(void);

/* Handling of real-time scrolling */
void tracker_start_updating(void);
void tracker_stop_updating(void);
void tracker_set_update_freq(int);

/* c'n'p operations */

void track_editor_copy_pattern(GtkWidget* w, Tracker* t);
void track_editor_cut_pattern(GtkWidget* w, Tracker* t);
void track_editor_paste_pattern(GtkWidget* w, Tracker* t);
void track_editor_copy_track(GtkWidget* w, Tracker* t);
void track_editor_cut_track(GtkWidget* w, Tracker* t);
void track_editor_paste_track(GtkWidget* w, Tracker* t);
void track_editor_delete_track(GtkWidget* w, Tracker* t);
void track_editor_insert_track(GtkWidget* w, Tracker* t);
void track_editor_kill_notes_track(GtkWidget* w, Tracker* t);
void track_editor_mark_selection(Tracker* t);
void track_editor_clear_mark_selection(GtkWidget* w, Tracker* t);
void track_editor_copy_selection(GtkWidget* w, Tracker* t);
void track_editor_cut_selection(GtkWidget* w, Tracker* t);
void track_editor_paste_selection(GtkWidget* w, Tracker* t, gboolean advance_cursor);

void track_editor_interpolate_fx(GtkWidget* w, Tracker* t);

void track_editor_cmd_mvalue(Tracker* t, gboolean mode);

void track_editor_toggle_permanentness(Tracker* t, gboolean all);

/* History of operations */
void track_editor_log_block(Tracker* t, const gchar* title,
    const gint channel, const gint pos, const gint width, const gint length);
void track_editor_log_note(Tracker* t, const gchar* title,
    const gint pattern, const gint channel, const gint row,
    const gint flags);

#endif /* _TRACK_EDITOR_H */
