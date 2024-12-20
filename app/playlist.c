
/*
 * The Real SoundTracker - gtk+ Playlist widget
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

#include <config.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include <string.h>

#include "extspinbutton.h"
#include "gui-subs.h"
#include "history.h"
#include "marshal.h"
#include "playlist.h"
#include "st-subs.h"

/* --- Structures, variables, definitions --- */

struct _Playlist {
    GtkEventBox widget;

    GtkWidget *mainwindow, *dialog;
    GtkAdjustment* adj_songpos;
    GtkWidget *spin_songlength, *spin_songpat, *spin_restartpos, *spin_editor_pat;
    GtkWidget *ibutton, *icbutton, *ifbutton, *dbutton;
    GtkWidget *list, *label_editor_ord;
    GtkListStore* ls;
    GtkTreeSelection* sel;
    GtkTreeModel* tm;

    GtkWidget *unused_label, *unused_list;
    GtkTextBuffer* buffer;
    gint unused_len;
    gchar unused_pat[256];

    GtkWidget *numlabels[5], *patlabels[5];

    gint max_length;
    gint min_pattern;
    gint max_pattern;

    gint current_position;
    gint old_length;
    gint signals_disabled;
    gboolean frozen, enabled, no_select_row;
    gint rbutton_tag, tag_del, tag_ins, tag_sel;
    gint skip_logging;
    gint row_ins;

    XM* xm;
};

enum {
    SIG_CURRENT_POSITION_CHANGED,
    SIG_ENTRY_CHANGED,
    SIG_LENGTH_CHANGED,
    SIG_INSERT_PATTERN,
    SIG_ADD_COPY,
    LAST_SIGNAL
};

static guint playlist_signals[LAST_SIGNAL] = { 0 };

/* --- Utility functions --- */

static void
playlist_update_adjustment(Playlist* p,
    const gint min,
    const gint max)
{
    gint newpos = CLAMP(p->current_position, min, max - 1);

    g_object_freeze_notify(G_OBJECT(p->adj_songpos));
    gtk_adjustment_set_lower(p->adj_songpos, min);
    gtk_adjustment_set_upper(p->adj_songpos, max);
    if (newpos != p->current_position)
        gtk_adjustment_set_value(p->adj_songpos, newpos);
    g_object_thaw_notify(G_OBJECT(p->adj_songpos));
}

static void
update_unused_list(Playlist* p, const gint newlen)
{
    if (p->unused_len) {
        GtkTextIter start, end;

        gtk_text_buffer_get_bounds(p->buffer, &start, &end);
        gtk_text_buffer_delete(p->buffer, &start, &end);
    }
    if (newlen) {
        gchar buf[5];
        gint i;

        for (i = 0; i < newlen; i++) {
            gint len = snprintf(buf, sizeof(buf), "%02d ", p->unused_pat[i]);

            gtk_text_buffer_insert_at_cursor(p->buffer, buf, len);
        }
        if (!p->unused_len) {
            gtk_widget_show(p->unused_label);
            gtk_widget_show(p->unused_list);
        }
    } else {
        gtk_widget_hide(p->unused_label);
        gtk_widget_hide(p->unused_list);
    }
    p->unused_len = newlen;
}

static void
playlist_draw_contents(Playlist* p,
    const gboolean update_playlist,
    const gboolean update_editor)
{
    gint i;
    gchar buf[4];

    g_assert(p->xm != NULL);

    if (p->dialog) {
        g_snprintf(buf, sizeof(buf), "%u", p->current_position);
        gtk_label_set_text(GTK_LABEL(p->label_editor_ord), buf);
    }

    if (update_playlist) {
        for (i = 0; i <= 4; i++) {
            if ((i >= 2 - p->current_position) &&
                (i <= p->xm->song_length - p->current_position + 1)) {
                g_snprintf(buf, sizeof(buf), "%.3u", i - 2 + p->current_position);
                gtk_label_set_text(GTK_LABEL(p->numlabels[i]), buf);
                if (i != 2) {
                    g_snprintf(buf, sizeof(buf), "%.3u", p->xm->pattern_order_table[i - 2 + p->current_position]);
                    gtk_label_set_text(GTK_LABEL(p->patlabels[i]), buf);
                }
            } else {
                gtk_label_set_text(GTK_LABEL(p->numlabels[i]), "");
                gtk_label_set_text(GTK_LABEL(p->patlabels[i]), "");
            }
        }
    }
    if (update_editor && p->dialog) {
        GtkTreeIter iter;
        gint newlen;
        gboolean need_rewrite;

        g_signal_handler_block(G_OBJECT(p->tm), p->tag_del);
        g_signal_handler_block(G_OBJECT(p->tm), p->tag_ins);
        g_signal_handler_block(G_OBJECT(p->sel), p->tag_sel);
        gui_list_freeze(p->list);
        gui_list_clear_with_model(p->tm);
        gtk_tree_view_set_model(GTK_TREE_VIEW(p->list), NULL);
        for (i = 0; i < p->xm->song_length; i++) {
            gtk_list_store_append(p->ls, &iter);
            gtk_list_store_set(p->ls, &iter, 0, i,
                1, p->xm->pattern_order_table[i],
                2, p->xm->patterns[p->xm->pattern_order_table[i]].length, -1);
        }
        gui_list_thaw(p->list, p->tm);
        g_signal_handler_unblock(G_OBJECT(p->tm), p->tag_del);
        g_signal_handler_unblock(G_OBJECT(p->tm), p->tag_ins);
        g_signal_handler_unblock(G_OBJECT(p->sel), p->tag_sel);

        for (i = 0, newlen = 0, need_rewrite = FALSE; i < XM_NUM_PATTERNS; i++)
            if (!st_is_pattern_used_in_song(p->xm, i) &&
                !st_is_empty_pattern(&p->xm->patterns[i])) {
                if (p->unused_pat[newlen] != i) {
                    p->unused_pat[newlen] = i;
                    need_rewrite = TRUE;
                }
                newlen++;
            }
        if (newlen != p->unused_len || need_rewrite)
            update_unused_list(p, newlen);
    }
}

