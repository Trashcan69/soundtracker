
/*
 * The Real SoundTracker - GTK+ Tracker widget (header)
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

#ifndef _TRACKER_H
#define _TRACKER_H

#include "xm.h"
#include "draw-interlayer.h"

#define TRACKER(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, tracker_get_type(), Tracker))
#define TRACKER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST(klass, tracker_get_type(), TrackerClass))
#define IS_TRACKER(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, tracker_get_type()))

typedef struct _Tracker Tracker;
typedef struct _TrackerClass TrackerClass;

struct _Tracker {
    GtkWidget widget;

    int disp_rows;
    int disp_starty;
    int disp_numchans;
    int disp_startx;
    int disp_chanwidth;
    int disp_cursor;

    int fonth, fontw;
    DILayout* layout;
    DIDrawable drawable;

    DIGC bg_gc, bg_cursor_gc, bg_majhigh_gc, bg_minhigh_gc, bars_gc,
        bg_sel_gc, bg_csel_gc, cur_gc, cur_e_gc;
    GdkColor *notes_color, *channums_color;
    int enable_backing_store;
    GdkPixmap* pixmap;
    guint idle_handler;

    XMPattern* curpattern;
    int patpos, oldpos, onlyrow;
    int num_channels;

    int cursor_ch, cursor_item;
    gboolean editing, print_numbers, print_cursor, selectable;
    int leftchan;

    /* Block selection stuff */
    gboolean inSelMode;
    int sel_start_ch, sel_start_row;
    int sel_end_ch, sel_end_row;

    int mouse_selecting;
    int button;
};

struct _TrackerClass {
    GtkWidgetClass parent_class;

    void (*patpos)(Tracker* t, int patpos);
    void (*xpanning)(Tracker* t, int leftchan);
    void (*mainmenu_blockmark_set)(Tracker* t, int state);
    void (*yconf)(Tracker* t, int patpos, int patlen, int disprows);
    void (*xconf)(Tracker* t, int leftchan, int numchans, int dispchans);
    void (*scroll_parameter)(Tracker* t, gint chan, gint item, gint patpos, gint direction);
    void (*alt_scroll)(Tracker* t, gint direction);
};

#define NUM_NOTES 96

GType tracker_get_type(void) G_GNUC_CONST;
GtkWidget* tracker_new(void);

void tracker_set_num_channels(Tracker* t, int);
void tracker_set_editing(Tracker* t, const gboolean is_editing);
void tracker_set_print_numbers(Tracker* t, const gboolean print_numbers);
void tracker_set_print_cursor(Tracker* t, const gboolean print_cursor);
void tracker_set_selectable(Tracker* t, const gboolean selectable);
void tracker_set_backing_store(Tracker* t, int on);
gboolean tracker_set_font(Tracker* t, const gchar* fontname);

const gchar* tracker_get_note_name(const guint key);

void tracker_reset(Tracker* t);
void tracker_redraw(Tracker* t);
void tracker_redraw_row(Tracker* t, int row);
void tracker_redraw_current_row(Tracker* t);

/* These are the navigation functions. */
void tracker_step_cursor_item(Tracker* t, int direction);
void tracker_step_cursor_channel(Tracker* t, int direction);
void tracker_set_cursor_channel(Tracker* t, int channel);
void tracker_set_cursor_item(Tracker* t, int item);
void tracker_set_patpos(Tracker* t, int row);
void tracker_step_cursor_row(Tracker* t, int direction);

void tracker_set_pattern(Tracker* t, XMPattern* pattern);
void tracker_set_xpanning(Tracker* t, int left_channel);

/* Set, get, clear selection markers */
void tracker_mark_selection(Tracker* t, gboolean enable);
void tracker_clear_mark_selection(Tracker* t);
void tracker_get_selection_rect(Tracker* t, int* chStart, int* rowStart, int* nChannel, int* nRows);
gboolean tracker_is_valid_selection(Tracker* t);
gboolean tracker_is_in_selection_mode(Tracker* t);

#endif /* _TRACKER_H */
