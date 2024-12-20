
/*
 * The Real SoundTracker - Transposition dialog
 *
 * Copyright (C) 1999-2019 Michael Krause
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

#include <glib/gi18n.h>

#include "gui-subs.h"
#include "gui.h"
#include "history.h"
#include "main.h"
#include "st-subs.h"
#include "track-editor.h"
#include "transposition.h"
#include "xm.h"

static GtkWidget *transposition_window = NULL,
                 *transposition_scope_w[4],
                 *transposition_instrument_mode_w[2],
                 *transposition_instrument_w[2];

static void
transposition_transpose_notes_full(XMNote* track,
    int length,
    int add,
    int instrument)
{
    while (length--) {
        if (track->note != 0 && track->note != XM_PATTERN_NOTE_OFF
            && (instrument == -1 || instrument == track->instrument)) {
            track->note = CLAMP(track->note + add, 1, NUM_NOTES);
        }
        track++;
    }
}

static void
transposition_transpose_notes_sub(XMNote* track,
    int patlen,
    int add)
{
    gboolean all_instruments = find_current_toggle(transposition_instrument_mode_w, 2);
    int current_instrument = gui_get_current_instrument();

    transposition_transpose_notes_full(track, patlen, add, all_instruments ? -1 : current_instrument);
}

static void
transposition_change_instruments_sub(XMNote* track,
    int patlen,
    int mode)
{
    int i1 = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(transposition_instrument_w[0]));
    int i2 = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(transposition_instrument_w[1]));

    while (patlen--) {
        if (track->instrument == i1)
            track->instrument = i2;
        else if (mode == 0 && track->instrument == i2)
            track->instrument = i1;
        track++;
    }
}

static void
transposition_for_each(const gboolean reassign_instr,
    int functiondata)
{
    int i, j;
    int mode = find_current_toggle(transposition_scope_w, 4);
    void (*function)(XMNote* track, int patlen, int data) =
        reassign_instr ? transposition_change_instruments_sub : transposition_transpose_notes_sub;
    const gchar* log_message = reassign_instr ?
        N_("Instrument(s) reassigning") : N_("Transposition");

    switch (mode) {
    case 0: // Whole Song
        gui_log_song(log_message, xm);
        for (i = 0; i < XM_NUM_PATTERNS; i++) {
            if (st_is_pattern_used_in_song(xm, i)) {
                for (j = 0; j < xm->num_channels; j++) {
                    function(xm->patterns[i].channels[j], xm->patterns[i].length, functiondata);
                }
            }
        }
        break;
    case 1: // All Patterns
        gui_log_song(log_message, xm);
        for (i = 0; i < XM_NUM_PATTERNS; i++) {
            for (j = 0; j < xm->num_channels; j++) {
                function(xm->patterns[i].channels[j], xm->patterns[i].length, functiondata);
            }
        }
        break;
    case 2: // Current Pattern
        i = gui_get_current_pattern();
        gui_log_pattern(&xm->patterns[i], log_message, -1, xm->patterns[i].length, xm->patterns[i].length);
        for (j = 0; j < xm->num_channels; j++) {
            function(xm->patterns[i].channels[j], xm->patterns[i].length, functiondata);
        }
        break;
    case 3: // Current Track
        i = gui_get_current_pattern();
        j = tracker->cursor_ch;
        track_editor_log_block(tracker, log_message, j, 0, 1, xm->patterns[i].length);
        function(xm->patterns[i].channels[j], xm->patterns[i].length, functiondata);
        break;
    }
}

static void
transposition_transpose_notes(GtkWidget* w,
    gpointer data)
{
    int add = 0;

    switch (GPOINTER_TO_INT(data)) {
    case 0:
        add = +1;
        break;
    case 1:
        add = -1;
        break;
    case 2:
        add = +12;
        break;
    case 3:
        add = -12;
        break;
    }

    transposition_for_each(FALSE, add);
    tracker_redraw(tracker);
}

void transposition_transpose_selection(Tracker* t,
    int by)
{
    int height, width, chStart, rowStart;
    int i;

    if (!tracker_is_valid_selection(t)) {
        return;
    }

    tracker_get_selection_rect(t, &chStart, &rowStart, &width, &height);
    track_editor_log_block(t, N_("Transposing block"), chStart, rowStart, width, height);

    for (i = chStart; i < chStart + width; i++) {
        transposition_transpose_notes_full(t->curpattern->channels[i] + rowStart,
            height, by, -1);
    }

    tracker_redraw(t);
}

static void
transposition_change_instruments(GtkWidget* w,
    gpointer data)
{
    int b = GPOINTER_TO_INT(data);

    transposition_for_each(TRUE, b);
    tracker_redraw(tracker);
}

static void
transposition_current_instrument_clicked(GtkWidget* w,
    gpointer data)
{
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(transposition_instrument_w[GPOINTER_TO_INT(data)]),
        gui_get_current_instrument());
}

void transposition_dialog(void)
{
    gint i;
    GtkWidget *mainbox, *thing, *box1, *box2, *frame;
    const char* labels1[] = {
        _("Whole Song"),
        _("All Patterns"),
        _("Current Pattern"),
        _("Current Track"),
    };
    const char* labels2[] = {
        _("Current Instrument"),
        _("All Instruments"),
    };
    const char* labels3[] = {
        _("Half note up"),
        _("Half note down"),
        _("Octave up"),
        _("Octave down")
    };
    const char* labels4[] = {
        _("Exchange 1 <-> 2"),
        _("Change 1 -> 2")
    };

    if (GUI_EDITING) {
        if (transposition_window != NULL) {
            gtk_window_present(GTK_WINDOW(transposition_window));
            return;
        }

        transposition_window = gtk_dialog_new_with_buttons(_("Transposition Tools"), GTK_WINDOW(mainwindow),
            GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
        gui_dialog_connect(transposition_window, NULL);
        gui_translate_keyevents(transposition_window, mainwindow, FALSE);

        mainbox = gtk_dialog_get_content_area(GTK_DIALOG(transposition_window));
        gui_dialog_adjust(transposition_window, GTK_RESPONSE_CLOSE);

        box1 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box1, FALSE, TRUE, 0);

        thing = gtk_label_new(_("Scope of the operation:"));
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
        make_radio_group_full(labels1, box1, transposition_scope_w, FALSE, TRUE, NULL, NULL);

        frame = gtk_frame_new(NULL);
        gtk_frame_set_label(GTK_FRAME(frame), _("Note Transposition"));
        gtk_box_pack_start(GTK_BOX(mainbox), frame, FALSE, TRUE, 0);

        box2 = gtk_vbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(frame), box2);
        gtk_container_set_border_width(GTK_CONTAINER(box2), 4);

        box1 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(box2), box1, FALSE, TRUE, 0);

        add_empty_hbox(box1);
        make_radio_group_full(labels2, box1, transposition_instrument_mode_w, FALSE, TRUE, NULL, NULL);
        add_empty_hbox(box1);

        box1 = gtk_hbox_new(TRUE, 4);
        gtk_box_pack_start(GTK_BOX(box2), box1, FALSE, TRUE, 0);

        for (i = 0; i < 4; i++) {
            thing = gtk_button_new_with_label(labels3[i]);
            gtk_box_pack_start(GTK_BOX(box1), thing, TRUE, TRUE, 0);
            g_signal_connect(thing, "clicked",
                G_CALLBACK(transposition_transpose_notes),
                GINT_TO_POINTER(i));
        }

        frame = gtk_frame_new(NULL);
        gtk_frame_set_label(GTK_FRAME(frame), _("Instrument Changing"));
        gtk_box_pack_start(GTK_BOX(mainbox), frame, FALSE, TRUE, 0);

        box2 = gtk_vbox_new(FALSE, 2);
        gtk_container_add(GTK_CONTAINER(frame), box2);
        gtk_container_set_border_width(GTK_CONTAINER(box2), 4);

        box1 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(box2), box1, FALSE, TRUE, 0);

        add_empty_hbox(box1);

        gui_put_labelled_spin_button(box1, _("Instrument 1:"), 1, XM_NUM_INSTRUMENTS, &transposition_instrument_w[0], NULL, NULL, FALSE);

        thing = gtk_button_new_with_label(_("Current instrument"));
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
        g_signal_connect(thing, "clicked",
            G_CALLBACK(transposition_current_instrument_clicked), GINT_TO_POINTER(0));

        add_empty_hbox(box1);

        gui_put_labelled_spin_button(box1, _("Instrument 2:"), 1, XM_NUM_INSTRUMENTS, &transposition_instrument_w[1], NULL, NULL, FALSE);

        thing = gtk_button_new_with_label(_("Current instrument"));
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
        g_signal_connect(thing, "clicked",
            G_CALLBACK(transposition_current_instrument_clicked), GINT_TO_POINTER(1));

        add_empty_hbox(box1);

        box1 = gtk_hbox_new(TRUE, 4);
        gtk_box_pack_start(GTK_BOX(box2), box1, FALSE, TRUE, 0);

        for (i = 0; i < 2; i++) {
            thing = gtk_button_new_with_label(labels4[i]);
            gtk_box_pack_start(GTK_BOX(box1), thing, TRUE, TRUE, 0);
            g_signal_connect(thing, "clicked",
                G_CALLBACK(transposition_change_instruments),
                GINT_TO_POINTER(i));
        }

        gtk_widget_show_all(transposition_window);
    } else {
        GtkWidget* dialog = NULL;

        gui_info_dialog(&dialog, _("Transposition is possible only in editing mode"), FALSE);
    }
}