static inline void
playlist_set_length(Playlist* p, const gint length)
{
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songlength), length);
}

static inline void
playlist_set_restartpos(Playlist* p, const gint pos)
{
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_restartpos), pos);
}

struct PlaylistUndoArg {
    gint length, pos, restartpos, src, dest;
    guint8 pot[256];
};

static void
playlist_undo(const gint smp, const gint ins, const gboolean redo,
    gpointer arg, gpointer data)
{
    gint tmp_value, old_restartpos;
    Playlist* p;
    struct PlaylistUndoArg* state = arg;
    guint8* pot = alloca(sizeof(p->xm->pattern_order_table));

    g_assert(IS_PLAYLIST(data));
    p = PLAYLIST(data);
    g_assert(p->xm);

    memcpy(pot, p->xm->pattern_order_table, sizeof(p->xm->pattern_order_table));
    tmp_value = p->xm->song_length;
    old_restartpos = p->xm->restart_position;
    playlist_freeze(p);
    playlist_freeze_signals(p);
    playlist_set_length(p, state->length);
    state->length = tmp_value;
    playlist_set_restartpos(p, state->restartpos);
    state->restartpos = old_restartpos;
    memcpy(p->xm->pattern_order_table, state->pot, sizeof(p->xm->pattern_order_table));
    memcpy(state->pot, pot, sizeof(state->pot));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat),
        p->xm->pattern_order_table[p->current_position]);
    tmp_value = p->current_position;
    gtk_adjustment_set_value(p->adj_songpos, state->pos);
    state->pos = tmp_value;

    if (state->src >= 0) {
        if (redo)
            st_copy_pattern(&p->xm->patterns[state->dest], &p->xm->patterns[state->src]);
        else
            st_clear_pattern(&p->xm->patterns[state->dest]);
    }

    if (!p->signals_disabled)
        g_signal_emit(G_OBJECT(p),
            playlist_signals[SIG_CURRENT_POSITION_CHANGED],
            0, tmp_value);
    playlist_thaw_signals(p);
    playlist_thaw(p);
}

static void
playlist_log(Playlist* p,
    const gchar* const title,
    const gint src,
    const gint dest)
{
    struct PlaylistUndoArg* arg = g_new(struct PlaylistUndoArg, 1);

    arg->length = p->xm->song_length;
    arg->pos = p->current_position;
    arg->restartpos = p->xm->restart_position;
    arg->src = src,
    arg->dest = dest;
    memcpy(arg->pot, p->xm->pattern_order_table, sizeof(arg->pot));
    history_log_action(HISTORY_ACTION_POINTER, _(title),
        HISTORY_FLAG_LOG_PAT, playlist_undo, p, sizeof(struct PlaylistUndoArg), arg);
}
static void
_playlist_insert_pattern(Playlist* p,
    const gchar* const title,
    const gint pos,
    const gint pat,
    const gint src)
{
    gint current_songpos;
    gint length;

    g_assert(IS_PLAYLIST(p));
    g_assert(p->xm != NULL);
    length = p->xm->song_length;
    g_assert(pos >= 0 && pos <= length);
    g_assert(pat >= p->min_pattern && pat <= p->max_pattern);

    if (length == p->max_length)
        return;

    current_songpos = lrint(gtk_adjustment_get_value(p->adj_songpos));

    playlist_log(p, _(title), src, pat);

    playlist_freeze(p);
    playlist_freeze_signals(p);
    p->skip_logging++;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songlength), length + 1);
    p->skip_logging--;
    playlist_thaw_signals(p);

    memmove(&p->xm->pattern_order_table[pos + 1], &p->xm->pattern_order_table[pos],
        sizeof(p->xm->pattern_order_table) - (1 + pos) * sizeof(p->xm->pattern_order_table[0]));
    p->xm->pattern_order_table[pos] = pat;
    if (pos == current_songpos) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat),
            p->xm->pattern_order_table[pos]);
    }

    /* This also redraws the playlist */
    playlist_thaw(p);
}

static void editor_select_current_row(Playlist* p)
{
    GtkTreeIter sel_iter;

    if (p->dialog && gui_list_get_iter(p->current_position, p->tm, &sel_iter)) {
        gtk_tree_selection_select_iter(p->sel, &sel_iter);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(p->list),
            gtk_tree_model_get_path(p->tm, &sel_iter), NULL, TRUE, 0.5, 0.5);
    }
}
/* --- GUI callbacks --- */

