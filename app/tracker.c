
/*
 * The Real SoundTracker - GTK+ Tracker widget
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

#include <stdio.h>
#include <string.h>

#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "colors.h"
#include "gui-settings.h"
#include "main.h"
#include "marshal.h"
#include "st-subs.h"
#include "tracker.h"

static const char* const notenames[4][NUM_NOTES] = {{
    "C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "H-0",
    "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "H-1",
    "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "H-2",
    "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "H-3",
    "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "H-4",
    "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "H-5",
    "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6", "G#6", "A-6", "A#6", "H-6",
    "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "A-7", "A#7", "H-7"
},{
    "C-0", "Db0", "D-0", "Eb0", "E-0", "F-0", "Gb0", "G-0", "Ab0", "A-0", "B-0", "H-0",
    "C-1", "Db1", "D-1", "Eb1", "E-1", "F-1", "Gb1", "G-1", "Ab1", "A-1", "B-1", "H-1",
    "C-2", "Db2", "D-2", "Eb2", "E-2", "F-2", "Gb2", "G-2", "Ab2", "A-2", "B-2", "H-2",
    "C-3", "Db3", "D-3", "Eb3", "E-3", "F-3", "Gb3", "G-3", "Ab3", "A-3", "B-3", "H-3",
    "C-4", "Db4", "D-4", "Eb4", "E-4", "F-4", "Gb4", "G-4", "Ab4", "A-4", "B-4", "H-4",
    "C-5", "Db5", "D-5", "Eb5", "E-5", "F-5", "Gb5", "G-5", "Ab5", "A-5", "B-5", "H-5",
    "C-6", "Db6", "D-6", "Eb6", "E-6", "F-6", "Gb6", "G-6", "Ab6", "A-6", "B-6", "H-6",
    "C-7", "Db7", "D-7", "Eb7", "E-7", "F-7", "Gb7", "G-7", "Ab7", "A-7", "B-7", "H-7"
},{
    "C-0", "C#0", "D-0", "D#0", "E-0", "F-0", "F#0", "G-0", "G#0", "A-0", "A#0", "B-0",
    "C-1", "C#1", "D-1", "D#1", "E-1", "F-1", "F#1", "G-1", "G#1", "A-1", "A#1", "B-1",
    "C-2", "C#2", "D-2", "D#2", "E-2", "F-2", "F#2", "G-2", "G#2", "A-2", "A#2", "B-2",
    "C-3", "C#3", "D-3", "D#3", "E-3", "F-3", "F#3", "G-3", "G#3", "A-3", "A#3", "B-3",
    "C-4", "C#4", "D-4", "D#4", "E-4", "F-4", "F#4", "G-4", "G#4", "A-4", "A#4", "B-4",
    "C-5", "C#5", "D-5", "D#5", "E-5", "F-5", "F#5", "G-5", "G#5", "A-5", "A#5", "B-5",
    "C-6", "C#6", "D-6", "D#6", "E-6", "F-6", "F#6", "G-6", "G#6", "A-6", "A#6", "B-6",
    "C-7", "C#7", "D-7", "D#7", "E-7", "F-7", "F#7", "G-7", "G#7", "A-7", "A#7", "B-7"
},{
    "C-0", "Db0", "D-0", "Eb0", "E-0", "F-0", "Gb0", "G-0", "Ab0", "A-0", "Bb0", "B-0",
    "C-1", "Db1", "D-1", "Eb1", "E-1", "F-1", "Gb1", "G-1", "Ab1", "A-1", "Bb1", "B-1",
    "C-2", "Db2", "D-2", "Eb2", "E-2", "F-2", "Gb2", "G-2", "Ab2", "A-2", "Bb2", "B-2",
    "C-3", "Db3", "D-3", "Eb3", "E-3", "F-3", "Gb3", "G-3", "Ab3", "A-3", "Bb3", "B-3",
    "C-4", "Db4", "D-4", "Eb4", "E-4", "F-4", "Gb4", "G-4", "Ab4", "A-4", "Bb4", "B-4",
    "C-5", "Db5", "D-5", "Eb5", "E-5", "F-5", "Gb5", "G-5", "Ab5", "A-5", "Bb5", "B-5",
    "C-6", "Db6", "D-6", "Eb6", "E-6", "F-6", "Gb6", "G-6", "Ab6", "A-6", "Bb6", "B-6",
    "C-7", "Db7", "D-7", "Eb7", "E-7", "F-7", "Gb7", "G-7", "Ab7", "A-7", "Bb7", "B-7"
}};

static void init_display(Tracker* t, int width, int height, gboolean signals);

enum {
    SIG_PATPOS,
    SIG_XPANNING,
    SIG_MAINMENU_BLOCKMARK_SET,
    SIG_XCONF,
    SIG_YCONF,
    SIG_CSCROLL,
    SIG_ASCROLL,
    LAST_SIGNAL
};

#define CLEAR(win, x, y, w, h)                               \
    do {                                                     \
        di_draw_rectangle(win, t->bg_gc, TRUE, x, y, w, h); \
    } while (0)

static guint tracker_signals[LAST_SIGNAL] = { 0 };

static gint tracker_idle_draw_function(Tracker* t);

static void
tracker_idle_draw(Tracker* t)
{
    if (!t->idle_handler) {
        t->idle_handler = g_idle_add((GSourceFunc)tracker_idle_draw_function,
            (gpointer)t);
        g_assert(t->idle_handler != 0);
    }
}

const gchar*
tracker_get_note_name(const guint key)
{
    guint index = (gui_settings.sharp ? 0 : 1) + (gui_settings.bh ? 2 : 0);
    g_assert(key < NUM_NOTES);
    return notenames[index][key];
}

void tracker_set_num_channels(Tracker* t,
    int n)
{
    GtkWidget* widget = GTK_WIDGET(t);

    t->num_channels = n;
    if (gtk_widget_get_realized(widget)) {
        init_display(t, widget->allocation.width, widget->allocation.height, FALSE);
        gtk_widget_queue_draw(widget);
    }
    g_signal_emit(G_OBJECT(t), tracker_signals[SIG_XCONF], 0, t->leftchan, t->num_channels, t->disp_numchans);
}

void tracker_set_editing(Tracker* t,
    const gboolean is_editing)
{
    t->editing = is_editing;
    tracker_redraw_current_row(t);
}

void tracker_set_print_numbers(Tracker* t,
    const gboolean print_numbers)
{
    t->print_numbers = print_numbers;
}

void tracker_set_print_cursor(Tracker* t,
    const gboolean print_cursor)
{
    t->print_cursor = print_cursor;
}

void tracker_set_selectable(Tracker* t,
    const gboolean selectable)
{
    t->selectable = selectable;
}

void tracker_set_patpos(Tracker* t,
    int row)
{
    g_return_if_fail(t != NULL);

    /* Special case of empty buffer */
    if (!t->curpattern->length)
        return;

    g_return_if_fail((t->curpattern == NULL && row == 0) || (row < t->curpattern->length));

    if (t->patpos != row) {
        t->patpos = row;
        if (t->curpattern != NULL) {
            if (t->inSelMode) {
                /* Force re-draw of patterns in block selection mode */
                gtk_widget_queue_draw(GTK_WIDGET(t));
            } else {
                tracker_idle_draw(t);
            }
            g_signal_emit(G_OBJECT(t), tracker_signals[SIG_PATPOS], 0, row);
        }
    }
}