static void
editor_row_activated(GtkTreeSelection* sel,
    Playlist* p)
{
    gint pos = gui_list_get_selection_index(sel);

    if (pos != p->current_position) {
        p->no_select_row = TRUE;
        playlist_set_position(p, pos);
        p->no_select_row = FALSE;
    }
}

static void
pot_undo(const gint smp, const gint ins, const gboolean redo,
    gpointer arg, gpointer data)
{
    Playlist* p = PLAYLIST(data);
    guint8 *pot = arg, *tmp = alloca(sizeof(p->xm->pattern_order_table));

    memcpy(tmp, p->xm->pattern_order_table, sizeof(p->xm->pattern_order_table));
    memcpy(p->xm->pattern_order_table, pot, sizeof(p->xm->pattern_order_table));
    memcpy(pot, tmp, sizeof(p->xm->pattern_order_table));
    playlist_draw_contents(p, TRUE, TRUE);
    editor_select_current_row(p);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat),
        p->xm->pattern_order_table[p->current_position]);
}

static void
playlist_log_pot(Playlist* p)
{
    guint8* pot = g_malloc(sizeof(p->xm->pattern_order_table));

    memcpy(pot, p->xm->pattern_order_table, sizeof(p->xm->pattern_order_table));
    history_log_action(HISTORY_ACTION_POINTER, _("Pattern sequence modification"),
        HISTORY_FLAG_LOG_POS | HISTORY_FLAG_LOG_PAT,
        pot_undo, p, sizeof(p->xm->pattern_order_table), pot);
}

static void
editor_row_deleted(GtkTreeView* tv, GtkTreePath* path, Playlist* p)
{
    gint* indices = gtk_tree_path_get_indices(path);
    gint row_del = indices[0], row_ins = p->row_ins;
    guint8 tmp;

    g_assert(p->xm != NULL);
    /* Something's wrong, just ignoring */
    if (p->row_ins == -1)
        return;

    /* Somewhat strange logic of treeview dnd */
    if (row_ins < row_del - 1) {
        playlist_log_pot(p);
        tmp = p->xm->pattern_order_table[row_del - 1];
        memmove(&p->xm->pattern_order_table[row_ins + 1],
                &p->xm->pattern_order_table[row_ins],
                row_del - row_ins - 1);
        p->xm->pattern_order_table[row_ins] = tmp;
        if (p->current_position != row_ins) {
            playlist_set_position(p, row_ins);
            playlist_draw_contents(p, FALSE, TRUE);
        } else {
            playlist_draw_contents(p, TRUE, TRUE);
        }
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat), tmp);
        editor_select_current_row(p);
    } else if (row_ins > row_del + 1) {
        playlist_log_pot(p);
        tmp = p->xm->pattern_order_table[row_del];
        memmove(&p->xm->pattern_order_table[row_del],
                &p->xm->pattern_order_table[row_del + 1],
                row_ins - row_del - 1);
        p->xm->pattern_order_table[row_ins - 1] = tmp;
        if (p->current_position != row_ins - 1) {
            playlist_set_position(p, row_ins - 1);
            playlist_draw_contents(p, FALSE, TRUE);
        } else {
            playlist_draw_contents(p, TRUE, TRUE);
        }
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat), tmp);
        editor_select_current_row(p);
    }

    p->row_ins = -1;
}

static void
editor_row_inserted(GtkTreeView* tv, GtkTreePath* path, GtkTreeIter* itr, Playlist* p)
{
    gint* indices = gtk_tree_path_get_indices(path);
    p->row_ins = indices[0];
}

static void
playlist_insert_clicked(Playlist* p)
{
    if (!p->signals_disabled)
        g_signal_emit(G_OBJECT(p),
            playlist_signals[SIG_INSERT_PATTERN],
            0, p->current_position);
}

static void
playlist_delete_clicked(Playlist* p)
{
    gint pos;

    g_assert(p->xm != NULL);

    if (p->xm->song_length == 1)
        return;

    pos = lrint(gtk_adjustment_get_value(p->adj_songpos));
    playlist_log(p, N_("Pattern removal"), -1, -1);
    memmove(&p->xm->pattern_order_table[pos], &p->xm->pattern_order_table[pos + 1],
        sizeof(p->xm->pattern_order_table) - (1 + pos) * sizeof(p->xm->pattern_order_table[0]));

    /* Length change is already logged */
    p->skip_logging++;
    playlist_set_length(p, p->xm->song_length - 1);
    p->skip_logging--;

    pos = lrint(gtk_adjustment_get_value(p->adj_songpos));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat),
        p->xm->pattern_order_table[pos]);
    editor_select_current_row(p);
    if (!p->signals_disabled)
        g_signal_emit(G_OBJECT(p),
            playlist_signals[SIG_CURRENT_POSITION_CHANGED],
            0, pos);
}

static void
playlist_add_free_clicked(Playlist* p)
{
    playlist_add_pattern(p, -1);
}

static void
playlist_add_copy_clicked(Playlist* p)
{
    if (!p->signals_disabled)
        g_signal_emit(G_OBJECT(p),
            playlist_signals[SIG_ADD_COPY], 0);
}

static gboolean
playlist_button_release(GtkWidget* wid, GdkEventButton* event, Playlist* p)
{
    const gchar* titles[] = {N_("Num"), N_("Pat"), N_("Len")};
    GType types[] = {G_TYPE_INT, G_TYPE_INT, G_TYPE_INT};
    const gfloat alignments[] = {0.5, 0.5, 0.5};
    const gboolean expands[] = {FALSE, FALSE, FALSE};

    g_assert(p->xm != NULL);

    if (event->button != 3)
        return FALSE;

    if (!p->dialog) {
        GtkWidget *vbox, *hbox, *thing;
        gint w, h;

        p->dialog = gtk_dialog_new_with_buttons(_("Pattern Sequence Editor"), GTK_WINDOW(p->mainwindow), 0,
            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
        gui_dialog_adjust(p->dialog, GTK_RESPONSE_CLOSE);
        gui_dialog_connect(p->dialog, NULL);
        gui_translate_keyevents(p->dialog, p->mainwindow, FALSE);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(p->dialog));
        gtk_box_set_spacing(GTK_BOX(vbox), 2);

        hbox = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
        thing = gtk_label_new(_("Order"));
        gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

        p->label_editor_ord = gtk_label_new("0");
        gui_get_pixel_size(p->label_editor_ord, "000", &w, &h);
        gtk_widget_set_size_request(p->label_editor_ord, w, -1);
        gtk_misc_set_alignment(GTK_MISC(p->label_editor_ord), 1.0, 0.5);
        gtk_box_pack_start(GTK_BOX(hbox), p->label_editor_ord, FALSE, FALSE, 0);
        /* Spacer */
        thing = gtk_label_new("");
        gtk_widget_set_size_request(thing, 10, -1);
        gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

        thing = gtk_label_new(_("Pattern"));
        gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
        p->spin_editor_pat = extspinbutton_new(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(p->spin_songpat)),
            1.0, 0, FALSE);
        gtk_box_pack_start(GTK_BOX(hbox), p->spin_editor_pat, FALSE, FALSE, 0);

        p->list = gui_list_in_scrolled_window_full(3, titles, vbox,
            types, alignments, expands, GTK_SELECTION_BROWSE, TRUE, TRUE,
            NULL, GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
        gtk_tree_view_set_reorderable(GTK_TREE_VIEW(p->list), TRUE);
        p->ls = GUI_GET_LIST_STORE(p->list);
        p->sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(p->list));
        p->tm = gtk_tree_view_get_model(GTK_TREE_VIEW(p->list));
        p->tag_sel = g_signal_connect_after(p->sel, "changed",
            G_CALLBACK(editor_row_activated), p);
        p->tag_del = g_signal_connect(gtk_tree_view_get_model(GTK_TREE_VIEW(p->list)),
            "row_deleted", G_CALLBACK(editor_row_deleted), p);
        p->tag_ins = g_signal_connect_after(gtk_tree_view_get_model(GTK_TREE_VIEW(p->list)),
            "row_inserted", G_CALLBACK(editor_row_inserted), p);

        hbox = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

        thing = gui_button(GTK_STOCK_ADD, G_CALLBACK(playlist_insert_clicked), p, hbox, TRUE, FALSE);
        gtk_widget_set_tooltip_text(thing, _("Insert pattern"));
        thing = gui_button(GTK_STOCK_REMOVE, G_CALLBACK(playlist_delete_clicked), p, hbox, TRUE, FALSE);
        gtk_widget_set_tooltip_text(thing, _("Delete pattern"));
        thing = gui_button(GTK_STOCK_NEW, G_CALLBACK(playlist_add_free_clicked), p, hbox, TRUE, FALSE);
        gtk_widget_set_tooltip_text(thing, _("Add free pattern"));
        thing = gui_button(GTK_STOCK_COPY, G_CALLBACK(playlist_add_copy_clicked), p, hbox, TRUE, FALSE);
        gtk_widget_set_tooltip_text(thing, _("Add free pattern and copy"));

        gtk_widget_show_all(p->dialog);

        p->unused_label = gtk_label_new(_("Unused patterns:"));
        gtk_box_pack_start(GTK_BOX(vbox), p->unused_label, FALSE, FALSE, 0);
        p->buffer = gtk_text_buffer_new(NULL);
        p->unused_list = gtk_text_view_new_with_buffer(p->buffer);
        gtk_text_view_set_editable(GTK_TEXT_VIEW(p->unused_list), FALSE);
        gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(p->unused_list), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(p->unused_list), GTK_WRAP_WORD);
        gtk_box_pack_start(GTK_BOX(vbox), p->unused_list, FALSE, FALSE, 0);

        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
        gtk_widget_show(thing);

        gtk_widget_set_size_request(p->list, -1, 100);
    }

    /* It's easier just to repopulate patterns list on each editor
       showing up rather than taking care of its actual state */
    playlist_draw_contents(p, FALSE, TRUE);
    editor_select_current_row(p);

    gtk_window_present(GTK_WINDOW(p->dialog));
    return TRUE;
}