void tracker_redraw(Tracker* t)
{
    gtk_widget_queue_draw(GTK_WIDGET(t));
}

void tracker_redraw_row(Tracker* t,
    int row)
{
    t->onlyrow = row;
    tracker_redraw(t);
}

void tracker_redraw_current_row(Tracker* t)
{
    tracker_redraw_row(t, t->patpos);
}

void tracker_set_pattern(Tracker* t,
    XMPattern* pattern)
{
    g_return_if_fail(t != NULL);

    if (t->curpattern != pattern) {
        t->curpattern = pattern;
        if (pattern != NULL) {
            if (!pattern->length)
                /* Special case of empty buffer */
                t->patpos = 0;
            else if (t->patpos >= pattern->length)
                t->patpos = pattern->length - 1;
            g_signal_emit(G_OBJECT(t), tracker_signals[SIG_YCONF], 0, t->patpos, pattern->length, t->disp_rows);
            gtk_widget_queue_draw(GTK_WIDGET(t));
        }
    }
}

void tracker_set_backing_store(Tracker* t,
    int on)
{
    GtkWidget* widget;

    g_return_if_fail(t != NULL);

    if (on == t->enable_backing_store)
        return;

    t->enable_backing_store = on;

    widget = GTK_WIDGET(t);
    if (gtk_widget_get_realized(widget)) {
        if (on) {
            t->pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
            t->drawable = di_get_drawable(t->pixmap);
            CLEAR(t->drawable, 0, 0, widget->allocation.width, widget->allocation.height);
            di_layout_set_drawable(t->layout, t->pixmap);
        } else {
            g_object_unref(t->pixmap);
            t->pixmap = NULL;
            CLEAR(di_get_drawable(widget->window), 0, 0, widget->allocation.width, widget->allocation.height);
        }

        di_gc_set_exposures(t->bg_gc, !on);
        gtk_widget_queue_draw(GTK_WIDGET(t));
    }
}

static void tracker_set_xpanning_full(Tracker* t,
    int left_channel, gboolean signal)
{
    GtkWidget* widget;

    g_return_if_fail(t != NULL);

    if (t->leftchan != left_channel && t->num_channels) {
        widget = GTK_WIDGET(t);
        if (gtk_widget_get_realized(widget)) {
            g_return_if_fail(left_channel + t->disp_numchans <= t->num_channels);

            t->leftchan = left_channel;
            gtk_widget_queue_draw(GTK_WIDGET(t));

            if (t->cursor_ch < t->leftchan)
                t->cursor_ch = t->leftchan;
            else if (t->cursor_ch >= t->leftchan + t->disp_numchans)
                t->cursor_ch = t->leftchan + t->disp_numchans - 1;
        }
        if (signal)
            g_signal_emit(G_OBJECT(t), tracker_signals[SIG_XPANNING], 0, t->leftchan);
    }
}

void tracker_set_xpanning(Tracker* t,
    int left_channel)
{
    tracker_set_xpanning_full(t, left_channel, FALSE);
}

static void
adjust_xpanning(Tracker* t, gboolean signal)
{
    if (t->cursor_ch < t->leftchan)
        tracker_set_xpanning_full(t, t->cursor_ch, signal);
    else if (t->cursor_ch >= t->leftchan + t->disp_numchans)
        tracker_set_xpanning_full(t, t->cursor_ch - t->disp_numchans + 1, signal);
    else if (t->leftchan + t->disp_numchans > t->num_channels)
        tracker_set_xpanning_full(t, t->num_channels - t->disp_numchans, signal);
}

void tracker_step_cursor_item(Tracker* t,
    int direction)
{
    t->cursor_item += direction;
    if (t->cursor_item & (~7)) {
        t->cursor_item &= 7;
        tracker_step_cursor_channel(t, direction > 0 ? 1 : -1);
    } else
        gtk_widget_queue_draw(GTK_WIDGET(t));
}

void tracker_set_cursor_item(Tracker* t,
    int item)
{
    if (item < 0)
        item = 0;
    if (item > 7)
        item = 7;

    t->cursor_item = item;
    gtk_widget_queue_draw(GTK_WIDGET(t));
}

void tracker_step_cursor_channel(Tracker* t,
    int direction)
{
    t->cursor_ch += direction;

    if (t->cursor_ch < 0)
        t->cursor_ch = t->num_channels - 1;
    else if (t->cursor_ch >= t->num_channels)
        t->cursor_ch = 0;

    adjust_xpanning(t, TRUE);

    if (t->inSelMode) {
        /* Force re-draw of patterns in block selection mode */
        gtk_widget_queue_draw(GTK_WIDGET(t));
    } else {
        tracker_idle_draw(t);
    }
}

void tracker_set_cursor_channel(Tracker* t,
    int channel)
{
    t->cursor_ch = channel;

    if (t->cursor_ch < 0)
        t->cursor_ch = 0;
    else if (t->cursor_ch >= t->num_channels)
        t->cursor_ch = t->num_channels - 1;

    adjust_xpanning(t, TRUE);

    if (t->inSelMode) {
        /* Force re-draw of patterns in block selection mode */
        gtk_widget_queue_draw(GTK_WIDGET(t));
    } else {
        tracker_idle_draw(t);
    }
}

void tracker_step_cursor_row(Tracker* t,
    int direction)
{
    int newpos = t->patpos + direction;

    while (newpos < 0)
        newpos += t->curpattern->length;
    newpos %= t->curpattern->length;

    tracker_set_patpos(t, newpos);
}

void tracker_mark_selection(Tracker* t,
    gboolean enable)
{
    if (!enable) {
        t->sel_end_ch = t->cursor_ch;
        t->sel_end_row = t->patpos;
        t->inSelMode = FALSE;
    } else {
        t->sel_start_ch = t->sel_end_ch = t->cursor_ch;
        t->sel_start_row = t->sel_end_row = t->patpos;
        t->inSelMode = TRUE;
        tracker_redraw(t);
    }
}

void tracker_clear_mark_selection(Tracker* t)
{
    if (t->sel_start_ch != -1) {
        t->sel_start_ch = t->sel_end_ch = -1;
        t->sel_start_row = t->sel_end_row = -1;
        t->inSelMode = FALSE;
        tracker_redraw(t);
    }
}

gboolean
tracker_is_in_selection_mode(Tracker* t)
{
    return t->inSelMode;
}

void tracker_get_selection_rect(Tracker* t,
    int* chStart,
    int* rowStart,
    int* nChannel,
    int* nRows)
{
    if (!t->inSelMode) {
        if (t->sel_start_ch <= t->sel_end_ch) {
            *nChannel = t->sel_end_ch - t->sel_start_ch + 1;
            *chStart = t->sel_start_ch;
        } else {
            *nChannel = t->sel_start_ch - t->sel_end_ch + 1;
            *chStart = t->sel_end_ch;
        }
        if (t->sel_start_row <= t->sel_end_row) {
            *nRows = t->sel_end_row - t->sel_start_row + 1;
            *rowStart = t->sel_start_row;
        } else {
            *nRows = t->sel_start_row - t->sel_end_row + 1;
            *rowStart = t->sel_end_row;
        }
    } else {
        if (t->sel_start_ch <= t->cursor_ch) {
            *nChannel = t->cursor_ch - t->sel_start_ch + 1;
            *chStart = t->sel_start_ch;
        } else {
            *nChannel = t->sel_start_ch - t->cursor_ch + 1;
            *chStart = t->cursor_ch;
        }
        if (t->sel_start_row <= t->patpos) {
            *nRows = t->patpos - t->sel_start_row + 1;
            *rowStart = t->sel_start_row;
        } else {
            *nRows = t->sel_start_row - t->patpos + 1;
            *rowStart = t->patpos;
        }
    }
}

gboolean
tracker_is_valid_selection(Tracker* t)
{
    return (t->sel_start_ch >= 0 && t->sel_start_ch < t->num_channels && t->sel_end_ch >= 0 && t->sel_end_ch < t->num_channels && t->sel_start_row >= 0 && t->sel_start_row < t->curpattern->length && t->sel_end_row >= 0 && t->sel_end_row < t->curpattern->length);
}

#define PUT_WIDE(buf, pos, symbol) memcpy(&buf[pos], symbol, sizeof(symbol) - 1);\
    pos += sizeof(symbol) - 1;

static guint
note2string(XMNote* note,
    char* buf)
{
    static const char hexmapU[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
    };
    static const char hexmapL[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
        'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'
    };
    static const char uparrow[] = "\342\206\221", downarrow[] = "\342\206\223",
        dtilde[] = "\342\211\210", notesym[] = "\342\231\251";

    const char* hexmap = gui_settings.tracker_upcase ? hexmapU : hexmapL;

    guint i = 0;
    guchar eff = note->volume >> 4;

    if (note->note > 0 && note->note < XM_PATTERN_NOTE_OFF) {
        int index = (gui_settings.sharp ? 0 : 1) + (gui_settings.bh ? 2 : 0);
        buf[i++] = notenames[index][note->note - 1][0];
        buf[i++] = notenames[index][note->note - 1][1];
        buf[i++] = notenames[index][note->note - 1][2];
    } else if (note->note == XM_PATTERN_NOTE_OFF) {
        buf[i++] = '[';
        buf[i++] = '-';
        buf[i++] = ']';
    } else {
        buf[i++] = gui_settings.tracker_line_format[0];
        buf[i++] = gui_settings.tracker_line_format[1];
        buf[i++] = gui_settings.tracker_line_format[2];
    }

    // Instrument number always displayed in Dec, because the spin
    // buttons use Dec as well.
    if (note->instrument >= 100) {
        buf[i++] = '1';
    } else {
        buf[i++] = ' ';
    }

    if (!note->instrument) {
        buf[i++] = gui_settings.tracker_line_format[3];
        buf[i++] = gui_settings.tracker_line_format[4];
    } else {
        buf[i++] = ((note->instrument / 10) % 10) + '0';
        buf[i++] = (note->instrument % 10) + '0';
    }

    buf[i++] = ' ';

    if (gui_settings.tracker_ft2_volume) {
        if (gui_settings.tracker_vol_dec &&
            note->volume >= 0x10 && note->volume <= 0x50) {
            /* 3 because of trailing \0 which will bw overwritten by futher symbols */
            g_snprintf(&buf[i], 3, "%02d", note->volume - 0x10);
            i += 2;
        } else if (eff == 0) {
            buf[i++] = gui_settings.tracker_line_format[5];
            buf[i++] = gui_settings.tracker_line_format[6];
        } else {
            gboolean use_wide = gui_settings.tracker_ft2_wide;

            switch (eff) {
            case 1 ... 5:
                buf[i++] = eff - 1 + '0';
                break;
            case 6:
                buf[i++] = '-';
                break;
            case 7:
                buf[i++] = '+';
                break;
            case 8:
                if (use_wide) {
                    PUT_WIDE(buf, i, downarrow);
                } else
                    buf[i++] = 'd';
                break;
            case 9:
                if (use_wide) {
                    PUT_WIDE(buf, i, uparrow);
                } else
                    buf[i++] = 'u';
                break;
            case 0xa: /* Set vibrato speed */
                buf[i++] = 's';
                break;
            case 0xb: /* Vibrato */
                if (use_wide) {
                    PUT_WIDE(buf, i, dtilde);
                } else
                    buf[i++] = 'v';
                break;
            case 0xc: /* Panning */
                buf[i++] = 'p';
                break;
            case 0xd: /* Pan left */
                buf[i++] = '<'; /* Looks better than left arrow */
                break;
            case 0xe: /* Pan right */
                buf[i++] = '>';
                break;
            case 0xf: /* Note porta */
                if (use_wide && !gui_settings.tracker_ft2_tpm) {
                    PUT_WIDE(buf, i, notesym);
                } else
                    buf[i++] = 'm';
                break;
            }
            buf[i++] = hexmap[note->volume & 15];
        }
    } else {
        if (note->volume) {
            buf[i++] = hexmap[eff];
            buf[i++] = hexmap[note->volume & 15];
        } else {
            buf[i++] = gui_settings.tracker_line_format[5];
            buf[i++] = gui_settings.tracker_line_format[6];
        }
    }

    buf[i++] = ' ';

    if (!note->fxtype && !note->fxparam) {
        buf[i++] = gui_settings.tracker_line_format[7];
        buf[i++] = gui_settings.tracker_line_format[8];
        buf[i++] = gui_settings.tracker_line_format[9];
    } else {
        /* Emulate FT2 behaviour on probably broken files */
        buf[i++] = hexmap[note->fxtype < 36 ? note->fxtype : 0];
        buf[i++] = hexmap[note->fxparam >> 4];
        buf[i++] = hexmap[note->fxparam & 15];
    }

    buf[i++] = ' ';

    return i;
}