static gboolean
scroll_event(GtkWidget* w, GdkEventScroll* event, Playlist* p)
{
    int pos = p->current_position;

    switch (event->direction) {
    case GDK_SCROLL_UP:
        pos--;
        if ((pos >= 0) && (pos) < p->xm->song_length)
            gtk_adjustment_set_value(p->adj_songpos, pos);
        return TRUE;
    case GDK_SCROLL_DOWN:
        pos++;
        if ((pos >= 0) && (pos) < p->xm->song_length)
            gtk_adjustment_set_value(p->adj_songpos, pos);
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

static gboolean
label_clicked(GtkWidget* w, GdkEventButton* event, Playlist* p)
{
    int i;

    int pos = p->current_position;

    if (event->button == 1) {
        // find what label is clicked
        for (i = -2; i <= 2; i++) {
            if ((p->numlabels[i + 2] == GTK_BIN(w)->child) || (p->patlabels[i + 2] == GTK_BIN(w)->child)) {
                pos += i;
                if ((pos >= 0) && (pos) < p->xm->song_length)
                    gtk_adjustment_set_value(p->adj_songpos, pos);
                return (TRUE);
            }
        }
    }
    return (FALSE);
}

static void
playlist_songpos_changed(GtkAdjustment* adj,
    Playlist* p)
{
    gint newpos;

    g_assert(p->xm != NULL);

    newpos = lrint(gtk_adjustment_get_value(adj));
    if (newpos == p->current_position)
        return;

    p->current_position = newpos;

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat),
        p->xm->pattern_order_table[newpos]);

    if (!p->frozen)
        playlist_draw_contents(p, TRUE, FALSE);

    if (p->dialog && gtk_widget_get_visible(p->dialog) && !p->no_select_row)
        editor_select_current_row(p);

    if (!p->signals_disabled)
        g_signal_emit(G_OBJECT(p),
            playlist_signals[SIG_CURRENT_POSITION_CHANGED],
            0, newpos);
}

struct LNR {
    gint length, restartpos;
};

static void
song_length_undo(const gint smp, const gint ins, const gboolean redo,
    gpointer arg, gpointer data)
{
    gint tmp_value;
    struct LNR* values = arg;
    Playlist* p = PLAYLIST(data);

    tmp_value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(p->spin_songlength));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songlength), values->length);
    values->length = tmp_value;
    tmp_value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(p->spin_restartpos));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_restartpos), values->restartpos);
    values->restartpos = tmp_value;
}

static void
playlist_songlength_changed(GtkSpinButton* spin,
    Playlist* p)
{
    gint newlen;

    g_assert(p->xm != NULL);

    newlen = gtk_spin_button_get_value_as_int(spin);

    if (newlen == p->xm->song_length)
        return;

    if (!p->skip_logging && !history_test_collate(HISTORY_ACTION_POINTER, 0, p)) {
            struct LNR* arg = g_new(struct LNR, 1);

            arg->length = p->xm->song_length;
            arg->restartpos = p->xm->restart_position;
            history_log_action(HISTORY_ACTION_POINTER, _("Song length changing"),
                0, song_length_undo, p, sizeof(struct LNR), arg);
    }

    p->xm->song_length = newlen;
    playlist_update_adjustment(p, 0, newlen);
    p->skip_logging++;
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(p->spin_restartpos), 0, newlen - 1);
    p->skip_logging--;

    if (!p->frozen)
        playlist_draw_contents(p, TRUE, TRUE);

    /* This signal is being emitted every time even when other signals are frozen */
    g_signal_emit(G_OBJECT(p),
        playlist_signals[SIG_LENGTH_CHANGED], 0, newlen);
}

static void
playlist_songpat_changed(GtkSpinButton* spin,
    Playlist* p)
{
    gint n = gtk_spin_button_get_value_as_int(spin);
    gint oldpat;
    gint unused_removed = -2, unused_inserted = -2, newlen = p->unused_len, i;

    if (!p->xm)
        return;

    oldpat = p->xm->pattern_order_table[p->current_position];
    if (oldpat == n)
        return;

    /* Check whether the pattern was unused */
    if (p->dialog && gtk_widget_get_visible(p->dialog)) {
        GtkTreeIter iter;

        if (gui_list_get_iter(p->current_position, p->tm, &iter))
            /* Update patterns list in the Sequence Editor */
            gtk_list_store_set(p->ls, &iter, 1, n,
                2, p->xm->patterns[n].length, -1);
        for (i = 0; i < newlen; i++)
            if (p->unused_pat[i] == n) {
                newlen--;
                unused_removed = i;
                break;
            }
    }

    history_log_spin_button(spin, _("Pattern number changing"),
        HISTORY_FLAG_LOG_POS, p->xm->pattern_order_table[p->current_position]);
    p->xm->pattern_order_table[p->current_position] = n;

    /* Check whether the pattern becomes unused */
    if (p->dialog && gtk_widget_get_visible(p->dialog)) {
        if (!st_is_pattern_used_in_song(p->xm, oldpat) &&
            !st_is_empty_pattern(&p->xm->patterns[oldpat])) {
            newlen++;

            unused_inserted = 0;
            for (i = newlen - 1; i >= 0; i--)
                if (oldpat > p->unused_pat[i]) {
                    unused_inserted = i + 1;
                    break;
                }
            if (unused_removed >= 0 &&
                unused_inserted > unused_removed)
                /* Specific case of exchange */
                unused_inserted--;
        }
        /* Update unused list if necessary */
        if (unused_removed >= 0 || unused_inserted >= 0) {
            if (unused_removed < 0)
                unused_removed = p->unused_len;
            if (unused_inserted < 0)
                unused_inserted = newlen; /* That is unused_len - 1 */
            if (unused_inserted > unused_removed)
                for (i = unused_removed; i < unused_inserted; i++)
                    p->unused_pat[i] = p->unused_pat[i + 1];
            else
                for (i = unused_removed; i > unused_inserted; i--)
                    p->unused_pat[i] = p->unused_pat[i - 1];
            if (newlen >= p->unused_len)
                p->unused_pat[unused_inserted] = oldpat;

            update_unused_list(p, newlen);
        }
    }

    if (!p->signals_disabled)
        g_signal_emit(G_OBJECT(p),
            playlist_signals[SIG_ENTRY_CHANGED],
            0, n);
}