static void
tracker_clear_notes_line(GtkWidget* widget,
    DIDrawable win,
    int y,
    int pattern_row)
{
    Tracker* t = TRACKER(widget);
    DIGC gc;

    gc = t->bg_gc;

    if (pattern_row == t->patpos) {
        gc = t->bg_cursor_gc; // cursor line
    } else if (gui_settings.highlight_rows) {
        if (pattern_row % gui_settings.highlight_rows_n == 0) {
            gc = t->bg_majhigh_gc; // highlighted line
        } else if (pattern_row % gui_settings.highlight_rows_minor_n == 0) {
            gc = t->bg_minhigh_gc; // minor highlighted line
        }
    }

    di_draw_rectangle(win, gc, TRUE, 0, y, widget->allocation.width, t->fonth);
}

static void
print_notes_line(GtkWidget* widget,
    DIDrawable win,
    int y,
    int ch,
    int numch,
    int row)
{
    Tracker* t = TRACKER(widget);
    static char buf[32 * 18];
    gint bufpt;
    int xBlock, BlockWidth, rowBlockStart, rowBlockEnd, chBlockStart, chBlockEnd;

    g_return_if_fail(ch + numch <= t->num_channels);

    tracker_clear_notes_line(widget, win, y, row);

    /* -- Draw selection highlighting if necessary -- */
    /* Calc starting and ending rows */
    if (t->inSelMode) {
        if (t->sel_start_row < t->patpos) {
            rowBlockStart = t->sel_start_row;
            rowBlockEnd = t->patpos;
        } else {
            rowBlockEnd = t->sel_start_row;
            rowBlockStart = t->patpos;
        }
    } else if (t->sel_start_row < t->sel_end_row) {
        rowBlockStart = t->sel_start_row;
        rowBlockEnd = t->sel_end_row;
    } else {
        rowBlockEnd = t->sel_start_row;
        rowBlockStart = t->sel_end_row;
    }

    if (row >= rowBlockStart && row <= rowBlockEnd) {
        /* Calc bar origin and size */
        if (t->inSelMode) {
            if (t->sel_start_ch <= t->cursor_ch) {
                chBlockStart = t->sel_start_ch;
                chBlockEnd = t->cursor_ch;
            } else {
                chBlockStart = t->cursor_ch;
                chBlockEnd = t->sel_start_ch;
            }
        } else if (t->sel_start_ch <= t->sel_end_ch) {
            chBlockStart = t->sel_start_ch;
            chBlockEnd = t->sel_end_ch;
        } else {
            chBlockStart = t->sel_end_ch;
            chBlockEnd = t->sel_start_ch;
        }

        xBlock = t->disp_startx + (chBlockStart - ch) * t->disp_chanwidth;
        if (xBlock < 0 && chBlockEnd >= ch) {
            xBlock = t->disp_startx;
        }
        BlockWidth = (chBlockEnd - (ch > chBlockStart ? ch : chBlockStart) + 1) * t->disp_chanwidth;
        if (BlockWidth > numch * t->disp_chanwidth) {
            BlockWidth = (numch - (chBlockStart > ch ? chBlockStart - ch : 0)) * t->disp_chanwidth;
        }

        /* Draw only if in bounds */
        if (xBlock >= t->disp_startx && xBlock < t->disp_startx + numch * t->disp_chanwidth)
            di_draw_rectangle(win, row == t->patpos ? t->bg_csel_gc : t->bg_sel_gc, TRUE, xBlock - 1, y, BlockWidth, t->fonth);
    }

    /* -- Draw the actual row contents -- */
    /* The row number */
    if (gui_settings.tracker_hexmode) {
        if (gui_settings.tracker_upcase)
            g_sprintf(buf, "%02X", row);
        else
            g_sprintf(buf, "%02x", row);
    } else {
        g_sprintf(buf, "%03d", row);
    }
    di_draw_text(t->layout, t->notes_color, buf, -1, 5, y);

    /* The notes */
    for (numch += ch, bufpt = 0; ch < numch; ch++) {
        bufpt += note2string(&t->curpattern->channels[ch][row], &buf[bufpt]);
    }
    buf[bufpt] = '\0';
    di_draw_text(t->layout, t->notes_color, buf, -1, t->disp_startx, y);
}

static void
print_notes_and_bars(GtkWidget* widget,
    DIDrawable win,
    int x,
    int y,
    int w,
    int h,
    int cursor_row)
{
    int scry;
    int n, n1, n2;
    int my;
    Tracker* t = TRACKER(widget);
    int i, x1, y1;

    /* Limit y and h to the actually used window portion */
    my = y - t->disp_starty;
    if (my < 0) {
        my = 0;
        h += my;
    }
    if (my + h > t->fonth * t->disp_rows) {
        h = t->fonth * t->disp_rows - my;
    }

    /* Calculate first and last line to be redrawn */
    n1 = my / t->fonth;
    n2 = (my + h - 1) / t->fonth;

    /* Print the notes */
    scry = t->disp_starty + n1 * t->fonth;
    for (i = n1; i <= n2; i++, scry += t->fonth) {
        n = i + cursor_row - t->disp_cursor;
        if (n >= 0 && n < t->curpattern->length) {
            print_notes_line(widget, win, scry, t->leftchan, t->disp_numchans, n);
        } else {
            CLEAR(win, 0, scry, widget->allocation.width, t->fonth);
        }
    }

    /* Draw the separation bars */
    x1 = t->disp_startx - 2;
    y1 = t->disp_starty + n1 * t->fonth;
    h = (n2 - n1 + 2) * t->fonth;
    for (i = 0; i <= t->disp_numchans; i++, x1 += t->disp_chanwidth) {
        di_draw_line(win, t->bars_gc, x1, y1, x1, y1 + h - t->fonth);
    }
}

static void
print_channel_numbers(GtkWidget* widget, DIDrawable win)
{
    int x, x1, y, i;
    char buf[5];
    Tracker* t = TRACKER(widget);

    x = t->disp_startx + (t->disp_chanwidth - (2 * t->fontw)) / 2;
    y = t->disp_starty - t->fonth;
    di_draw_rectangle(win, t->bg_gc, TRUE, 0, y - 1, widget->allocation.width, t->fonth + 1);

    x1 = t->disp_startx - 2;

    for (i = 1; i <= t->disp_numchans; i++, x += t->disp_chanwidth, x1 += t->disp_chanwidth) {
        g_sprintf(buf, "%2d", i + t->leftchan);
        if (gui_settings.permanent_channels & (1 << (i + t->leftchan - 1)))
            strcat(buf, "*");
        di_draw_text(t->layout, t->channums_color, buf, -1, x, y);
        /* The beginnings of the separation bars */
        di_draw_line(win, t->bars_gc, x1, y - 1, x1, t->disp_starty);
    }
    di_draw_line(win, t->bars_gc, x1, y - 1, x1, t->disp_starty);
}

static void
print_cursor(GtkWidget* widget,
    DIDrawable win)
{
    int x, y, width;
    Tracker* t = TRACKER(widget);

    g_return_if_fail(t->cursor_ch >= t->leftchan && t->cursor_ch < t->leftchan + t->disp_numchans);
    g_return_if_fail((unsigned)t->cursor_item <= 7);

    width = 1;
    x = 0;
    y = t->disp_starty + t->disp_cursor * t->fonth;

    switch (t->cursor_item) {
    case 0: /* note */
        width = 3;
        break;
    case 1: /* instrument 0 */
        x = 4;
        break;
    case 2: /* instrument 1 */
        x = 5;
        break;
    case 3: /* volume 0 */
        x = 7;
        break;
    case 4: /* volume 1 */
        x = 8;
        break;
    case 5: /* effect 0 */
        x = 10;
        break;
    case 6: /* effect 1 */
        x = 11;
        break;
    case 7: /* effect 2 */
        x = 12;
        break;
    default:
        g_assert_not_reached();
        break;
    }

    x = x * t->fontw + t->disp_startx + (t->cursor_ch - t->leftchan) * t->disp_chanwidth - 1;

    di_draw_rectangle(win, t->editing ? t->cur_e_gc : t->cur_gc,
        FALSE, x, y, width * t->fontw + 1, t->fonth - 1);
}

static void
tracker_draw_clever(GtkWidget* widget,
    GdkRectangle* area)
{
    Tracker* t;
    DIDrawable win, pmap;
    gint fonth;

    g_return_if_fail(widget != NULL);

    if (!gtk_widget_get_visible(widget))
        return;

    t = TRACKER(widget);
    g_return_if_fail(t->curpattern != NULL);

    win = di_get_drawable(widget->window);
    if (t->enable_backing_store)
        pmap = t->drawable;
    else {
        pmap = win;
        di_layout_set_drawable(t->layout, widget->window);
    }
    if (!t->curpattern->length) {
        CLEAR(pmap, area->x, area->y, area->width, area->height);
        if (t->enable_backing_store) {
            di_draw_drawable(win, t->bg_gc, pmap,
                area->x, area->y,
                area->x, area->y,
                area->width, area->height);
        }
        return;
    }

    fonth = t->fonth;

    if (t->onlyrow >= 0) {
        gint i;
        gint x1 = t->disp_startx - 2;
        gint y1 = t->disp_starty + (t->disp_cursor - t->oldpos + t->onlyrow) * fonth;

        print_notes_line(widget, pmap, y1,
            t->leftchan, t->disp_numchans, t->onlyrow);
        for (i = 0; i <= t->disp_numchans; i++, x1 += t->disp_chanwidth)
            di_draw_line(pmap, t->bars_gc, x1, y1, x1, y1 + fonth);
        if (t->onlyrow == t->patpos)
            print_cursor(widget, pmap);

        if (t->enable_backing_store) {
            di_draw_drawable(win, t->bg_gc, pmap,
                area->x, y1,
                area->x, y1,
                area->width, fonth);
        }

        t->onlyrow = -1;
    } else {
        gint dist, absdist;
        gint y, redrawcnt;

        dist = t->patpos - t->oldpos;
        absdist = ABS(dist);
        t->oldpos = t->patpos;

        redrawcnt = t->disp_rows;
        y = t->disp_starty;

        if (absdist <= t->disp_cursor) {
            /* Before scrolling, redraw cursor line in the old picture;
           better than scrolling first and then redrawing the old
           cursor line (prevents flickering) */
            print_notes_and_bars(widget, pmap,
                0, (t->disp_cursor) * fonth + t->disp_starty,
                widget->allocation.width, fonth, t->oldpos - dist);
        }

        if (absdist < t->disp_rows) {
            /* this is interesting. we don't have to redraw the whole area, we can optimize
           by scrolling around instead. */
            if (dist > 0) {
                /* go down in pattern -- scroll up */
                redrawcnt = absdist;
                di_draw_drawable(pmap, t->bg_gc, pmap,
                    0, y + (absdist * fonth), 0, y,
                    widget->allocation.width, (t->disp_rows - absdist) * fonth);
                y += (t->disp_rows - absdist) * fonth;
            } else if (dist < 0) {
                /* go up in pattern -- scroll down */
                if (1 /* gui_settings.channel_numbering */) {
                    /* Redraw line displaying the channel numbers before scrolling down */
                    print_notes_and_bars(widget, pmap,
                        0, t->disp_starty,
                        widget->allocation.width, fonth, t->oldpos - dist);
                }
                redrawcnt = absdist;
                di_draw_drawable(pmap, t->bg_gc, pmap,
                    0, y, 0, y + (absdist * fonth),
                    widget->allocation.width, (t->disp_rows - absdist) * fonth);
            }
        }

        if (dist != 0) {
            print_notes_and_bars(widget, pmap, 0, y, widget->allocation.width, redrawcnt * fonth, t->oldpos);
            if (t->print_numbers)
                print_channel_numbers(widget, pmap);
        }

        /* update the cursor */
        print_notes_and_bars(widget, pmap,
            0, t->disp_cursor * fonth + t->disp_starty,
            widget->allocation.width, fonth, t->oldpos);
        if (t->print_cursor)
            print_cursor(widget, pmap);

        if (t->enable_backing_store) {
            di_draw_drawable(win, t->bg_gc, pmap,
                area->x, area->y,
                area->x, area->y,
                area->width, area->height);
        }
    }
}

static void
tracker_draw_stupid(GtkWidget* widget,
    GdkRectangle* area)
{
    Tracker* t = TRACKER(widget);

    if (t->onlyrow < 0)
        t->oldpos = -666;
    tracker_draw_clever(widget, area);
}

static gint
tracker_expose(GtkWidget* widget,
    GdkEventExpose* event)
{
    tracker_draw_stupid(widget, &event->area);
    return FALSE;
}

static gint
tracker_idle_draw_function(Tracker* t)
{
    GtkWidget* widget = GTK_WIDGET(t);
    GdkRectangle area = { 0, 0, widget->allocation.width, widget->allocation.height };

    if (gtk_widget_get_mapped(GTK_WIDGET(t))) {
        tracker_draw_clever(GTK_WIDGET(t), &area);
    }

    t->idle_handler = 0;
    return FALSE;
}

static void
tracker_size_request(GtkWidget* widget,
    GtkRequisition* requisition)
{
    Tracker* t = TRACKER(widget);
    requisition->width = 14 * t->fontw + 3 * t->fontw + 10;
    requisition->height = 11 * t->fonth;
}