static void
playlist_restartpos_changed(GtkSpinButton* spin,
    Playlist* p)
{
    gint n = gtk_spin_button_get_value_as_int(spin);

    g_assert(p->xm != NULL);

    if (p->xm->restart_position == n)
        return;

    if (!p->skip_logging)
        history_log_spin_button(spin, _("Restart position changing"),
            0, p->xm->restart_position);
    p->xm->restart_position = n;
}

static void
is_realized(GtkWidget* algn, gpointer data)
{
    guint x, y;

    gtk_widget_realize(algn);
    x = (algn->allocation).width;
    y = (algn->allocation).height;
    gtk_widget_set_size_request(algn, x, y);
}

/* --- API functions --- */

GtkWidget*
playlist_new(GtkWidget* mainwindow)
{
    GtkWidget *box, *thing, *thing1, *vbox, *frame, *box1, *evbox, *al;
    GtkAdjustment* adj;
    gint i;

    Playlist *p = g_object_new(playlist_get_type(), NULL);

    p->mainwindow = mainwindow;
    p->rbutton_tag = g_signal_connect(p, "button_release_event",
        G_CALLBACK(playlist_button_release), p);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(p), vbox);

    box = gtk_hbox_new(FALSE, 2);
    thing = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(thing), GTK_SHADOW_IN);
    thing1 = gtk_table_new(2, 5, FALSE);

    /* pattern list view */

    for (i = 0; i <= 4; i++)
        if (i != 2) {
            p->numlabels[i] = gtk_label_new("");
            evbox = gtk_event_box_new();
            gtk_container_add(GTK_CONTAINER(evbox), p->numlabels[i]);
            g_signal_connect(evbox, "button_press_event",
                G_CALLBACK(label_clicked), p);
            box1 = gtk_hbox_new(FALSE, 0),
            gtk_box_pack_start(GTK_BOX(box1), evbox, FALSE, FALSE, 3);
            gtk_table_attach_defaults(GTK_TABLE(thing1), box1, 0, 1, i, i + 1);

            p->patlabels[i] = gtk_label_new("");
            evbox = gtk_event_box_new();
            gtk_container_add(GTK_CONTAINER(evbox), p->patlabels[i]);
            g_signal_connect(evbox, "button_press_event",
                G_CALLBACK(label_clicked), p);
            box1 = gtk_hbox_new(FALSE, 0),
            gtk_box_pack_start(GTK_BOX(box1), evbox, FALSE, FALSE, 0);
            gtk_table_attach_defaults(GTK_TABLE(thing1), box1, 1, 2, i, i + 1);
        }
    /* central label */
    box1 = gtk_hbox_new(FALSE, 0);
    p->numlabels[2] = gtk_label_new("");
    /* a bit trick to keep label size constant */
    al = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
    gtk_container_add(GTK_CONTAINER(al), p->numlabels[2]);
    g_signal_connect(al, "realize", G_CALLBACK(is_realized), NULL);
    gtk_box_pack_start(GTK_BOX(box1), al, TRUE, TRUE, 0);

    /* current pattern */
    adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, p->min_pattern, p->max_pattern, 1.0, 5.0, 0.0));
    p->spin_songpat = extspinbutton_new(adj, 1.0, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(p->spin_songpat));
    g_signal_connect(p->spin_songpat, "value-changed",
        G_CALLBACK(playlist_songpat_changed), p);
    gtk_box_pack_end(GTK_BOX(box1), p->spin_songpat, TRUE, TRUE, 0);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
    gtk_container_add(GTK_CONTAINER(frame), box1);

    gtk_table_attach_defaults(GTK_TABLE(thing1), frame, 0, 2, 2, 3);

    evbox = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(evbox), thing1);
    g_signal_connect(evbox, "button_press_event",
        G_CALLBACK(label_clicked), p);
    g_signal_connect(evbox, "scroll-event",
        G_CALLBACK(scroll_event), p);

    gtk_container_add(GTK_CONTAINER(thing), evbox);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);

    /* scrollbar */
    p->adj_songpos = GTK_ADJUSTMENT(gtk_adjustment_new(p->current_position,
        0.0, 1.0, 1.0, 5.0, 1.0));
    thing = gtk_vscrollbar_new(p->adj_songpos);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);
    g_signal_connect(p->adj_songpos, "value_changed",
        G_CALLBACK(playlist_songpos_changed), p);

    /* buttons */
    thing = gtk_vbox_new(TRUE, 0);

    thing1 = p->ibutton = gtk_button_new_with_label(_("Insert"));
    gtk_widget_set_tooltip_text(thing1, _("Insert pattern that is being edited"));
    gtk_box_pack_start(GTK_BOX(thing), thing1, TRUE, FALSE, 0);
    g_signal_connect_swapped(thing1, "clicked",
        G_CALLBACK(playlist_insert_clicked), p);
    g_signal_connect(thing1, "button_release_event",
        G_CALLBACK(playlist_button_release), p);

    thing1 = p->dbutton = gtk_button_new_with_label(_("Delete"));
    gtk_widget_set_tooltip_text(thing1, _("Remove current playlist entry"));
    gtk_box_pack_start(GTK_BOX(thing), thing1, TRUE, FALSE, 0);
    g_signal_connect_swapped(thing1, "clicked",
        G_CALLBACK(playlist_delete_clicked), p);
    g_signal_connect(thing1, "button_release_event",
        G_CALLBACK(playlist_button_release), p);

    thing1 = p->icbutton = gtk_button_new_with_label(_("Add + Cpy"));
    gtk_widget_set_tooltip_text(thing1, _("Add a free pattern behind current position, and copy current pattern to it"));
    gtk_box_pack_start(GTK_BOX(thing), thing1, TRUE, FALSE, 0);
    g_signal_connect_swapped(thing1, "clicked",
        G_CALLBACK(playlist_add_copy_clicked), p);
    g_signal_connect(thing1, "button_release_event",
        G_CALLBACK(playlist_button_release), p);

    thing1 = p->ifbutton = gtk_button_new_with_label(_("Add Free"));
    gtk_widget_set_tooltip_text(thing1, _("Add a free pattern behind current position"));
    gtk_box_pack_start(GTK_BOX(thing), thing1, TRUE, FALSE, 0);
    g_signal_connect_swapped(thing1, "clicked",
        G_CALLBACK(playlist_add_free_clicked), p);
    g_signal_connect(thing1, "button_release_event",
        G_CALLBACK(playlist_button_release), p);

    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);

    /* box with songlen and repstart spinbuttons */
    box = gtk_hbox_new(FALSE, 0);

    thing1 = gtk_label_new(_("Len"));
    gtk_box_pack_start(GTK_BOX(box), thing1, FALSE, FALSE, 0);

    adj = GTK_ADJUSTMENT(gtk_adjustment_new(1.0, 1.0, p->max_length, 1.0, 5.0, 0.0));
    thing1 = p->spin_songlength = extspinbutton_new(adj, 1.0, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(thing1));
    gtk_widget_set_tooltip_text(thing1, _("Song length"));
    gtk_box_pack_start(GTK_BOX(box), thing1, FALSE, FALSE, 2);
    g_signal_connect(thing1, "value-changed",
        G_CALLBACK(playlist_songlength_changed), p);

    adj = GTK_ADJUSTMENT(gtk_adjustment_new(p->current_position, 0.0, 1.0, 1.0, 5.0, 0.0));
    thing1 = p->spin_restartpos = extspinbutton_new(adj, 1.0, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(thing1));
    gtk_widget_set_tooltip_text(thing1, _("Song restart position"));
    gtk_box_pack_end(GTK_BOX(box), thing1, FALSE, FALSE, 0);
    g_signal_connect(thing1, "value-changed",
        G_CALLBACK(playlist_restartpos_changed), p);

    thing1 = gtk_label_new(_("Rstrt"));
    gtk_box_pack_end(GTK_BOX(box), thing1, FALSE, FALSE, 2);

    gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);

    gtk_widget_show_all(GTK_WIDGET(p));
    return GTK_WIDGET(p);
}