static void
init_display(Tracker* t,
    int width,
    int height,
    gboolean signals)
{
    GtkWidget* widget = GTK_WIDGET(t);
    int u;
    int line_numbers_space = 3 * t->fontw;

    if (!gui_settings.tracker_hexmode) {
        line_numbers_space += 1 * t->fontw; // Line numbers take up more space in decimal mode
    }

    t->disp_rows = height / t->fonth;
    if (!(t->disp_rows % 2))
        t->disp_rows--;
    t->disp_cursor = t->disp_rows / 2;
    t->disp_starty = (height - t->fonth * t->disp_rows) / 2;
    if (t->print_numbers) {
        t->disp_starty += t->fonth;
        t->disp_rows--;
    }

    u = width - line_numbers_space - 10;
    t->disp_numchans = u / t->disp_chanwidth;

    if (t->disp_numchans > t->num_channels)
        t->disp_numchans = t->num_channels;

    t->disp_startx = (u - t->disp_numchans * t->disp_chanwidth) / 2 + line_numbers_space + 5;
    adjust_xpanning(t, FALSE);

    if (t->curpattern && signals) {
        g_signal_emit(G_OBJECT(t), tracker_signals[SIG_YCONF], 0, t->patpos, t->curpattern->length, t->disp_rows);
        g_signal_emit(G_OBJECT(t), tracker_signals[SIG_XCONF], 0, t->leftchan, t->num_channels, t->disp_numchans);
    }

    if (t->enable_backing_store) {
        if (t->pixmap) {
            g_object_unref(t->pixmap);
        }
        t->pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
        t->drawable = di_get_drawable(t->pixmap);
        CLEAR(t->drawable, 0, 0, widget->allocation.width, widget->allocation.height);
        di_layout_set_drawable(t->layout, t->pixmap);
    } else
        CLEAR(di_get_drawable(widget->window), 0, 0, widget->allocation.width, widget->allocation.height);
}