GtkAdjustment*
playlist_get_songpat_adjustment(Playlist* p)
{
    g_assert(IS_PLAYLIST(p));

    return gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(p->spin_songpat));
}

void playlist_freeze(Playlist* p)
{
    g_assert(IS_PLAYLIST(p));
    p->frozen++;
}

void playlist_thaw(Playlist* p)
{
    g_assert(IS_PLAYLIST(p));

    if (p->frozen)
        p->frozen--;
    if (!p->frozen)
        playlist_draw_contents(p, TRUE, TRUE);
}

void playlist_enable(Playlist* p,
    const gboolean enable)
{
    g_assert(IS_PLAYLIST(p));

    if (enable == p->enabled)
        return;

    p->enabled = enable;
    gtk_widget_set_sensitive(p->spin_songlength, enable);
    gtk_widget_set_sensitive(p->spin_songpat, enable);
    gtk_widget_set_sensitive(p->spin_restartpos, enable);
    gtk_widget_set_sensitive(p->ibutton, enable);
    gtk_widget_set_sensitive(p->ifbutton, enable);
    gtk_widget_set_sensitive(p->icbutton, enable);
    gtk_widget_set_sensitive(p->dbutton, enable);
    enable ? g_signal_handler_unblock(G_OBJECT(p), p->rbutton_tag) :
        g_signal_handler_block(G_OBJECT(p), p->rbutton_tag);
    if (!enable && p->dialog)
        gtk_widget_hide(p->dialog);
}

void playlist_freeze_signals(Playlist* p)
{
    g_assert(IS_PLAYLIST(p));
    p->signals_disabled++;
}

void playlist_thaw_signals(Playlist* p)
{
    g_assert(IS_PLAYLIST(p));
    g_assert(p->signals_disabled > 0);

    p->signals_disabled--;
}