static void
tracker_size_allocate(GtkWidget* widget,
    GtkAllocation* allocation)
{
    Tracker* t;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(IS_TRACKER(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;
    if (gtk_widget_get_realized(widget)) {
        t = TRACKER(widget);

        gdk_window_move_resize(widget->window,
            allocation->x, allocation->y,
            allocation->width, allocation->height);

        init_display(t, allocation->width, allocation->height, TRUE);
    }
}

void tracker_reset(Tracker* t)
{
    GtkWidget* widget;

    g_return_if_fail(t != NULL);

    widget = GTK_WIDGET(t);
    if (gtk_widget_get_realized(widget)) {
        t->patpos = 0;
        t->cursor_ch = 0;
        t->cursor_item = 0;
        t->leftchan = 0;
        init_display(t, widget->allocation.width, widget->allocation.height, TRUE);
        gtk_widget_queue_draw(GTK_WIDGET(t));
    }
}

static void
tracker_realize(GtkWidget* widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;
    Tracker* t;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(IS_TRACKER(widget));

    gtk_widget_set_realized(widget, TRUE);
    t = TRACKER(widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(widget->parent->window, &attributes, attributes_mask);

    widget->style = gtk_style_attach(widget->style, widget->window);

    /* We need a particular context since we may need to set exposures for it */
    t->bg_gc = colors_new_gc(COLOR_BG, widget->window);
    colors_add_widget(COLOR_BG, widget);
    /* For other contexts we don't need anything special */
    t->bg_cursor_gc = colors_get_gc(COLOR_BG_CURSOR);
    t->bg_majhigh_gc = colors_get_gc(COLOR_BG_MAJHIGH);
    t->bg_minhigh_gc = colors_get_gc(COLOR_BG_MINHIGH);
    t->notes_color = colors_get_color(COLOR_NOTES);
    t->bg_sel_gc = colors_get_gc(COLOR_BG_SELECTION);
    t->bg_csel_gc = colors_get_gc(COLOR_BG_CURSOR_SEL);
    t->channums_color = colors_get_color(COLOR_CHANNUMS);
    t->cur_gc = colors_get_gc(COLOR_CURSOR);
    t->cur_e_gc = colors_get_gc(COLOR_CURSOR_EDIT);
    t->bars_gc = colors_get_gc(COLOR_BARS);

    if (!t->enable_backing_store)
        di_gc_set_exposures(t->bg_gc, 1); /* man XCopyArea, grep exposures */

    init_display(t, attributes.width, attributes.height, TRUE);

    gdk_window_set_user_data(widget->window, widget);
    gdk_window_set_background(widget->window, colors_get_color(COLOR_BG));
}

gboolean
tracker_set_font(Tracker* t,
    const gchar* fontname)
{
    g_debug("tracker_set_font(%s)", fontname);

    int fonth, fontw, chanwidth;

    if (!di_layout_set_font(t->layout, fontname))
        return FALSE;

    const gchar dummy[] = "--- 00 00 000 ";
    di_layout_get_pixel_size(t->layout, dummy, -1, &chanwidth, &fonth);
    // apparently there is no simple way to find out at this point whether the
    // font is monospace, so just assume we're lucky...
    fontw = chanwidth / (ARRAY_SIZE(dummy) - 1);

    /* Some fonts have width 0, for example 'clearlyu' */
    if (fonth >= 1 && fontw >= 1) {
        t->fontw = fontw;
        t->fonth = fonth;
        t->disp_chanwidth = chanwidth;
        tracker_reset(t);
        return TRUE;
    }

    return FALSE;
}

typedef void (*___Sig1)(Tracker*, gint, gint, gint, gpointer);

static void
___marshal_Sig1(GClosure* closure,
    GValue* return_value,
    guint n_param_values,
    const GValue* param_values,
    gpointer invocation_hint,
    gpointer marshal_data)
{
    register ___Sig1 callback;
    register GCClosure* cc = (GCClosure*)closure;
    register gpointer data1, data2;

    g_return_if_fail(n_param_values == 4);

    if (G_CCLOSURE_SWAP_DATA(closure)) {
        data1 = closure->data;
        data2 = g_value_peek_pointer(param_values + 0);
    } else {
        data1 = g_value_peek_pointer(param_values + 0);
        data2 = closure->data;
    }

    callback = (___Sig1)(marshal_data != NULL ? marshal_data : cc->callback);

    callback((Tracker*)data1,
        (gint)g_value_get_int(param_values + 1),
        (gint)g_value_get_int(param_values + 2),
        (gint)g_value_get_int(param_values + 3),
        data2);
}

typedef void (*___Sig2)(Tracker*, gint, gpointer);

static void
___marshal_Sig2(GClosure* closure,
    GValue* return_value,
    guint n_param_values,
    const GValue* param_values,
    gpointer invocation_hint,
    gpointer marshal_data)
{
    register ___Sig2 callback;
    register GCClosure* cc = (GCClosure*)closure;
    register gpointer data1, data2;

    g_return_if_fail(n_param_values == 2);

    if (G_CCLOSURE_SWAP_DATA(closure)) {
        data1 = closure->data;
        data2 = g_value_peek_pointer(param_values + 0);
    } else {
        data1 = g_value_peek_pointer(param_values + 0);
        data2 = closure->data;
    }

    callback = (___Sig2)(marshal_data != NULL ? marshal_data : cc->callback);

    callback((Tracker*)data1,
        (gint)g_value_get_int(param_values + 1),
        data2);
}

/* If selecting, mouse is used to select in pattern */
static gboolean
tracker_mouse_to_cursor_pos(Tracker* t,
    int x,
    int y,
    int* cursor_ch,
    int* cursor_item,
    int* patpos)
{
    int HPatHalf;
    gboolean exact = FALSE;

    /* Calc the column (channel and pos in channel) */
    if (x < t->disp_startx) {
        if (t->leftchan)
            *cursor_ch = t->leftchan - 1;
        else
            *cursor_ch = t->leftchan;
        *cursor_item = 0;
    } else if (x > t->disp_startx + t->disp_numchans * t->disp_chanwidth) {
        if (t->leftchan + t->disp_numchans < t->num_channels) {
            *cursor_ch = t->leftchan + t->disp_numchans;
            *cursor_item = 0;
        } else {
            *cursor_ch = t->num_channels - 1;
            *cursor_item = 7;
        }
    } else {
        *cursor_ch = t->leftchan + ((x - t->disp_startx) / t->disp_chanwidth);
        *cursor_item = (x - (t->disp_startx + (*cursor_ch - t->leftchan) * t->disp_chanwidth)) / t->fontw;
        switch (*cursor_item) {
        case 0 ... 2:
            exact = TRUE;
        case 3:
            *cursor_item = 0;
            break;
        case 4:
            exact = TRUE;
            *cursor_item = 1;
            break;
        case 5:
            exact = TRUE;
        case 6:
            *cursor_item = 2;
            break;
        case 7:
            exact = TRUE;
            *cursor_item = 3;
            break;
        case 8:
            exact = TRUE;
        case 9:
            *cursor_item = 4;
            break;
        case 10:
            exact = TRUE;
            *cursor_item = 5;
            break;
        case 11:
            exact = TRUE;
            *cursor_item = 6;
            break;
        case 12:
            exact = TRUE;
        default:
            *cursor_item = 7;
            break;
        }
    }

    /* Calc the row */
    HPatHalf = t->disp_rows / 2;
    if (y < t->disp_starty) {
        *patpos = t->patpos - HPatHalf - 1;
        exact = FALSE;
    } else if ((y - t->disp_starty) > t->disp_rows * t->fonth) {
        *patpos = t->patpos + HPatHalf + 1;

        if (y > (t->disp_rows + 1) * t->fonth)
            exact = FALSE;
    } else {
        *patpos = (y - t->disp_starty) / t->fonth;
        if (t->patpos <= *patpos)
            *patpos = t->patpos + *patpos - HPatHalf;
        else
            *patpos = t->patpos - (HPatHalf - *patpos);
    }
    if (*patpos < 0) {
        *patpos = 0;
        exact = FALSE;
    } else if (*patpos >= t->curpattern->length) {
        *patpos = t->curpattern->length - 1;
        exact = FALSE;
    }

    return exact;
}

static gboolean
tracker_scroll(GtkWidget* widget,
    GdkEventScroll* event)
{
    Tracker* t;
    t = TRACKER(widget);

    if (event->state & GDK_CONTROL_MASK) {
        gint direction;
        gboolean is_shift = event->state & GDK_SHIFT_MASK;

        if (event->direction == GDK_SCROLL_UP)
            direction = is_shift ? -10 : -1;
        else if (event->direction == GDK_SCROLL_DOWN)
            direction = is_shift ? 10 : 1;
        else
            return TRUE;

        g_signal_emit(G_OBJECT(t), tracker_signals[SIG_CSCROLL], 0, direction);
        return TRUE;
    }

    if (event->state & GDK_MOD1_MASK) {
        GdkModifierType state;
        gint x, y, cur_ch, cur_item, patpos;

        gdk_window_get_pointer(event->window, &x, &y, &state);
        if (tracker_mouse_to_cursor_pos(t, x, y, &cur_ch, &cur_item, &patpos)) {
            gint direction;

            if (event->direction == GDK_SCROLL_UP)
                direction = 1;
            else if (event->direction == GDK_SCROLL_DOWN)
                direction = -1;
            else
                return TRUE;

            g_signal_emit(G_OBJECT(t), tracker_signals[SIG_ASCROLL], 0,
                cur_ch, cur_item, patpos, direction);
        }
        return TRUE;
    }

    switch (event->direction) {
    case GDK_SCROLL_UP:
        if (event->state & GDK_SHIFT_MASK) {
            if (t->leftchan > 0)
                tracker_set_xpanning_full(t, t->leftchan - 1, TRUE);
        } else if (t->patpos > 0)
            tracker_step_cursor_row(t, -1);
        return TRUE;
    case GDK_SCROLL_DOWN:
        if (event->state & GDK_SHIFT_MASK) {
            if (t->leftchan + t->disp_numchans < t->num_channels)
                tracker_set_xpanning_full(t, t->leftchan + 1, TRUE);
        } else if (t->patpos < t->curpattern->length - 1)
            tracker_step_cursor_row(t, 1);
        return TRUE;
    case GDK_SCROLL_LEFT: /* For happy folks with 2-scroller mice :-) */
        if (t->leftchan > 0)
            tracker_set_xpanning_full(t, t->leftchan - 1, TRUE);
        return TRUE;
    case GDK_SCROLL_RIGHT:
        if (t->leftchan + t->disp_numchans < t->num_channels)
            tracker_set_xpanning_full(t, t->leftchan + 1, TRUE);
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

static gint
tracker_button_press(GtkWidget* widget,
    GdkEventButton* event)
{
    Tracker* t;
    int x, y, cursor_ch, cursor_item, patpos, redraw = 0;
    GdkModifierType state;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(IS_TRACKER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    t = TRACKER(widget);
    gdk_window_get_pointer(event->window, &x, &y, &state);

    if (t->mouse_selecting && event->button != 1) {
        t->mouse_selecting = 0;
    } else if (!t->mouse_selecting) {
        t->button = event->button;
        gdk_window_get_pointer(event->window, &x, &y, &state);
        if (t->button == 1 && t->selectable) {
            if ((state & GDK_SHIFT_MASK) && (t->sel_start_ch >= 0) && (t->sel_start_row >= 0)) {
                /* Extend existing block */
                gint tmp;

                if (t->sel_start_ch > t->sel_end_ch) {
                    tmp = t->sel_start_ch;
                    t->sel_start_ch = t->sel_end_ch;
                    t->sel_end_ch = tmp;
                }
                if (t->sel_start_row > t->sel_end_row) {
                    tmp = t->sel_start_row;
                    t->sel_start_row = t->sel_end_row;
                    t->sel_end_row = tmp;
                }
                tracker_mouse_to_cursor_pos(t, x, y, &t->sel_end_ch, &cursor_item, &t->sel_end_row);
            } else {
                /* Start selecting block */
                g_signal_emit(G_OBJECT(t), tracker_signals[SIG_MAINMENU_BLOCKMARK_SET], 0, 1);
                tracker_mouse_to_cursor_pos(t, x, y, &t->sel_start_ch, &cursor_item, &t->sel_start_row);
                t->sel_end_row = t->sel_start_row;
                t->sel_end_ch = t->sel_start_ch;
            }
            t->inSelMode = FALSE;
            t->mouse_selecting = 1;
            tracker_redraw(t);
        } else if (t->button == 2) {
            /* Tracker cursor posititioning and clear block mark if any */
            if (t->inSelMode || t->sel_start_ch != -1) {
                g_signal_emit(G_OBJECT(t), tracker_signals[SIG_MAINMENU_BLOCKMARK_SET], 0, 0);
                t->sel_start_ch = t->sel_end_ch = -1;
                t->sel_start_row = t->sel_end_row = -1;
                t->inSelMode = FALSE;
                tracker_redraw(t);
            }
            tracker_mouse_to_cursor_pos(t, x, y, &cursor_ch, &cursor_item, &patpos);
            /* Redraw only if needed */
            if (cursor_ch != t->cursor_ch || cursor_item != t->cursor_item) {
                t->cursor_ch = cursor_ch;
                t->cursor_item = cursor_item;
                adjust_xpanning(t, TRUE);
                redraw = 1;
            }
            if (patpos != t->patpos) {
                tracker_set_patpos(t, patpos);
                redraw = 0;
            }
            if (redraw) {
                gtk_widget_queue_draw(GTK_WIDGET(t));
            }
        }
    }
    return TRUE;
}

static gint
tracker_motion_notify(GtkWidget* widget,
    GdkEventMotion* event)
{
    Tracker* t;
    int x, y, cursor_item;
    GdkModifierType state;

    t = TRACKER(widget);

    if (!t->mouse_selecting)
        return TRUE;

    if (event->is_hint) {
        gdk_window_get_pointer(event->window, &x, &y, &state);
    } else {
        x = event->x;
        y = event->y;
        state = event->state;
    }

    if ((state & GDK_BUTTON1_MASK) && t->mouse_selecting) {
        tracker_mouse_to_cursor_pos(t, x, y, &t->sel_end_ch, &cursor_item, &t->sel_end_row);

        if (x > t->disp_startx + t->disp_numchans * t->disp_chanwidth && t->leftchan + t->disp_numchans < t->num_channels) {
            t->sel_end_ch++;
            tracker_set_xpanning_full(t, t->leftchan + 1, TRUE);
        } else if (x < t->disp_startx && t->leftchan > 0) {
            t->sel_end_ch--;
            tracker_set_xpanning_full(t, t->leftchan - 1, TRUE);
        }
        if ((t->sel_end_row > t->patpos + (t->disp_rows / 2)) || (y > t->widget.allocation.height && t->patpos < t->sel_end_row))
            tracker_set_patpos(t, t->patpos + 1);
        else if ((t->sel_end_row < t->patpos - (t->disp_rows / 2)) || (y <= 0 && t->patpos > t->sel_end_row))
            tracker_set_patpos(t, t->patpos - 1);
        tracker_redraw(t);
    }

    return TRUE;
}

static gint
tracker_button_release(GtkWidget* widget,
    GdkEventButton* event)
{
    Tracker* t;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(IS_TRACKER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    t = TRACKER(widget);

    if (t->mouse_selecting && event->button == 1) {
        t->mouse_selecting = 0;
        g_signal_emit(G_OBJECT(t), tracker_signals[SIG_MAINMENU_BLOCKMARK_SET], 0, 0);
    }

    return TRUE;
}

static void
tracker_class_init(TrackerClass* class)
{
    GObjectClass* object_class;
    GtkWidgetClass* widget_class;

    object_class = (GObjectClass*)class;
    widget_class = (GtkWidgetClass*)class;

    widget_class->realize = tracker_realize;
    widget_class->expose_event = tracker_expose;
    widget_class->size_request = tracker_size_request;
    widget_class->size_allocate = tracker_size_allocate;
    widget_class->button_press_event = tracker_button_press;
    widget_class->button_release_event = tracker_button_release;
    widget_class->motion_notify_event = tracker_motion_notify;
    widget_class->scroll_event = tracker_scroll;

    tracker_signals[SIG_PATPOS] = g_signal_new("patpos",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET(TrackerClass, patpos),
        NULL, NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    tracker_signals[SIG_XPANNING] = g_signal_new("xpanning",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(TrackerClass, xpanning),
        NULL, NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    tracker_signals[SIG_XCONF] = g_signal_new("xconf",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET(TrackerClass, xconf),
        NULL, NULL,
        ___marshal_Sig1,
        G_TYPE_NONE, 3,
        G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

    tracker_signals[SIG_YCONF] = g_signal_new("yconf",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(TrackerClass, yconf),
        NULL, NULL,
        ___marshal_Sig1,
        G_TYPE_NONE, 3,
        G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

    tracker_signals[SIG_MAINMENU_BLOCKMARK_SET] = g_signal_new(
        "mainmenu_blockmark_set",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(TrackerClass, mainmenu_blockmark_set),
        NULL, NULL,
        ___marshal_Sig2,
        G_TYPE_NONE, 1, G_TYPE_INT);

    tracker_signals[SIG_ASCROLL] = g_signal_new(
        "alt_scroll",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(TrackerClass, scroll_parameter),
        NULL, NULL,
        __marshal_VOID__INT_INT_INT_INT,
        G_TYPE_NONE, 4,
        G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

    tracker_signals[SIG_CSCROLL] = g_signal_new(
        "ctrl_scroll",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(TrackerClass, alt_scroll),
        NULL, NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    class->patpos = NULL;
    class->xpanning = NULL;
    class->mainmenu_blockmark_set = NULL;
    class->scroll_parameter = NULL;
}

static void
tracker_init(Tracker* t)
{
    GtkWidget* widget = GTK_WIDGET(t);

    t->layout = di_layout_new(widget);

    t->oldpos = -1;
    t->onlyrow = -1;
    t->curpattern = NULL;
    t->enable_backing_store = 0;
    t->pixmap = NULL;
    t->patpos = 0;
    t->cursor_ch = 0;
    t->cursor_item = 0;
    t->editing = FALSE;
    t->print_numbers = TRUE;
    t->print_cursor = TRUE;

    t->selectable = TRUE;
    t->inSelMode = FALSE;
    t->sel_end_row = t->sel_end_ch = t->sel_start_row = t->sel_start_ch = -1;
    t->mouse_selecting = 0;
    t->button = -1;

    /* We implement double buffering by our own (if the user wants),
       so implicit implementation by Gtk+ is not needed */
    gtk_widget_set_double_buffered(widget, FALSE);
}

G_DEFINE_TYPE(Tracker, tracker, GTK_TYPE_WIDGET)

GtkWidget*
tracker_new(void)
{
    return GTK_WIDGET(g_object_new(tracker_get_type(), NULL));
}