void playlist_set_xm(Playlist* p, XM* xm)
{
    g_assert(IS_PLAYLIST(p));
    g_assert(xm != NULL);

    p->xm = xm;
    if (p->old_length != xm->song_length) {
        p->old_length = xm->song_length;
        /* Force signaling about length changing */
        g_signal_emit(G_OBJECT(p),
            playlist_signals[SIG_LENGTH_CHANGED], 0, xm->song_length);
    }

    playlist_freeze(p);
    playlist_freeze_signals(p);
    gtk_adjustment_set_value(p->adj_songpos, 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songlength),
        p->xm->song_length);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_restartpos),
        p->xm->restart_position);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(p->spin_songpat),
        p->xm->pattern_order_table[0]);
    playlist_update_adjustment(p, 0, xm->song_length);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(p->spin_restartpos), 0, xm->song_length - 1);
    playlist_thaw_signals(p);
    playlist_thaw(p);
}

gint playlist_get_nth_pattern(Playlist* p,
    const int pos)
{
    g_assert(IS_PLAYLIST(p));
    g_assert(p->xm != NULL);
    g_assert(pos >= 0 && pos < p->xm->song_length);

    return p->xm->pattern_order_table[pos];
}

void playlist_set_pattern_length(Playlist* p,
    const gint pat,
    const gint len)
{
    gint i;

    g_assert(IS_PLAYLIST(p));
    g_assert(p->xm != NULL);

    if (p->dialog && gtk_widget_get_visible(p->dialog))
        for (i = 0; i < p->xm->song_length; i++) {
            const gint n = p->xm->pattern_order_table[i];

            if (n == pat) {
                GtkTreeIter iter;

                if (gui_list_get_iter(i, p->tm, &iter))
                    gtk_list_store_set(p->ls, &iter,
                        2, p->xm->patterns[n].length, -1);
            }
        }
}

void
playlist_set_position(Playlist* p,
    const gint pos)
{
    g_assert(IS_PLAYLIST(p));
    g_assert(p->xm != NULL);
    g_assert(pos >= 0 && pos < p->xm->song_length);

    if (p->current_position != pos)
        gtk_adjustment_set_value(p->adj_songpos, pos);
}

gint playlist_get_position(Playlist* p)
{
    g_assert(IS_PLAYLIST(p));

    return p->current_position;
}

void playlist_insert_pattern(Playlist* p,
    const gint pos,
    const gint pat)
{
    _playlist_insert_pattern(p, N_("Pattern insertion"), pos, pat, -1);
    editor_select_current_row(p);
}

void
playlist_add_pattern(Playlist* p, const gint copy_from)
{

    g_assert(IS_PLAYLIST(p));
    g_assert(p->xm != NULL);
    g_assert(!(copy_from > p->max_pattern));

    if (p->xm->song_length < p->max_length) {
        gint n = st_find_first_unused_and_empty_pattern(p->xm);

        if (n != -1) {
            _playlist_insert_pattern(p,
                copy_from >= 0 ? N_("Pattern add + copy") : N_("Free pattern addition"),
                p->current_position + 1, n, copy_from);
            playlist_set_position(p, p->current_position + 1);
        }

        if (copy_from >= 0) {
            st_copy_pattern(&p->xm->patterns[n], &p->xm->patterns[copy_from]);
        }
    }
}

/* --- gtk+ object initialization crap --- */

static void
playlist_class_init(PlaylistClass* class)
{
    GObjectClass* g_object_class;

    g_object_class = (GObjectClass*)class;

    playlist_signals[SIG_CURRENT_POSITION_CHANGED] = g_signal_new("current_position_changed",
        G_TYPE_FROM_CLASS(g_object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(PlaylistClass, current_position_changed),
        NULL, NULL,
        g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
    playlist_signals[SIG_ENTRY_CHANGED] = g_signal_new("entry_changed",
        G_TYPE_FROM_CLASS(g_object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(PlaylistClass, entry_changed),
        NULL, NULL,
        g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
    playlist_signals[SIG_LENGTH_CHANGED] = g_signal_new("song_length_changed",
        G_TYPE_FROM_CLASS(g_object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(PlaylistClass, song_length_changed),
        NULL, NULL,
        g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
    playlist_signals[SIG_INSERT_PATTERN] = g_signal_new("insert_pattern",
        G_TYPE_FROM_CLASS(g_object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(PlaylistClass, insert_pattern),
        NULL, NULL,
        g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
    playlist_signals[SIG_ADD_COPY] = g_signal_new("add_copy",
        G_TYPE_FROM_CLASS(g_object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(PlaylistClass, add_copy),
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    class->current_position_changed = NULL;
    class->entry_changed = NULL;
    class->insert_pattern = NULL;
    class->add_copy = NULL;
}

static void
playlist_init(Playlist* p)
{
    // These presets should probably be configurable via the interface
    p->max_length = 256;
    p->min_pattern = 0;
    p->max_pattern = 255;
    p->unused_len = 0;
    p->old_length = 0;

    // A reasonable default playlist
    p->xm = NULL;
    p->current_position = 0;
    p->signals_disabled = 0;
    p->frozen = FALSE;
    p->enabled = TRUE;
    p->no_select_row = FALSE;
    p->skip_logging = FALSE;
    p->row_ins = -1;

    p->dialog = NULL;
}

G_DEFINE_TYPE(Playlist, playlist, GTK_TYPE_EVENT_BOX)
