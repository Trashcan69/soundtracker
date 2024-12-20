
/*
 * The Real SoundTracker - track editor
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "audio.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "gui.h"
#include "history.h"
#include "keys.h"
#include "main.h"
#include "menubar.h"
#include "preferences.h"
#include "st-subs.h"
#include "track-editor.h"
#include "tracker-settings.h"
#include "xm-player.h"

#define NG_0x40 4

/* How many parameters each FX has. 0 means that there is no effect with this code,
   3 -- that the effect has only one 4-bit parameter, NG_0x40 -- that the value
   should not be greater than 0x40 */

static const guint8 n_params[] =
    {2, 1, 1, 1, 2, 1, 1, 2, 1, 1, /* 0 - 9 */
     2, 1, 1 | NG_0x40, 1, 3, 1, 1 | NG_0x40, 2, 0, 0, /* a - j */
     1, 1, 0, 0, 0, 2, 1, 2, 0, 2, /* k - t */
     0, 0, 0, 3, 0, 1}; /* u - z */

extern GtkBuilder *gui_builder;

Tracker* tracker;
GtkWidget* trackersettings;
GtkWidget* vscrollbar;

static GtkWidget* hscrollbar;
static GtkAdjustment* adj;

typedef struct {
    XMPattern pattern; /* Must be at the first place for inheritance */
    guint num_chans;
    Tracker* tracker;
    const gchar* const name;
} BlockBuffer;

static XMNote notes[256];
/* A little bit overhead, but allows to handle track / block / pattern buffer
   at the same manner */
static BlockBuffer ccp_buffers[3] = {
    {
        .pattern.length = 0,
        .pattern.channels[0] = notes,
        .num_chans = 1,
        .name = N_("Track")
    },
    {
        .pattern.length = 0,
        .pattern.channels[0] = NULL,
        .num_chans = 0,
        .name = N_("Pattern")
    },
    {
        .pattern.length = 0,
        .pattern.channels[0] = NULL,
        .num_chans = 0,
        .name = N_("Block")
    }
};
#define track_buffer ccp_buffers[0]
#define pattern_buffer ccp_buffers[1]
#define block_buffer ccp_buffers[2]

static guint hscroll_tag, vscroll_tag;

/* this array contains -1 if the note is not running on the specified channel
   or the note number if it is being played. This is necessary to handle the
   key on/off situation. */
static int note_running[32] = {-1};

static int update_freq = 30;
static int gtktimer = -1;

static guint track_editor_editmode_status_idle_handler = 0;
static gchar track_editor_editmode_status_ed_buf[512];
#define SEBUF_SIZE sizeof(track_editor_editmode_status_ed_buf)

/* jazz edit stuff */
static GtkWidget* jazzbox;
static GtkWidget* jazztable;
static GtkToggleButton* jazztoggles[32];
static int jazz_numshown = 0;
static gboolean jazz_enabled = FALSE;
static gboolean scroll_handled = FALSE;

/* Live recording stuff */
static gint8 cur_tick = 0;
static gdouble curtime = 0.0, prevtime = 0.0, nexttime = 0.0;
static gint cur_pos = 0, cur_pat = 0, cur_tempo = 0, prev_tempo = 0, patno = 0, bpm = 0;

static void vscrollbar_changed(GtkRange* scrollbar, Tracker* t);
static void hscrollbar_changed(GtkRange* scrollbar, Tracker* t);
static void update_mainmenu_blockmark(Tracker* t, int state);
static gboolean track_editor_handle_column_input(Tracker* t, int gdkkey);

/* Note recording stuff */
static struct {
    guint32 key;
    int chn;
    gboolean act;
} reckey[32];

static gint
track_editor_editmode_status_idle_function(void)
{
    gui_statusbar_update_message(track_editor_editmode_status_ed_buf, FALSE);

    track_editor_editmode_status_idle_handler = 0;
    return FALSE;
}

#define PRINT_STATUS(pos, ...) \
    g_snprintf(&track_editor_editmode_status_ed_buf[pos], SEBUF_SIZE - pos, __VA_ARGS__)

void show_editmode_status(void)
{
    Tracker* t = tracker;
    XMNote* note = &t->curpattern->channels[t->cursor_ch][t->patpos];
    const gchar *line = NULL;
    gint cmd_p1, cmd_p2, pos;

    static const gchar* fx_commands[] = {
        N_("Arpeggio"), /* 0 */
        N_("Porta up"), /* 1 */
        N_("Porta down"), /* 2 */
        N_("Tone porta"), /* 3 */
        N_("Vibrato"), /* 4 */
        N_("Tone porta + Volume slide"), /* 5 */
        N_("Vibrato + Volume slide"), /* 6 */
        N_("Tremolo"), /* 7 */
        N_("Set panning"), /* 8 */
        N_("Sample offset"), /* 9 */
        N_("Set volume"), /* A */
        N_("Position jump"), /* B */
        N_("Set volume"), /* C */
        N_("Pattern break"), /* D */
        NULL, /* E */
        N_("Set tempo/bpm"), /* F */
        N_("Set global volume"), /* G */
        N_("Global volume slide"), /* H */
        NULL, /* I */
        NULL, /* J */
        N_("Key off"), /* K */
        N_("Set envelop position"), /* L */
        NULL, /* M */
        NULL, /* N */
        NULL, /* O */
        N_("Panning slide"), /* P */
        N_("LP filter resonance"), /* Q */
        N_("Multi retrig note"), /* R */
        NULL, /* S */
        N_("Tremor"), /* T */
        NULL, /* U */
        NULL, /* V */
        NULL, /* W */
        NULL, /* X */
        NULL, /* Y */
        N_("LP filter cutoff"), /* Z */
    };

    static const gchar* e_fx_commands[] = {
        NULL, /* 0 */
        N_("Fine porta up"), /* 1 */
        N_("Fine porta down"), /* 2 */
        N_("Set gliss control"), /* 3 */
        N_("Set vibrato control"), /* 4 */
        N_("Set finetune"), /* 5 */
        N_("Pattern loop"), /* 6 */
        N_("Set tremolo control"), /* 7 */
        NULL, /* 8 */
        N_("Retrig note"), /* 9 */
        N_("Fine volume slide up"), /* A */
        N_("Fine volume slide down"), /* B */
        N_("Note cut"), /* C */
        N_("Note delay"), /* D */
        N_("Pattern delay"), /* E */
        NULL, /* F */
    };

    static const gchar* vol_fx_commands[] = {
        N_("Volume slide down"),
        N_("Volume slide up"),
        N_("Fine volume slide down"),
        N_("Fine volume slide up"),
        N_("Set vibrato speed"),
        N_("Vibrato"),
        N_("Set panning"),
        N_("Panning slide left"),
        N_("Panning slide right"),
        N_("Tone porta"),
    };

    static const gchar* e47_fx_forms[] = {
        N_("sine"), /* 0 */
        N_("ramp down"), /* 1 */
        N_("square"), /* 2 */
    };

    pos = PRINT_STATUS(0, _("[Chnn: %02d] [Pos: %03d] [Instr: %03d] [Vol: "), t->cursor_ch + 1,
        t->patpos,
        note->instrument);

    if (note->volume >= 0x10 && note->volume <= 0x50)
        pos += PRINT_STATUS(pos, _("Set volume"));
    else if (note->volume >= 0x60)
        pos += PRINT_STATUS(pos, "%s", _(vol_fx_commands[((note->volume & 0xf0) - 0x60) >> 4]));
    else
        pos += PRINT_STATUS(pos, _("None"));

    if (note->volume & 0xf0)
        pos += PRINT_STATUS(pos, " => %02d] ",
            (note->volume >= 0x10 && note->volume <= 0x50) ? note->volume - 0x10 : note->volume & 0xf);
    else
        pos += PRINT_STATUS(pos, "] ");

    pos += PRINT_STATUS(pos, _("[Cmd: "));

    switch (note->fxtype) {
    case xmpCmdArpeggio:
        if (note->fxparam)
            line = fx_commands[note->fxtype];
        break;
    case xmpCmdExtended:
        switch ((note->fxparam & 0xf0) >> 4) {
        case 0:
        case 8:
        case 15:
            break;
        default:
            line = e_fx_commands[(note->fxparam & 0xf0) >> 4];
            break;
        }
        break;
    case xmpCmdXPorta:
        switch ((note->fxparam & 0xf0) >> 4) {
        case 1:
            line = N_("Extra fine porta up");
            break;
        case 2:
            line = N_("Extra fine porta down");
            break;
        default:
            break;
        }
        break;
    default:
        line = fx_commands[note->fxtype];
        break;
    }
    pos += PRINT_STATUS(pos, "%s", line ? _(line) : _("None]"));

    cmd_p1 = (note->fxparam & 0xf0) >> 4;
    cmd_p2 = note->fxparam & 0xf;

    if (line) {
        switch (n_params[note->fxtype] & 3) {
        case 1:
        case 3:
            switch (note->fxtype) {
            case xmpCmdSpeed:
                if (note->fxparam < 32)
                    PRINT_STATUS(pos, _(" => tempo: %02d]"), note->fxparam);
                else
                    PRINT_STATUS(pos, _(" => BPM: %03d]"), note->fxparam);
                break;
            case xmpCmdOffset:
                PRINT_STATUS(pos, _(" => offset: %d]"), note->fxparam << 8);
                break;
            case xmpCmdExtended:
                if ((cmd_p1 == 4 || cmd_p1 == 7) && (cmd_p2 < 7) && cmd_p2 != 3)
                    /* Vibrato / tremolo control */
                    PRINT_STATUS(pos, " => %02d (%s)%s]", cmd_p2, _(e47_fx_forms[cmd_p2 & 3]),
                        cmd_p2 & 4 ? _(", continuous mode") : "");
                else if (cmd_p1 == 6) {
                    /* Pattern loop */
                    if (cmd_p2 == 0)
                        PRINT_STATUS(pos, _(" begin]"));
                    else
                        PRINT_STATUS(pos, _(" %02d times]"), cmd_p2);
                } else
                    PRINT_STATUS(pos, " => %02d]", cmd_p2);
                break;
            default:
                PRINT_STATUS(pos, " => %03d]", note->fxparam);
            }

            break;
        case 2:
            if (note->fxtype != xmpCmdArpeggio || note->fxparam) /* TODO detailed explanation of arpeggio */
                PRINT_STATUS(pos, " => %02d %02d]", cmd_p1, cmd_p2);
            break;
        default:
            PRINT_STATUS(pos, "]");
            break;
        }
    }

    if (!track_editor_editmode_status_idle_handler) {
        track_editor_editmode_status_idle_handler = g_idle_add(
            (GSourceFunc)track_editor_editmode_status_idle_function,
            NULL);
        g_assert(track_editor_editmode_status_idle_handler != 0);
    }
}

void track_editor_set_patpos(gint row)
{
    gtk_adjustment_set_value(adj, row);
}

void track_editor_set_num_channels(int n)
{
    int i;

    tracker_set_num_channels(tracker, n);

    // Remove superfluous togglebuttons from table
    for (i = n; i < jazz_numshown; i++) {
        g_object_ref(G_OBJECT(jazztoggles[i]));
        gtk_container_remove(GTK_CONTAINER(jazztable), GTK_WIDGET(jazztoggles[i]));
    }

    // Resize table
    gtk_table_resize(GTK_TABLE(jazztable), 1, n);

    // Add new togglebuttons to table
    for (i = jazz_numshown; i < n; i++) {
        gtk_table_attach_defaults(GTK_TABLE(jazztable), GTK_WIDGET(jazztoggles[i]), i, i + 1, 0, 1);
    }

    jazz_numshown = n;
}

static void
set_vscrollbar(Tracker* t,
    int patpos,
    GtkAdjustment* adj)
{
    g_signal_handler_block(vscrollbar, vscroll_tag);
    gtk_adjustment_set_value(adj, patpos);
    g_signal_handler_unblock(vscrollbar, vscroll_tag);
}

static void
update_vscrollbar(Tracker* t,
    int patpos,
    int patlen,
    int disprows,
    GtkAdjustment* adj)
{
    g_signal_handler_block(vscrollbar, vscroll_tag);
    gtk_adjustment_configure(adj, patpos, 0, patlen + disprows - 1, 1, disprows, disprows);
    g_signal_handler_unblock(vscrollbar, vscroll_tag);
}

static void
set_hscrollbar(Tracker* t,
    int leftchan,
    GtkAdjustment* adj)
{
    g_signal_handler_block(hscrollbar, hscroll_tag);
    gtk_adjustment_set_value(adj, leftchan);
    g_signal_handler_unblock(hscrollbar, hscroll_tag);
}

static void
update_hscrollbar(Tracker* t,
    int leftchan,
    int numchans,
    int dispchans,
    GtkAdjustment* adj)
{
    g_signal_handler_block(hscrollbar, hscroll_tag);
    gtk_adjustment_configure(adj, leftchan, 0, numchans, 1, dispchans, dispchans);//or numchans - 1???
    g_signal_handler_unblock(hscrollbar, hscroll_tag);
}

static void
set_vscrollbar2(Tracker* t,
    int patpos,
    GtkAdjustment* adj)
{
    gtk_adjustment_set_value(adj, patpos);
}

static void
update_vscrollbar2(Tracker* t,
    int patpos,
    int patlen,
    int disprows,
    GtkAdjustment* adj)
{
    gtk_adjustment_configure(adj, patpos, 0, patlen + disprows - 1, 1, disprows, disprows);
}

static void
set_hscrollbar2(Tracker* t,
    int leftchan,
    GtkAdjustment* adj)
{
    gtk_adjustment_set_value(adj, leftchan);
}

static void
update_hscrollbar2(Tracker* t,
    int leftchan,
    int numchans,
    int dispchans,
    GtkAdjustment* adj)
{
    gtk_adjustment_configure(adj, leftchan, 0, numchans, 1, dispchans, dispchans);//or numchans - 1???
}

struct NoteArg {
    gint channel, row;
    XMNote note;
};

static void
note_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    XMNote tmp_value;
    Tracker* t;
    struct NoteArg* na = arg;

    g_assert(IS_TRACKER(data));
    t = TRACKER(data);
    tmp_value = t->curpattern->channels[na->channel][na->row];
    t->curpattern->channels[na->channel][na->row] = na->note;
    na->note = tmp_value;

    tracker_set_cursor_channel(t, na->channel);
    track_editor_set_patpos(na->row);
}

void
track_editor_log_note(Tracker* t,
    const gchar* title,
    const gint pattern, const gint channel, const gint row,
    const gint flags)
{
    struct NoteArg* arg = g_new(struct NoteArg, 1);

    arg->channel = channel;
    arg->row = row;
    arg->note = (pattern == -1 ? t->curpattern : &xm->patterns[pattern])->channels[channel][row];
    history_log_action(HISTORY_ACTION_POINTER, _(title), flags | HISTORY_FLAG_LOG_POS |
        (pattern == -1 ? HISTORY_FLAG_LOG_PAT : HISTORY_SET_PAT(pattern)), note_undo,
        t, sizeof(struct NoteArg), arg);
}

typedef struct
{
    gint width, length, channel, pos;
    /* Manual 2D indexing is supposed if needed */
    XMNote notes[1];
} BlockArg;

static void track_editor_block_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    Tracker* t;
    BlockArg* ba = arg;
    const gsize chansize = ba->length * sizeof(XMNote);
    const gsize size = ba->width * chansize;
    XMNote* tmp_notes = g_malloc(size);
    gint i, chaddr;

    g_assert(IS_TRACKER(data));
    t = TRACKER(data);

    /* Current pattern is already set by undo routines */
    for (i = 0, chaddr = 0; i < ba->width; i++, chaddr += ba->length) {
        memcpy(&tmp_notes[chaddr],
            &t->curpattern->channels[ba->channel + i][ba->pos], chansize);
        memcpy(&t->curpattern->channels[ba->channel + i][ba->pos],
            &ba->notes[chaddr], chansize);
    }
    memcpy(ba->notes, tmp_notes, size);

    g_free(tmp_notes);
    tracker_redraw(t);
}

void track_editor_log_block(Tracker* t, const gchar* title,
    const gint channel, const gint pos, const gint width, const gint length)
{
    const gsize chansize = length * sizeof(XMNote);
    const gsize asize = sizeof(BlockArg) + sizeof(XMNote) * (length * width - 1);
    gint i, chaddr;
    /* We already have 1 XMNote allocated */
    BlockArg* arg = g_malloc(asize);

    arg->width = width;
    arg->length = length;
    arg->channel = channel;
    arg->pos = pos;
    for (i = 0, chaddr = 0; i < width; i++, chaddr += length)
        memcpy(&arg->notes[chaddr], &t->curpattern->channels[channel + i][pos], chansize);

    history_log_action(HISTORY_ACTION_POINTER, _(title),
        HISTORY_FLAG_LOG_POS | HISTORY_FLAG_LOG_PAT | HISTORY_FLAG_LOG_PAGE,
        track_editor_block_undo, t, asize, arg);
}

static void
track_editor_parameter_scroll(Tracker* t,
    gint cur_ch,
    gint cur_item,
    gint patpos,
    gint direction)
{

    if (GUI_EDITING) {
        guint8 vol, n_p, fxtype;
        gint fxparam, fxparam1;
        gboolean redraw = FALSE;
        XMNote* note_entry = &t->curpattern->channels[cur_ch][patpos];

        vol = note_entry->volume;
        fxparam = note_entry->fxparam;

        switch (cur_item) {
        case 3: /* Volume column */
        case 4:
            if (!vol) { /* No volume specified => take it from sample basic value */
                gint ins = note_entry->instrument;

                if (ins)
                    vol = xm->instruments[ins - 1].samples[xm->instruments[ins - 1].samplemap[note_entry->note - 1]].volume + 0x10;
            }
            if (vol >= 0x10 && vol <= 0x50) {
                vol = CLAMP(vol + direction, 0x10, 0x50);
                redraw = TRUE;
            } else if (vol >= 0x60) {
                gint8 nibble = vol & 0xf;

                nibble = CLAMP(nibble + direction, 0, 0xf);
                vol = (vol & 0xf0) | nibble;
                redraw = TRUE;
            }
            break;
        case 5 ... 7:
            fxtype = note_entry->fxtype;
            if ((fxtype || fxparam != 0) && fxtype < 36) { /* Valid XMs, non-empty effect */
                n_p = n_params[fxtype];

                switch (n_p & 3) {
                case 1:
                    fxparam = CLAMP(fxparam + direction, 0, (n_p & NG_0x40) ? 0x40 : 0xff);
                    redraw = TRUE;
                    break;
                case 3:
                    fxparam1 = fxparam & 0xf0;
                    fxparam = CLAMP((fxparam & 0xf) + direction, 0, 0xf) | fxparam1;
                    redraw = TRUE;
                    break;
                case 2:
                    fxparam1 = fxparam & 0xf0;
                    fxparam = fxparam & 0xf;
                    if (cur_item == 7)
                        fxparam = CLAMP(fxparam + direction, 0, 0xf);
                    else
                        fxparam1 = CLAMP(fxparam1 + direction * 16, 0, 0xf0);
                    fxparam |= fxparam1;
                    redraw = TRUE;
                    break;
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
        if (redraw) {
            static gpointer prev_pattern = NULL;
            static gint prev_ch = -1, prev_patpos = -1;

            if (t->curpattern != prev_pattern || prev_ch != cur_ch || prev_patpos != patpos) {
                prev_pattern = t->curpattern;
                prev_ch = cur_ch;
                prev_patpos = patpos;
                track_editor_log_note(t, N_("Volume / FX parameter scrolling"),
                    -1, cur_ch, patpos, HISTORY_FLAG_LOG_PAGE);
            }
            note_entry->volume = vol;
            note_entry->fxparam = fxparam;
            tracker_redraw_row(t, patpos);
        }
        scroll_handled = TRUE;
    }
}

void tracker_page_create(GtkNotebook* nb)
{
    GtkWidget *vbox, *hbox, *table, *thing;
    int i;

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(vbox);

    jazzbox = hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    thing = gtk_label_new(_("Jazz Edit:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    jazztable = table = gtk_table_new(1, 1, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, TRUE, 0);
    gtk_widget_show(table);

    for (i = 0; i < 32; i++) {
        char buf[10];
        g_sprintf(buf, "%02d", i + 1);
        thing = gtk_toggle_button_new_with_label(buf);
        jazztoggles[i] = GTK_TOGGLE_BUTTON(thing);
        gtk_widget_show(thing);
    }

    table = gtk_table_new(2, 2, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);
    gtk_widget_show(table);

    thing = tracker_new();
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 0, 1);
    gtk_widget_show(thing);
    g_signal_connect(thing, "mainmenu_blockmark_set", G_CALLBACK(update_mainmenu_blockmark), NULL);
    g_signal_connect(thing, "alt_scroll", G_CALLBACK(track_editor_parameter_scroll), NULL);
    tracker = TRACKER(thing);

    tracker_set_update_freq(gui_settings.tracker_update_freq);

    trackersettings = trackersettings_new();
    trackersettings_set_tracker_widget(TRACKERSETTINGS(trackersettings), tracker);

    hscrollbar = gtk_hscrollbar_new(NULL);
    adj = gtk_range_get_adjustment(GTK_RANGE(hscrollbar));
    gtk_table_attach(GTK_TABLE(table), hscrollbar, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
    gtk_widget_show(hscrollbar);
    g_signal_connect(thing, "xpanning", G_CALLBACK(set_hscrollbar), adj);
    g_signal_connect(thing, "xconf", G_CALLBACK(update_hscrollbar), adj);
    hscroll_tag = g_signal_connect(hscrollbar, "value-changed",
                                   G_CALLBACK(hscrollbar_changed), thing);

    vscrollbar = gtk_vscrollbar_new(NULL);
    adj = gtk_range_get_adjustment(GTK_RANGE(vscrollbar));
    gtk_table_attach(GTK_TABLE(table), vscrollbar, 1, 2, 0, 1, 0, GTK_FILL, 0, 0);
    gtk_widget_show(vscrollbar);
    g_signal_connect(thing, "patpos", G_CALLBACK(set_vscrollbar), adj);
    g_signal_connect(thing, "yconf", G_CALLBACK(update_vscrollbar), adj);
    vscroll_tag = g_signal_connect(vscrollbar, "value-changed",
                                   G_CALLBACK(vscrollbar_changed), thing);

    gtk_notebook_append_page(nb, vbox, gtk_label_new(_("Tracker")));
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    trackersettings_apply_font(TRACKERSETTINGS(trackersettings));

    thing = gui_get_widget(gui_builder, "track_editor_popup_menu", UI_FILE);
    gui_popup_menu_attach(thing, &tracker->widget, NULL);
}

static void
update_mainmenu_blockmark(Tracker* t,
    int state)
{
    menubar_block_mode_set((gboolean)state, TRUE);
}

static void
vscrollbar_changed(GtkRange* scrollbar, Tracker* t)
{
    GtkAdjustment* adj = gtk_range_get_adjustment(scrollbar);

    tracker_set_patpos(t, rint(gtk_adjustment_get_value(adj)));
}

static void
hscrollbar_changed(GtkRange* scrollbar, Tracker* t)
{
    GtkAdjustment* adj = gtk_range_get_adjustment(scrollbar);

    tracker_set_xpanning(t, rint(gtk_adjustment_get_value(adj)));
}

void track_editor_toggle_jazz_edit(GtkCheckMenuItem* b)
{
    jazz_enabled = gtk_check_menu_item_get_active(b);
    (jazz_enabled ? gtk_widget_show : gtk_widget_hide)(jazzbox);
}

static int
track_editor_find_next_jazz_channel(int current_channel)
{
    /* Jazz Edit Mode. The user has selected some channels that we can
       use. Go to the next one where no sample is currently
       playing. */

    int j, n;

    for (j = 0, n = (current_channel + 1) % xm->num_channels;
         j < xm->num_channels - 1;
         j++, n = (n + 1) % xm->num_channels) {
        if (jazztoggles[n]->active && note_running[n] < 0) {
            return n;
        }
    }

    return current_channel;
}

/* Returns the channel number on which the specified note is running,
   or -1 if no such note is found. If was == TRUE, look for the channel
   when the note was running */
static gint
note_is_running(const guint note, const gboolean was)
{
    guint i;
    for (i = 0; i < ARRAY_SIZE(note_running); i++)
        if (was) {
            if (note_running[i] == -(note + 1))
                return i;
        } else {
            if (note_running[i] == note + 1)
                return i;
        }

    return -1;
}

gboolean track_editor_do_the_note_key(int notekeymeaning,
    gboolean pressed,
    guint32 xkeysym,
    int modifiers,
    gboolean always_poly)
{
    static int playchan = 0, trychan = 0;

    int n;
    int note;

    note = notekeymeaning + 12 * gui_get_current_octave_value() + 1;

    if (!(note > 0 && note <= NUM_NOTES))
        return FALSE;

    if (pressed) {
        if (note_is_running(note, FALSE) == -1) {
            if (gui_settings.try_polyphony &&
                (always_poly || (!GTK_TOGGLE_BUTTON(editing_toggle)->active)) &&
                gui_playing_mode != PLAYING_SONG &&
                gui_playing_mode != PLAYING_PATTERN) {
                gint cur_chan = -1, playchan;

                if (!gui_settings.repeat_same_note)
                    cur_chan = note_is_running(note, TRUE);
                gui_play_note(playchan = cur_chan == -1 ? trychan : cur_chan, note, TRUE);
                note_running[playchan] = note + 1;
                /* All 32 channels are using for trying independent on
                   actual number of channels used */
                if (cur_chan == -1)
                    trychan = (trychan + 1) & 31;
            } else {
                if (jazz_enabled) {
                    if (GTK_TOGGLE_BUTTON(editing_toggle)->active) {
                        gui_play_note(tracker->cursor_ch, note, FALSE);
                        note_running[tracker->cursor_ch] = note + 1;
                        n = track_editor_find_next_jazz_channel(tracker->cursor_ch);
                        tracker_step_cursor_channel(tracker, n - tracker->cursor_ch);
                    } else {
                        playchan = track_editor_find_next_jazz_channel(playchan);
                        gui_play_note(playchan, note, FALSE);
                        note_running[playchan] = note + 1;
                    }
                } else {
                    gui_play_note(tracker->cursor_ch, note, FALSE);
                    note_running[tracker->cursor_ch] = note + 1;
                }
            }
            return TRUE; /* New note */
        }
        return FALSE; /* Key repeate from keyboard */
    } else {
        gint pc;

        if ((pc = note_is_running(note, FALSE)) != -1) {
            if (keys_is_key_pressed(xkeysym, modifiers)) {
                /* this is just an auto-repeat fake keyoff. pooh.
                   in reality this key is still being held down */
                return FALSE;
            }
            gui_play_note_keyoff(pc);
        }
        note_running[pc] = -(note + 1);
        return TRUE;
    }
}

#define EDITING_PATTERN (gui_playing_mode == PLAYING_PATTERN ? \
    tracker->curpattern : &xm->patterns[xm->pattern_order_table[action_pat]])

static void
track_editor_put_note(guint ch, guint nn, guint instr)
{
    gdouble press_time = current_driver->get_play_time(current_driver_object) - gui_settings.delay_comp;
    gint8 action_tick = cur_tick;
    gint action_pos = cur_pos;
    gint action_pat = cur_pat;
    XMNote *note, newnote;

    if (gui_settings.precise_timing) {
        /* This solution is not perfect since it can calculate only +- 1 tick shift.
           At high BPM this may be not true. But thorough calculations are much more difficult.
           Sorry... */
        if (press_time > (nexttime + curtime) * 0.5) {
            if (++action_tick >= cur_tempo) {
                /* Roll over the pattern line boundary */
                action_tick = 0;
                action_pos++;
            }
        } else if (prevtime > 0.0 && press_time < (curtime + prevtime) * 0.5) {
            if (--action_tick < 0) {
                /* Roll over the pattern line boundary */
                if (prev_tempo == 0)
                    prev_tempo = cur_tempo;
                action_tick = prev_tempo - 1;
                action_pos--;
            }
        }
    } else {
        action_tick = 0;
        /* The key is pressed later than the half of the line -- moving it to the next line */
        if (press_time >
            curtime + (nexttime - curtime) * ((gdouble)cur_tempo * 0.5 - (gdouble)cur_tick))
            action_pos++;
    }

    /* Roll over the pattern boundary */
    if (action_pos < 0) {
        if (--action_pat < 0)
            /* Roll over the song boundary */
            action_pat = xm->song_length - 1;
        action_pos = EDITING_PATTERN->length - 1;
    }
    if (action_pos >= EDITING_PATTERN->length) {
        if (++action_pat >= xm->song_length)
            /* Roll over the song boundary */
            action_pat = 0;
        action_pos = 0;
    }

    note = &EDITING_PATTERN->channels[ch][action_pos];
    newnote = *note;
    if (action_tick == 0) {
        newnote.note = nn;
        newnote.instrument = instr;
    } else {
        if (nn == XM_PATTERN_NOTE_OFF) {
            /* Key release */
            if (note->fxtype == 0 && note->fxparam == 0) {
                /* Effect field is free; we can use 'k' effect */
                newnote.fxtype = xmpCmdKeyOff;
                newnote.fxparam = action_tick;
            } else {
                /* Effect field is occupied; we can nothing to do else than just put key off
                   at the next line. Regard the possible rollovers. */
                if (++action_pos >= xm->patterns[xm->pattern_order_table[action_pat]].length) {
                    if (++action_pat >= xm->song_length)
                        /* Roll over the song boundary */
                        action_pat = 0;
                    action_pos = 0;
                }
                note = &xm->patterns[xm->pattern_order_table[action_pat]].channels[ch][action_pos];
                newnote = *note;
                newnote.note = nn;
                newnote.instrument = instr;
            }
        } else {
            /* We use 'ed' effect to record a note at the proper tick */
            newnote.note = nn;
            newnote.instrument = instr;
            newnote.fxtype = xmpCmdExtended;
            newnote.fxparam = XMP_CMD_EXTENDED(xmpCmdDelayNote, action_tick);
        }
    }

    track_editor_log_note(tracker, N_("Note input"), xm->pattern_order_table[action_pat],
        ch, action_pos, HISTORY_FLAG_LOG_PAGE);
    *note = newnote;
    if (action_pat == cur_pat)
        /* Redrawing is needed only if note is recorded in the current pattern */
        tracker_redraw_row(tracker, action_pos);
}

static gboolean
move_notes(Tracker* t,
    const gint channel,
    const gint pos,
    const gint len,
    const gint dir)
{
    gboolean handled = FALSE;
    gint i;
    XMNote* notes = t->curpattern->channels[channel];

    for (i = pos + dir; (dir == -1 && i >= 0) || (dir == 1 && i < len); i += dir) {
        if (!notes[i].note && !notes[i].instrument && !notes[i].volume
            && !notes[i].fxtype && !notes[i].fxparam) {
                handled = TRUE;
                break;
            }
    }
    if (handled) {
        track_editor_log_block(t, _("Notes moving"), channel,
            dir == -1 ? i : pos, 1, abs(pos - i) + 1);
        memmove(&notes[dir == -1 ? i : pos + 1],
            &notes[dir == -1 ? i + 1: pos], abs(pos - i) * sizeof(XMNote));
        memset(&notes[pos], 0, sizeof(XMNote));
    }

    return handled;
}

gboolean
track_editor_handle_keys(const int shift,
    const int ctrl,
    const int alt,
    const guint32 keyval,
    const gboolean pressed)
{
    int c, i, m, tip;
    Tracker* t = tracker;
    gboolean handled = FALSE;
    gboolean in_sel = tracker_is_in_selection_mode(t);
    const gboolean sel_ft2 = gui_settings.selection_mode == SELECTION_FT2 ||
        gui_settings.selection_mode == SELECTION_MIXED;

    m = i = keys_get_key_meaning(keyval, ENCODE_MODIFIERS(shift, ctrl, alt), -1);
    tip = KEYS_MEANING_TYPE(i);

    if (i != -1) {
        if (tip == KEYS_MEANING_NOTE && !GTK_TOGGLE_BUTTON(editing_toggle)->active)
            track_editor_do_the_note_key(m, pressed, keyval, ENCODE_MODIFIERS(shift, ctrl, alt), FALSE);

        if (t->cursor_item == 0) {
            if (tip == KEYS_MEANING_NOTE) {
                i += 12 * gui_get_current_octave_value() + 1;

                if (GTK_TOGGLE_BUTTON(editing_toggle)->active) {
                    if (i <= NUM_NOTES) {
                        if (t->cursor_item == 0) {
                            if (!jazztoggles[tracker->cursor_ch]->active && jazz_enabled) {
                                int n = track_editor_find_next_jazz_channel(tracker->cursor_ch);
                                tracker_step_cursor_channel(tracker, n - tracker->cursor_ch);
                            }

                            if (!GUI_ENABLED && !ASYNCEDIT) { // Recording mode
                                if (pressed) { // Insert note

                                    for (c = 0; c < 32; c++) { // Cleanup
                                        if (!reckey[c].act)
                                            continue;
                                        if (reckey[c].key == keyval)
                                            break; // Key is allready down
                                        else if (reckey[c].chn == t->cursor_ch)
                                            reckey[c].act = 0; // There can be only one sound per channel
                                    }

                                    if (c == 32) {
                                        // Find free reckey
                                        for (c = 0; c < 32; c++)
                                            if (!reckey[c].act)
                                                break;

                                        // Fill in the reckey
                                        reckey[c].key = keyval;
                                        reckey[c].chn = t->cursor_ch;
                                        reckey[c].act = TRUE;
                                        track_editor_put_note(t->cursor_ch, i, gui_get_current_instrument());
                                    }
                                } else if (!keys_is_key_pressed(keyval,
                                               ENCODE_MODIFIERS(shift, ctrl, alt))) { // Release key

                                    // Find right reckey
                                    for (c = 0; c < 32; c++)
                                        if (reckey[c].act && reckey[c].key == keyval)
                                            break;
                                    if (c < 32) { /* Check if the key was not released by other means */
                                        reckey[c].act = FALSE;

                                        if (gui_settings.insert_noteoff)
                                            track_editor_put_note(reckey[c].chn, XM_PATTERN_NOTE_OFF, 0);
                                    }
                                }
                            } else if (pressed) {
                                gint jump = gui_get_current_jump_value();
                                XMNote* note = &t->curpattern->channels[t->cursor_ch][t->patpos];

                                track_editor_log_note(t, N_("Note input"),
                                    -1, t->cursor_ch, t->patpos, HISTORY_FLAG_LOG_PAGE);
                                note->note = i;
                                note->instrument = gui_get_current_instrument();
                                if (jump)
                                    tracker_step_cursor_row(t, jump);
                                else
                                    tracker_redraw_current_row(t);
                            }
                        }
                    }
                    show_editmode_status();
                    track_editor_do_the_note_key(m, pressed, keyval, ENCODE_MODIFIERS(shift, ctrl, alt), FALSE);
                }

                return TRUE;
            } else if (tip == KEYS_MEANING_KEYOFF) {
                if (GTK_TOGGLE_BUTTON(editing_toggle)->active) {
                    if (pressed) {
                        gint jump = gui_get_current_jump_value();
                        XMNote* note = &t->curpattern->channels[t->cursor_ch][t->patpos];

                        track_editor_log_note(t, N_("Note input"),
                            -1, t->cursor_ch, t->patpos, HISTORY_FLAG_LOG_PAGE);
                        note->note = XM_PATTERN_NOTE_OFF;
                        note->instrument = 0;
                        if (jump)
                            tracker_step_cursor_row(t, gui_get_current_jump_value());
                        else
                            tracker_redraw_current_row(t);
                    }
                    show_editmode_status();
                }

                return TRUE;
            }
        }
    }

    if (!pressed) {
        /* Preventing instrument selector showing up after parameter scrolling */
        if (scroll_handled &&
            (keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Meta_L)) {
            scroll_handled = FALSE;
            return TRUE;
        }
        /* Finishing selection in the FT2 mode */
        if (keyval == GDK_Shift_L && in_sel &&
            gui_settings.selection_mode == SELECTION_FT2) {
            menubar_block_mode_set(FALSE, FALSE);
            return TRUE;
        }
        return FALSE;
    }

    switch (tip) {
    case KEYS_MEANING_FUNC:
        switch (KEYS_MEANING_VALUE(i)) {
        case KEY_JMP_PLUS:
            m = gui_get_current_jump_value() + 1;
            if (m <= 16)
                gui_set_jump_value(m);
            handled = TRUE;
            break;
        case KEY_JMP_MINUS:
            m = gui_get_current_jump_value() - 1;
            if (m >= 0)
                gui_set_jump_value(m);
            handled = TRUE;
            break;
        case KEY_PLAY_ROW:
            gui_play_current_pattern_row();
            handled = TRUE;
            break;
        case KEY_PLAY_BLK:
            gui_play_block();
            handled = TRUE;
            break;
        default:
            break;
        }
        if (handled)
            return TRUE;
        break;
    case KEYS_MEANING_CH:
        tracker_set_cursor_channel(t, KEYS_MEANING_VALUE(i));
        if (GTK_TOGGLE_BUTTON(editing_toggle)->active)
            show_editmode_status();
        return TRUE;
    }

    switch (keyval) {
    case GDK_Up:
        if ((GUI_ENABLED || ASYNCEDIT) && !ctrl) {
            if (!alt) {
                if (shift && !in_sel && sel_ft2) {
                /* Start selection */
                    in_sel = TRUE;
                    menubar_block_mode_set(TRUE, FALSE);
                }
                track_editor_set_patpos(t->patpos > 0 ? t->patpos - 1 : t->curpattern->length - 1);
                handled = TRUE;
            } else if (!shift) {
                handled = move_notes(t, t->cursor_ch,
                    t->patpos, t->curpattern->length, -1);
                if (handled) {
                    track_editor_set_patpos(t->patpos - 1);
                    tracker_redraw(t);
                }
            }
        }
        break;
    case GDK_Down:
        if ((GUI_ENABLED || ASYNCEDIT) && !ctrl) {
            if (!alt) {
                if (shift && !in_sel && sel_ft2) {
                /* Start selection */
                    in_sel = TRUE;
                    menubar_block_mode_set(TRUE, FALSE);
                }
                track_editor_set_patpos(t->patpos < t->curpattern->length - 1 ? t->patpos + 1 : 0);
                handled = TRUE;
            } else if (!shift) {
                handled = move_notes(t, t->cursor_ch,
                    t->patpos, t->curpattern->length, 1);
                if (handled) {
                    track_editor_set_patpos(t->patpos + 1);
                    tracker_redraw(t);
                }
            }
        }
        break;
    case GDK_Page_Up:
        if ((GUI_ENABLED || ASYNCEDIT) && !ctrl && !alt) {
            if (shift && !in_sel && sel_ft2) {
            /* Start selection */
                in_sel = TRUE;
                menubar_block_mode_set(TRUE, FALSE);
            }
            if (t->patpos >= 16)
                track_editor_set_patpos(t->patpos - 16);
            else
                track_editor_set_patpos(0);
            handled = TRUE;
        }
        break;
    case GDK_Page_Down:
        if ((GUI_ENABLED || ASYNCEDIT) && !ctrl && !alt) {
            if (shift && !in_sel && sel_ft2) {
            /* Start selection */
                in_sel = TRUE;
                menubar_block_mode_set(TRUE, FALSE);
            }
            if (t->patpos < t->curpattern->length - 16)
                track_editor_set_patpos(t->patpos + 16);
            else
                track_editor_set_patpos(t->curpattern->length - 1);
            handled = TRUE;
        }
        break;
    case GDK_Home:
    case GDK_F9:
        if ((GUI_ENABLED || ASYNCEDIT) && !ctrl && !alt) {
            if (shift && !in_sel && sel_ft2) {
            /* Start selection */
                in_sel = TRUE;
                menubar_block_mode_set(TRUE, FALSE);
            }
            track_editor_set_patpos(0);
            handled = TRUE;
        }
        break;
    case GDK_F10:
        if ((GUI_ENABLED || ASYNCEDIT) && !ctrl && !alt) {
            if (shift && !in_sel && sel_ft2) {
            /* Start selection */
                in_sel = TRUE;
                menubar_block_mode_set(TRUE, FALSE);
            }
            track_editor_set_patpos(t->curpattern->length / 4);
            handled = TRUE;
        }
        break;
    case GDK_F11:
        if ((GUI_ENABLED || ASYNCEDIT) && !ctrl && !alt) {
            if (shift && !in_sel && sel_ft2) {
            /* Start selection */
                in_sel = TRUE;
                menubar_block_mode_set(TRUE, FALSE);
            }
            track_editor_set_patpos(t->curpattern->length / 2);
            handled = TRUE;
        }
        break;
    case GDK_F12:
        if ((GUI_ENABLED || ASYNCEDIT) && !ctrl && !alt) {
            if (shift && !in_sel && sel_ft2) {
            /* Start selection */
                in_sel = TRUE;
                menubar_block_mode_set(TRUE, FALSE);
            }
            track_editor_set_patpos(3 * t->curpattern->length / 4);
            handled = TRUE;
        }
        break;
    case GDK_End:
        if ((GUI_ENABLED || ASYNCEDIT) && !ctrl && !alt) {
            if (shift && !in_sel && sel_ft2) {
            /* Start selection */
                in_sel = TRUE;
                menubar_block_mode_set(TRUE, FALSE);
            }
            track_editor_set_patpos(t->curpattern->length - 1);
            handled = TRUE;
        }
        break;
    case GDK_Left:
        if (!ctrl && !alt) {
            if (shift && !in_sel && sel_ft2) {
            /* Start selection */
                in_sel = TRUE;
                menubar_block_mode_set(TRUE, FALSE);
                handled = TRUE;
            }
            if (!shift || sel_ft2) {
                /* cursor left */
                in_sel ? tracker_step_cursor_channel(t, -1)
                    : tracker_step_cursor_item(t, -1);
                handled = TRUE;
            }
        }
        break;
    case GDK_Right:
        if (!ctrl && !alt) {
            if (shift && !in_sel && sel_ft2) {
            /* Start selection */
                in_sel = TRUE;
                menubar_block_mode_set(TRUE, FALSE);
                handled = TRUE;
            }
            if (!shift || sel_ft2) {
                /* cursor right */
                in_sel ? tracker_step_cursor_channel(t, 1)
                    : tracker_step_cursor_item(t, 1);
                handled = TRUE;
            }
        }
        break;
    case GDK_Tab:
    case GDK_ISO_Left_Tab:
        tracker_step_cursor_channel(t, shift ? -1 : 1);
        handled = TRUE;
        break;
    case GDK_Delete:
        if (GTK_TOGGLE_BUTTON(editing_toggle)->active) {
            gint jump = gui_get_current_jump_value();
            XMNote* note = &t->curpattern->channels[t->cursor_ch][t->patpos];

            track_editor_log_note(t, N_("Note cleaning"),
                -1, t->cursor_ch, t->patpos, HISTORY_FLAG_LOG_PAGE);
            if (shift) {
                note->note = 0;
                note->instrument = 0;
                note->volume = 0;
                note->fxtype = 0;
                note->fxparam = 0;
            } else if (ctrl) {
                note->volume = 0;
                note->fxtype = 0;
                note->fxparam = 0;
            } else if (alt) {
                note->fxtype = 0;
                note->fxparam = 0;
            } else {
                switch (t->cursor_item) {
                case 0:
                case 1:
                case 2:
                    note->note = 0;
                    note->instrument = 0;
                    break;
                case 3:
                case 4:
                    note->volume = 0;
                    break;
                case 5:
                case 6:
                case 7:
                    note->fxtype = 0;
                    note->fxparam = 0;
                    break;
                default:
                    g_assert_not_reached();
                    break;
                }
            }

            if (jump)
                tracker_step_cursor_row(t, jump);
            else
                tracker_redraw_current_row(t);
            handled = TRUE;
        }
        break;
    case GDK_Insert:
        if (GTK_TOGGLE_BUTTON(editing_toggle)->active && !shift && !alt && !ctrl) {
            XMNote* note = &t->curpattern->channels[t->cursor_ch][t->patpos];

            track_editor_log_block(t, N_("Note inserting"),
                t->cursor_ch, t->patpos, 1, t->curpattern->length - t->patpos);
            for (i = t->curpattern->length - 1; i > t->patpos; --i)
                t->curpattern->channels[t->cursor_ch][i] = t->curpattern->channels[t->cursor_ch][i - 1];

            note->note = 0;
            note->instrument = 0;
            note->volume = 0;
            note->fxtype = 0;
            note->fxparam = 0;

            tracker_redraw(t);
            handled = TRUE;
        }
        break;
    case GDK_BackSpace:
        if (GTK_TOGGLE_BUTTON(editing_toggle)->active && !shift &&!ctrl && !alt) {
            XMNote* note;

            if (t->patpos) {
                track_editor_log_block(t, N_("Note removing"),
                    t->cursor_ch, t->patpos - 1, 1, t->curpattern->length - t->patpos + 1);
                for (i = t->patpos - 1; i < t->curpattern->length - 1; i++)
                    t->curpattern->channels[t->cursor_ch][i] = t->curpattern->channels[t->cursor_ch][i + 1];

                note = &t->curpattern->channels[t->cursor_ch][t->curpattern->length - 1];
                note->note = 0;
                note->instrument = 0;
                note->volume = 0;
                note->fxtype = 0;
                note->fxparam = 0;

                tracker_redraw(t);
                track_editor_set_patpos(t->patpos - 1);
                handled = TRUE;
            }
        }
        break;

    case GDK_Escape:
        if (shift) {
            tracker_set_cursor_item(t, 0);
            handled = TRUE;
        }
        break;
    default:
        if (!ctrl && !alt) {
            if (GTK_TOGGLE_BUTTON(editing_toggle)->active) {
                handled = track_editor_handle_column_input(t, keyval);
            }
        }
        break;
    }

    if (GTK_TOGGLE_BUTTON(editing_toggle)->active && handled)
        show_editmode_status();

    return handled;
}

void track_editor_delete_line(GtkWidget* w, Tracker* t)
{
    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        XMPattern* pat = t->curpattern;

        gui_log_pattern_full(pat, N_("Pattern line removing"), -1, -1, -1, t->patpos);
        st_pattern_delete_line(pat, t->patpos);
        if (gui_settings.change_patlen && pat->length > 1) {
            st_set_pattern_length(pat, pat->length - 1);
            gui_update_pattern_data();
            tracker_set_pattern(tracker, NULL);
            tracker_set_pattern(tracker, pat);
        } else
            tracker_redraw(t);
    }
}

void track_editor_insert_line(GtkWidget* w, Tracker* t)
{
    GtkWidget* focus_widget = GTK_WINDOW(mainwindow)->focus_widget;

    if (GTK_IS_ENTRY(focus_widget)) { /* Emulate Shift + Ins if the cursor is in an entry */
        g_signal_emit_by_name(focus_widget, "paste-clipboard", NULL);
        return;
    }

    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        XMPattern* pat = t->curpattern;

        gui_log_pattern_full(pat, N_("Pattern line insertion"), -1, -1,
            gui_settings.change_patlen ? MIN(pat->length + 1, 256) : -1, t->patpos);
        st_pattern_insert_line(pat, t->patpos);
        if (gui_settings.change_patlen && pat->length < 256) {
            st_set_pattern_length(pat, pat->length + 1);
            gui_update_pattern_data();
            tracker_set_pattern(tracker, NULL);
            tracker_set_pattern(tracker, pat);
        } else
            tracker_redraw(t);
    }
}

void track_editor_clear_buffers(GtkWidget* w, Tracker* t)
{
    track_buffer.pattern.length = 0;
    pattern_buffer.pattern.length = 0;
    block_buffer.pattern.length = 0;
}

static void
copy_notes(BlockBuffer* dstb,
    const XMPattern* src,
    const guint start_row,
    const guint length,
    const guint start_ch,
    const guint numch,
    const gboolean cut)
{
    guint mask = gui_settings.cpt_mask;
    XMPattern* dst = &dstb->pattern;
    gboolean need_clear = FALSE;
    gint i, j;

    g_assert(start_row + length <= 256 && start_ch + numch <= 32);

    if (mask & MASK_ENABLE) {
        const gboolean action = cut ? MASK_CUT : MASK_COPY;

        /* Accumulation works only if the source and destination lengths
           are equal. Otherwise we clear the buffer */
        if (dst->length != length) {
            dst->length = length;
            need_clear = TRUE;
        }
        if (dstb->num_chans != numch) {
            dstb->num_chans = numch;
            need_clear = TRUE;
        }
        if (need_clear)
            for (i = 0; i < numch; i++)
                memset(dst->channels[i], 0, length * sizeof(XMNote));

        for (i = 0; i < length; i++)
            for (j = 0; j < numch && src->channels[j + start_ch]; j++) {
                XMNote* dest = &dst->channels[j][i];
                XMNote* source = &src->channels[j + start_ch][i + start_row];

                if ((mask & cpt_masks[action + MASK_NOTE]) &&
                    (!((mask & cpt_masks[MASK_TRANSP + MASK_NOTE]) && dest->note)))
                    dest->note = source->note;
                if ((mask & cpt_masks[action + MASK_INSTR]) &&
                    (!((mask & cpt_masks[MASK_TRANSP + MASK_INSTR]) && dest->instrument)))
                    dest->instrument = source->instrument;
                if ((mask & cpt_masks[action + MASK_VOLUME]) &&
                    (!((mask & cpt_masks[MASK_TRANSP + MASK_VOLUME]) && dest->volume)))
                    dest->volume = source->volume;
                if ((mask & cpt_masks[action + MASK_FXTYPE]) &&
                    (!((mask & cpt_masks[MASK_TRANSP + MASK_FXTYPE]) && dest->fxtype)))
                    dest->fxtype = source->fxtype;
                if ((mask & cpt_masks[action + MASK_FXPARAM]) &&
                    (!((mask & cpt_masks[MASK_TRANSP + MASK_FXPARAM]) && dest->fxparam)))
                    dest->fxparam = source->fxparam;
            }
    } else {
        dst->length = length;
        dstb->num_chans = numch;
        for (j = 0; j < numch && src->channels[j + start_ch]; j++)
            memcpy(dst->channels[j], &src->channels[j + start_ch][start_row],
                length * sizeof(XMNote));
    }
}

static void
_paste_notes(XMPattern* dst,
    const XMPattern* src,
    const gint start_row,
    const gint src_row,
    const gint length,
    const gint start_ch,
    const gint src_ch,
    const gint num_ch)
{
    guint mask = gui_settings.cpt_mask;
    guint i, j;

    for (j = 0; j < num_ch && dst->channels[j + start_ch]; j++)
        if (gui_settings.cpt_mask & MASK_ENABLE)
            for (i = 0; i < length; i++) {
                XMNote* dest = &dst->channels[j + start_ch][i + start_row];
                XMNote* source = &src->channels[j + src_ch][i + src_row];

                if ((mask & cpt_masks[MASK_PASTE + MASK_NOTE]) &&
                    (!((mask & cpt_masks[MASK_TRANSP + MASK_NOTE]) && dest->note)))
                    dest->note = source->note;
                if ((mask & cpt_masks[MASK_PASTE + MASK_INSTR]) &&
                    (!((mask & cpt_masks[MASK_TRANSP + MASK_INSTR]) && dest->instrument)))
                    dest->instrument = source->instrument;
                if ((mask & cpt_masks[MASK_PASTE + MASK_VOLUME]) &&
                    (!((mask & cpt_masks[MASK_TRANSP + MASK_VOLUME]) && dest->volume)))
                    dest->volume = source->volume;
                if ((mask & cpt_masks[MASK_PASTE + MASK_FXTYPE]) &&
                    (!((mask & cpt_masks[MASK_TRANSP + MASK_FXTYPE]) && dest->fxtype)))
                    dest->fxtype = source->fxtype;
                if ((mask & cpt_masks[MASK_PASTE + MASK_FXPARAM]) &&
                    (!((mask & cpt_masks[MASK_TRANSP + MASK_FXPARAM]) && dest->fxparam)))
                    dest->fxparam = source->fxparam;
            }
        else
            memcpy(&dst->channels[start_ch + j][start_row],
                   &src->channels[j + src_ch][src_row], length * sizeof(XMNote));
}

static void
paste_notes(XMPattern* dst,
    const BlockBuffer* srcb,
    const gint start_row,
    const gint start_ch,
    const gint num_ch,
    const gboolean wrap)
{
    const XMPattern* src = &srcb->pattern;
    const gint length = dst->length;
    gint newlength = MIN(src->length, length - start_row);
    gint newnum_ch = MIN(srcb->num_chans, num_ch - start_ch);

    _paste_notes(dst, src, start_row, 0, newlength, start_ch, 0, newnum_ch);
    if (wrap) {
        gint newnum_ch1 = srcb->num_chans - num_ch + start_ch;
        gint newlength1 = src->length - length + start_row;

        if (newnum_ch1 > 0) {
            newnum_ch1 = MIN(newnum_ch1, start_ch);
            if (newnum_ch1)
                _paste_notes(dst, src, start_row, 0, newlength, 0, num_ch - start_ch, newnum_ch1);
        }
        if (newlength1 > 0) {
            newlength1 = MIN(newlength1, start_row);
            if (newlength1) {
                _paste_notes(dst, src, 0, length - start_row, newlength1, start_ch, 0, newnum_ch);
                if (newnum_ch1 > 0)
                    _paste_notes(dst, src, 0, length - start_row, newlength1, 0, num_ch - start_ch, newnum_ch1);
            }
        }
    }
}

void _copy_pattern(GtkWidget* w,
    Tracker* t,
    const gboolean cut)
{
    XMPattern* p = t->curpattern;

    if (!pattern_buffer.pattern.channels[0]) {
        guint i;

        for (i = 0; i < XM_NUM_CHAN; i++)
            pattern_buffer.pattern.channels[i] = calloc(256, sizeof(XMNote));
    }
    /* We copy all 32 channels in spite of actual number of channels */
    copy_notes(&pattern_buffer, p, 0, p->length, 0, 32, cut);
}

void track_editor_copy_pattern(GtkWidget* w, Tracker* t)
{
    _copy_pattern(w, t, FALSE);
}

static void
clear_notes(XMPattern* src,
    const guint start_row,
    const guint length,
    const guint start_ch,
    const guint numch)
{
    guint mask = gui_settings.cpt_mask;
    gint i, j;

    g_assert(start_row + length <= 256 && start_ch + numch <= 32);

    for (i = 0; i < length; i++)
        for (j = 0; j < numch && src->channels[j + start_ch]; j++) {
            XMNote* source = &src->channels[j + start_ch][i + start_row];

            if (mask & cpt_masks[MASK_CUT + MASK_NOTE])
                source->note = 0;
            if (mask & cpt_masks[MASK_CUT + MASK_INSTR])
                source->instrument = 0;
            if (mask & cpt_masks[MASK_CUT + MASK_VOLUME])
                source->volume = 0;
            if (mask & cpt_masks[MASK_CUT + MASK_FXTYPE])
                source->fxtype = 0;
            if (mask & cpt_masks[MASK_CUT + MASK_FXPARAM])
                source->fxparam = 0;
        }
}

void track_editor_cut_pattern(GtkWidget* w, Tracker* t)
{
    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        XMPattern* p = t->curpattern;

        gui_log_pattern(p, N_("Pattern cut"), -1, -1, -1);
        _copy_pattern(w, t, TRUE);
        if (gui_settings.cpt_mask & MASK_ENABLE)
            clear_notes(p, 0, p->length, 0, 32);
        else
            st_clear_pattern(p);
        tracker_redraw(t);
    }
}

void track_editor_paste_pattern(GtkWidget* w, Tracker* t)
{
    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        XMPattern* p = t->curpattern;
        guint len = pattern_buffer.pattern.length;
        gboolean full_update = FALSE;

        if (!len)
            return;
        gui_log_pattern(p, N_("Pattern paste"), -1, -1,
            MAX(p->length, len));

        if (p->length != len) {
            if (p->alloc_length < len)
                st_set_alloc_length(p, len, FALSE);
            p->length = len;
            /* Source and destination pattern sizes don't match.
               The old content is to be cleaned out */
            st_clear_pattern(p);
            full_update = TRUE;
        }

        paste_notes(p, &pattern_buffer, 0, 0, 32, FALSE);

        if (full_update) {
            gui_update_pattern_data();
            tracker_reset(t);
        } else {
            tracker_redraw(t);
        }
    }
}

void track_editor_copy_track(GtkWidget* w, Tracker* t)
{
    copy_notes(&track_buffer, t->curpattern, 0, t->curpattern->length, t->cursor_ch, 1, FALSE);
}

void track_editor_cut_track(GtkWidget* w, Tracker* t)
{
    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        int l = t->curpattern->length;

        track_editor_log_block(t, N_("Track cut"), t->cursor_ch, 0, 1, l);
        copy_notes(&track_buffer, t->curpattern, 0, l, t->cursor_ch, 1, TRUE);
        if (gui_settings.cpt_mask & MASK_ENABLE)
            clear_notes(t->curpattern, 0, l, t->cursor_ch, 1);
        else
            st_clear_track(t->curpattern->channels[t->cursor_ch], l);
        tracker_redraw(t);
    }
}

void track_editor_paste_track(GtkWidget* w, Tracker* t)
{
    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        if (!track_buffer.pattern.length)
            return;
        track_editor_log_block(t, N_("Track paste"), t->cursor_ch, 0, 1, t->curpattern->length);
        paste_notes(t->curpattern, &track_buffer, 0, t->cursor_ch, 32, FALSE);
        tracker_redraw(t);
    }
}

static void
track_editor_copy_cut_selection_common(Tracker* t,
    gboolean cut)
{
    int i;
    int height, width, chStart, rowStart;
    XMPattern* p = t->curpattern;

    if (!tracker_is_valid_selection(t))
        return;

    if (tracker_is_in_selection_mode(t))
        tracker_mark_selection(t, FALSE);

    tracker_get_selection_rect(t, &chStart, &rowStart, &width, &height);

    if (!block_buffer.pattern.channels[0]) {
        guint i;

        for (i = 0; i < XM_NUM_CHAN; i++)
            block_buffer.pattern.channels[i] = calloc(256, sizeof(XMNote));
    }
    copy_notes(&block_buffer, p, rowStart, height, chStart, width, cut);

    if (cut) {
        track_editor_log_block(t, N_("Block cutting"), chStart, rowStart, width, height);
        if (gui_settings.cpt_mask & MASK_ENABLE)
            clear_notes(p, rowStart, height, chStart, width);
        else
            for (i = 0; i < width; i++)
                memset(&p->channels[i + chStart][rowStart], 0, height * sizeof(XMNote));
    }
}

void track_editor_copy_selection(GtkWidget* w, Tracker* t)
{
    track_editor_copy_cut_selection_common(t, FALSE);
    menubar_block_mode_set(FALSE, TRUE);
}

void track_editor_cut_selection(GtkWidget* w, Tracker* t)
{
    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        track_editor_copy_cut_selection_common(t, TRUE);
        menubar_block_mode_set(FALSE, TRUE);
        tracker_redraw(t);
    }
}

void track_editor_paste_selection(GtkWidget* w, Tracker* t, gboolean advance_cursor)
{
    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        gui_log_pattern(t->curpattern, N_("Selection paste"), -1, -1, -1);
        paste_notes(t->curpattern, &block_buffer, t->patpos,
            t->cursor_ch, xm->num_channels, TRUE);

        if (advance_cursor && t->curpattern->length > block_buffer.pattern.length)
            track_editor_set_patpos((t->patpos + block_buffer.pattern.length) % t->curpattern->length);
        tracker_redraw(t);
    }
}

void track_editor_delete_track(GtkWidget* w, Tracker* t)
{
    GtkWidget* focus_widget = GTK_WINDOW(mainwindow)->focus_widget;

    if (GTK_IS_ENTRY(focus_widget)) { /* Emulate Ctrl + BackSpc if the cursor is in an entry */
        g_signal_emit_by_name(focus_widget, "delete-from-cursor",
            GTK_DELETE_WORD_ENDS, -1, NULL);
        return;
    }

    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        gui_log_pattern(t->curpattern, N_("Track removing"), -1, -1, -1);
        st_pattern_delete_track(t->curpattern, t->cursor_ch);
        tracker_redraw(t);
    }
}

void track_editor_insert_track(GtkWidget* w, Tracker* t)
{
    GtkWidget* focus_widget = GTK_WINDOW(mainwindow)->focus_widget;

    if (GTK_IS_ENTRY(focus_widget)) { /* Emulate Ctrl + Ins if the cursor is in an entry */
        g_signal_emit_by_name(focus_widget, "copy-clipboard", NULL);
        return;
    }

    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        gboolean is_ok = TRUE;

        if (!st_is_empty_track(t->curpattern->channels[t->num_channels - 1], t->curpattern->length)) {
            if (t->num_channels < 32)
                is_ok = gui_ok_cancel_modal(mainwindow,
                    _("The last track of the pattern is not empty. It will be shifted beyond the pattern scope. "
                      "You can increase the number of channels to see it. Continue?"));
            else
                is_ok = gui_ok_cancel_modal(mainwindow,
                    _("Warning! The last track of the pattern is not empty. It will be brought to digital nought! "
                      "Do you really want to do this?"));
        }
        if (is_ok) {
            gui_log_pattern(t->curpattern, N_("Track insertion"), -1, -1, -1);
            st_pattern_insert_track(t->curpattern, t->cursor_ch);
            tracker_redraw(t);
        }
    }
}

void track_editor_kill_notes_track(GtkWidget* w, Tracker* t)
{
    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        int i;
        XMNote* note;

        track_editor_log_block(t, N_("Killing notes"),
            t->cursor_ch, t->patpos, 1, t->curpattern->length - t->patpos);

        for (i = t->patpos; i < t->curpattern->length; i++) {
            note = &t->curpattern->channels[t->cursor_ch][i];
            note->note = 0;
            note->instrument = 0;
            note->volume = 0;
            note->fxtype = 0;
            note->fxparam = 0;
        }

        tracker_redraw(t);
    }
}

void track_editor_cmd_mvalue(Tracker* t, gboolean mode)
{
    XMNote* note;
    int nparam, tpos;

    tpos = t->cursor_item;

    if (tpos >= 3) {
        gint from = t->patpos - gui_get_current_jump_value();
        gint jump = gui_get_current_jump_value();

        if (from >= 0) {
            XMNote newnote = t->curpattern->channels[t->cursor_ch][t->patpos];
            note = &t->curpattern->channels[t->cursor_ch][from];

            if (tpos < 5) {
                if (!(nparam = note->volume))
                    return;
                if (nparam < 0x10 || nparam > 0x50) {
                    nparam &= 0xf;
                    mode ? nparam++ : nparam--;

                    newnote.volume &= 0xf0;
                    newnote.volume |= nparam & 0xf;
                } else {
                    if (mode && nparam < 0x50)
                        nparam++;
                    if (!mode && nparam > 0x10)
                        nparam--;

                    newnote.volume = nparam;
                }
            } else {
                if (!(nparam = note->fxparam) && ! note->fxtype)
                    return;
                mode ? nparam++ : nparam--;
                newnote.fxparam = nparam & 0xff;
            }
            track_editor_log_note(t, N_("Vol / FX value increasing / decreasing"), -1,
                t->cursor_ch, t->patpos, HISTORY_FLAG_LOG_PAGE);
            t->curpattern->channels[t->cursor_ch][t->patpos] = newnote;

            if (jump)
                tracker_step_cursor_row(t, jump);
            else
                tracker_redraw_current_row(t);
        }
    }
}

void track_editor_mark_selection(Tracker* t)
{
    tracker_mark_selection(t, TRUE);
}

void track_editor_clear_mark_selection(GtkWidget* w, Tracker* t)
{
    tracker_clear_mark_selection(t);
    menubar_block_mode_set(FALSE, TRUE);
}

void track_editor_paste_selection_no_advance(GtkWidget* w, Tracker* t)
{
    track_editor_paste_selection(w, t, FALSE);
}

static void
track_editor_do_interpolate_fx(GtkWidget* w,
    Tracker* t,
    const gboolean match,
    const gboolean expon)
{
    if (GUI_EDITING && notebook_current_page == NOTEBOOK_PAGE_TRACKER) {
        gint height, width, chStart, rowStart;
        gint xmnote_offset;
        guint8 xmnote_mask, xmnote_mask1 = 0, vol_fx = 0;
        XMNote *note_start, *note_end;
        gint i;
        gint dy, dy1 = 0;
        gint start_value, start_value1 = 0, start_char;
        gfloat k = 1.0, k1 = 1.0;

        if (!tracker_is_valid_selection(t))
            return;

        tracker_get_selection_rect(t, &chStart, &rowStart, &width, &height);
        if (width != 1 || t->cursor_ch != chStart)
            return;

        if (t->cursor_item < 3)
            return;

        note_start = &t->curpattern->channels[t->cursor_ch][rowStart];
        note_end = &t->curpattern->channels[t->cursor_ch][rowStart + height - 1];

        if (t->cursor_item <= 4) {
            // Interpolate volume column
            xmnote_offset = offsetof(XMNote, volume);

            if (note_start->volume >= 0x10 && note_start->volume <= 0x50) {
                if (note_end->volume < 0x10 || note_end->volume > 0x50)
                    return;
                xmnote_mask = 0xff;
            } else {
                vol_fx = note_start->volume & 0xf0;
                if ((note_end->volume & 0xf0) != vol_fx)
                    return;
                xmnote_mask = 0x0f;
            }
        } else {
            // Interpolate effects column
            xmnote_offset = offsetof(XMNote, fxparam);

            if (note_start->fxtype != note_end->fxtype)
                return;

            switch (n_params[note_start->fxtype] & 3) {
            case 0:
                return;
            case 1:
                xmnote_mask = 0xff;
                break;
            case 2:
                xmnote_mask1 = 0xf0;
                xmnote_mask = 0x0f;
                break;
            case 3:
                if ((note_start->fxparam & 0xf0) != (note_end->fxparam & 0xf0))
                    return;
                xmnote_mask = 0x0f;
                break;
            }
        }

        /* Bit-fiddling coming up... */

        start_char = *((guint8*)(note_start) + xmnote_offset);
        start_value = start_char & xmnote_mask;
        dy = *((guint8*)(note_end) + xmnote_offset) & xmnote_mask;
        if (expon) {
            if (!start_value || !dy || start_value == dy)
                return;
            k = logf((float)dy / (float)start_value);
        } else
            dy -= start_value;
        if (xmnote_mask1) {
            dy1 = *((guint8*)(note_end) + xmnote_offset) & xmnote_mask1;
            start_value1 = start_char & xmnote_mask1;
            if (expon) {
                if (!start_value1 || !dy1 || start_value1 == dy1)
                    return;
                k1 = logf((float)dy1 / (float)start_value1);
            } else
                dy1 -= start_value1;
        }

        track_editor_log_block(t, N_("FX interpolating"), t->cursor_ch, rowStart, 1, height);

        for (i = 1; i < height - 1; i++) {
            gint new_value;

            if (t->cursor_item >= 5) {
                if ((!match || (note_start + i)->note) &&
                    !(note_start + i)->fxtype) {
                    /* Copy the effect type into all rows in between */
                    (note_start + i)->fxtype = note_start->fxtype;
                    /* Single 4-bit parameter, most significant nibble should also be added */
                    if (xmnote_mask == 0x0f && !xmnote_mask1)
                        (note_start + i)->fxparam = note_start->fxparam & 0xf0;
                }

                /* On effect interpolation, skip lines that already contain different effects */
                if ((note_start + i)->fxtype != note_start->fxtype)
                    continue;

                /* A special case of the '0' effect in the match mode */
                if (match && !note_start->fxtype && !(note_start + i)->note)
                    continue;

                /* Effect with single 4-bit parameter. The most significant parameter nibble acts
                   like subeffect and must be taken into account */
                if (xmnote_mask == 0x0f && !xmnote_mask1 &&
                    ((note_start + i)->fxparam & 0xf0) != (note_start->fxparam & 0xf0))
                    continue;

            } else {
                const guint8 volume = (note_start + i)->volume;

                if (vol_fx) {
                    /* On vol. effect interpolation, skip lines that already contain different effects */
                    if (volume && 0xf0 != vol_fx)
                        continue;
                } else {
                    /* Volume interpolation, skip lines with volume effect */
                    if (volume > 0x50)
                        continue;
                }

                /* Skip empty and key-off lines in the match mode */
                if (match && (!(note_start + i)->note ||
                               (note_start + i)->note == XM_PATTERN_NOTE_OFF))
                    continue;
            }

            if (expon)
                new_value = lrint((gfloat)start_value * exp (k * (gfloat)i / (gfloat)(height - 1)));
            else
                new_value = start_value + (gint)((gfloat)i * dy / (height - 1) + (dy >= 0 ? 1.0 : -1.0) * 0.5);
            new_value &= xmnote_mask;
            new_value |= (start_char & ~xmnote_mask);
            if (xmnote_mask1) {
                gint new_value1;

                if (expon)
                    new_value1 = lrint((gfloat)start_value1 * exp (k1 * (gfloat)i / (gfloat)(height - 1)));
                else
                    new_value1 = start_value1 + (gint)((gfloat)i * dy1 / (height - 1) + (dy1 >= 0 ? 1.0 : -1.0) * 0.5);
                new_value1 &= xmnote_mask1;
                new_value = (new_value & ~xmnote_mask1) | new_value1;
            }

            *((guint8*)(note_start + i) + xmnote_offset) = new_value;
        }

        tracker_redraw(t);
    }
}

void track_editor_interpolate_fx(GtkWidget* w, Tracker* t)
{
    track_editor_do_interpolate_fx(w, t, FALSE, FALSE);
}

void track_editor_interpolate_efx(GtkWidget* w, Tracker* t)
{
    track_editor_do_interpolate_fx(w, t, TRUE, FALSE);
}

void track_editor_interpolate_fx_exp(GtkWidget* w, Tracker* t)
{
    track_editor_do_interpolate_fx(w, t, FALSE, TRUE);
}

void track_editor_interpolate_efx_exp(GtkWidget* w, Tracker* t)
{
    track_editor_do_interpolate_fx(w, t, TRUE, TRUE);
}

static gint
char2hex(gint n, const gchar maxl, const gchar maxu)
{
    if (n >= '0' && n <= '9')
        n -= '0';
    else {
        if (n >= 'a' && n <= maxl)
            n = n - 'a' + 10;
        else if (n >= 'A' && n <= maxu)
            n = n - 'A' + 10;
        else
            return -1;
    }

    return n;
}

static gboolean
track_editor_handle_semidec_column_input(Tracker* t,
    int expr,
    guint8* modpt,
    int n)
{
    switch (expr) {
    case 0:
        if (n < '0' || n > '9')
            return FALSE;
        *modpt = (*modpt / 10) * 10 + n - '0';
        break;
    case 1:
        /* 128 instruments maximum, so only 'a', 'b' and 'c' are acceptable */
        n = char2hex(n, 'c', 'C');
        if (n == -1)
            return FALSE;
        *modpt = (*modpt % 10) + 10 * n;
        break;
    }
    if (*modpt > 128)
        *modpt = 128;

    return TRUE;
}

static void
track_editor_offset_cursor(Tracker* t, gint expr)
{
    gint step_row = 0;

    if (!gui_settings.advance_cursor_in_fx_columns) {
        step_row = gui_get_current_jump_value();
    } else {
        if (expr) { /* not at the end */
            tracker_step_cursor_item(t, 1);
        } else {
            step_row = gui_get_current_jump_value();
            /* -2 in case of fx col */
            tracker_step_cursor_item(t, t->cursor_item == 7 ? -2 : -1);
        }
    }
    if (step_row)
        tracker_step_cursor_row(t, step_row);
    else
        tracker_redraw_current_row(t);
}

static gboolean
track_editor_handle_hex_column_input(Tracker* t,
    gint expr,
    guint8* modpt,
    gint n)
{
    gint s, rest;

    if (gui_settings.tracker_ft2_volume) {
        if (gui_settings.tracker_vol_dec) {
            switch (t->cursor_item) {
            case 3:
                if (n < '0' || n > '6')
                    /* This is not volume */
                    break;
            /* Then falling through */
            case 4:
                s = *modpt;
                if (s > 0x40 && t->cursor_item == 4)
                    /* The first byte refers to an FX rather than volume */
                    break;
                if (n < '0' || n > '9')
                    return FALSE;

                n -= '0';
                /* Exp can be only 0 or 1 here */
                if (expr && s > 0x40) {
                    /* FX is converted to volume, we keep the parameter as is,
                       possibly limiting it to the value of 9 */
                    rest = s & 0xf;
                    rest = rest > 9 ? 9 : rest;
                } else
                    rest = s % 10;
                s = expr ? rest + n * 10 : s - rest + n;
                if (s > 64)
                    s = 64;
                *modpt = s;

                return TRUE;
            default:
                break;
            }
        }
        /* Convert '-' symbol to '6' FX */
        if (n == '-' && t->cursor_item == 3)
            n = '6';
    }

    n = char2hex(n, 'f', 'F');
    if (n == -1)
        return FALSE;

    s = *modpt;
    if (gui_settings.tracker_ft2_volume &&
        gui_settings.tracker_vol_dec &&
        t->cursor_item == 3 && s <= 0x40) {
            /* Trasfer volume -> FX, preserving the lower digit */
            rest = s % 10;
            s = n << 4 | rest;
    } else {
        expr *= 4;
        s &= 0xf0 >> expr;
        s |= n << expr;
    }
    *modpt = s;

    return TRUE;
}

static gboolean
track_editor_handle_column_input(Tracker* t,
    int gdkkey)
{
    static XMPattern* fxcp = NULL;
    static gint fxch = -1, fxpatpos = -1;
    gboolean handled = FALSE;
    gint n;
    guint8 tmp_value;
    guchar volume;
    XMNote* note = &t->curpattern->channels[t->cursor_ch][t->patpos];
    const gboolean ft2_volume = gui_settings.tracker_ft2_volume;

    if (t->cursor_item == 5) {
        gint jump = 0;

        /* Effect column (not the parameter) */
        switch (gdkkey) {
        case '0' ... '9':
            n = gdkkey - '0';
            break;
        case 'a' ... 'z':
        case 'A' ... 'Z':
            gdkkey = tolower(gdkkey);
            n = gdkkey - 'a' + 10;
            break;
        default:
            return FALSE;
        }
        if (fxcp != t->curpattern || fxch != t->cursor_ch || fxpatpos != t->patpos) {
            fxcp = t->curpattern;
            fxch = t->cursor_ch;
            fxpatpos = t->patpos;

            track_editor_log_note(t, N_("Effect changing"),
                -1, t->cursor_ch, t->patpos, HISTORY_FLAG_LOG_PAGE);
        }
        note->fxtype = n;
        if (!gui_settings.advance_cursor_in_fx_columns)
            jump = gui_get_current_jump_value();
        else
            tracker_step_cursor_item(t, 1);

        if (jump)
            tracker_step_cursor_row(t, jump);
        else
            tracker_redraw_current_row(t);

        return TRUE;
    }

    switch (t->cursor_item) {
    case 1:
    case 2: /* instrument column */
        tmp_value = note->instrument;
        if (track_editor_handle_semidec_column_input(t, 2 - t->cursor_item, &tmp_value, gdkkey)) {
            static XMPattern* cp = NULL;
            static gint ch = -1, patpos = -1;

            if (cp != t->curpattern || ch != t->cursor_ch || patpos != t->patpos) {
                cp = t->curpattern;
                ch = t->cursor_ch;
                patpos = t->patpos;

                track_editor_log_note(t, N_("Instrument changing"),
                    -1, t->cursor_ch, t->patpos, HISTORY_FLAG_LOG_PAGE);
            }
            note->instrument = tmp_value;
            track_editor_offset_cursor(t, 2 - t->cursor_item);
            handled = TRUE;
        }
        break;
    case 3:
        if (ft2_volume) {
            /* Convert some keybingings to emulate FT2 */
            switch (gdkkey) {
            /* '-' -> '6' is a special case, the conversion is performed
               in track_editor_handle_hex_column_input() */
            case '+':
                gdkkey = '7';
                break;
            case 'd':
            case 'D':
                gdkkey = '8';
                break;
            case 'u':
            case 'U':
                gdkkey = '9';
                break;
            case 's':
            case 'S':
                gdkkey = 'a';
                break;
            case 'v':
            case 'V':
                gdkkey = 'b';
                break;
            case 'p':
            case 'P':
                gdkkey = 'c';
                break;
            case 'l':
            case 'L':
            case '<':
                gdkkey = 'd';
                break;
            case 'r':
            case 'R':
            case '>':
                gdkkey = 'e';
                break;
            case 'm':
            case 'M':
                gdkkey = 'f';
                break;
            default:
                break;
            }
        } /* Then falling through */
    case 4: /* volume column */
        volume = note->volume;
        /* The volume value is shifted by 0x10, so if we want to see the real value (FT2 mode),
           but store it correctly, we should subtract 0x10 before the modification, and add
           this value afterwards */
        if (ft2_volume && volume < 0x60 && volume >= 0x10)
            volume -= 0x10;
        if (track_editor_handle_hex_column_input(t, 4 - t->cursor_item, &volume, gdkkey)) {
            if (ft2_volume && volume < 0x50)
                volume += 0x10;
            /* Avoiding unused volume ranges */
            if (volume > 0x50 && volume < 0x60)
                volume = 0x50;
            if (volume >= 0x10) {
                static XMPattern* cp = NULL;
                static gint ch = -1, patpos = -1;

                if (cp != t->curpattern || ch != t->cursor_ch || patpos != t->patpos) {
                    cp = t->curpattern;
                    ch = t->cursor_ch;
                    patpos = t->patpos;

                    track_editor_log_note(t, N_("Volume / Vol. FX changing"),
                        -1, t->cursor_ch, t->patpos, HISTORY_FLAG_LOG_PAGE);
                }
                note->volume = volume;
                track_editor_offset_cursor(t, 4 - t->cursor_item);
            }
            handled = TRUE;
        }
        break;
    case 6:
    case 7: /* effect parameter */
        tmp_value = note->fxparam;
        if (track_editor_handle_hex_column_input(t, 7 - t->cursor_item, &tmp_value, gdkkey)) {
            if (fxcp != t->curpattern || fxch != t->cursor_ch || fxpatpos != t->patpos) {
                fxcp = t->curpattern;
                fxch = t->cursor_ch;
                fxpatpos = t->patpos;

                track_editor_log_note(t, N_("Effect changing"),
                    -1, t->cursor_ch, t->patpos, HISTORY_FLAG_LOG_PAGE);
            }
            note->fxparam = tmp_value;
            track_editor_offset_cursor(t, 7 - t->cursor_item);
            handled = TRUE;
        }
        break;
    default:
        break;
    }

    return handled;
}

static void
tracker_timeout_foreach(gpointer data,
    gpointer user_data)
{
    /* Walking through all events from player because one of them
       can be the stop command */
    if (data) {
        audio_player_pos* p = data;

        if (p->command == AUDIO_COMMAND_STOP_PLAYING)
            gui_play_stop();
        else {
            cur_tick = p->curtick;
            curtime = p->time;
            cur_pos = p->patpos;
            cur_pat = p->songpos;
            cur_tempo = p->tempo;
            prev_tempo = p->prev_tempo;
            prevtime = p->prev_tick_time;
            nexttime = p->prev_tick_time;
            patno = p->patno;
            bpm = p->bpm;
            *((gboolean *)user_data) = TRUE;
        }
    }
}

static gint
tracker_timeout(gpointer data)
{
    g_debug("tracker_timeout()");

    double display_songtime;
    gboolean new_event = TRUE;

    if (current_driver_object == NULL) {
        /* Can happen when audio thread stops on its own. Note that
	 * tracker_stop_updating() is called in
	 * gui.c::read_mixer_pipe(). */
        return TRUE;
    }

    display_songtime = current_driver->get_play_time(current_driver_object);
    g_debug("tracker_timeout() songtime=%lf", display_songtime);

    if (display_songtime < 0.0) {
        gui_clipping_indicator_update(FALSE);
    } else {
        audio_clipping_indicator* c =
            time_buffer_get(audio_clipping_indicator_tb, display_songtime);
        if (c) {
            gui_clipping_indicator_update(c->clipping);
            g_free(c);
        }
    }

    time_buffer_foreach(audio_playerpos_tb, display_songtime, tracker_timeout_foreach, &new_event);
    if (new_event)
        gui_update_player_pos(curtime, cur_pat, patno, cur_pos, cur_tempo, bpm);

    return TRUE;
}

void tracker_start_updating(void)
{
    g_debug("tracker_start_updating()");

    if (gtktimer != -1)
        return;

    gtktimer = g_timeout_add(1000 / update_freq, tracker_timeout, NULL);
}

void tracker_stop_updating(void)
{
    g_debug("tracker_stop_updating()");

    if (gtktimer == -1)
        return;

    g_source_remove(gtktimer);
    gtktimer = -1;
}

void tracker_set_update_freq(int freq)
{
    update_freq = freq;
    if (gtktimer != -1) {
        tracker_stop_updating();
        tracker_start_updating();
    }
}

void track_editor_load_config(void)
{
    gboolean* buf;
    int i;

    buf = prefs_get_bool_array("settings", "jazz-toggle", NULL);
    if (buf) {
        for (i = 0; i < 32; i++) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(jazztoggles[i]), buf[i]);
        }

        g_free(buf);
    }
}

void track_editor_save_config(void)
{
    gboolean buf[32];
    guint i;

    if (gui_settings.save_settings_on_exit) {
        trackersettings_write_settings();
    }

    for (i = 0; i < 32; i++)
        buf[i] = GTK_TOGGLE_BUTTON(jazztoggles[i])->active;
    prefs_put_bool_array("settings", "jazz-toggle", buf, 32);
}

void track_editor_toggle_permanentness(Tracker* t, gboolean all)
{
    int i = t->cursor_ch;

    if (!all)
        gui_settings.permanent_channels ^= 1 << i;
    else
        gui_settings.permanent_channels = gui_settings.permanent_channels ? 0 : 0xFFFFFFFF;

    tracker_redraw(t);
}

static void show_buffers_response(GtkWidget* dialog,
    gint resp,
    GtkNotebook* nbook)
{
    if (resp == 1) {
        gint i = gtk_notebook_get_current_page(nbook);
        Tracker* t = ccp_buffers[i].tracker;

        tracker_set_pattern(t, NULL);
        ccp_buffers[i].pattern.length = 0;
        tracker_set_pattern(t, &ccp_buffers[i].pattern);
        tracker_set_num_channels(t, 0);
    } else
        gtk_widget_hide(dialog);
}

void track_editor_show_buffers(void)
{
    static GtkWidget* dialog = NULL;
    gint i;
    Tracker* t;

    if (!dialog) {
        GtkWidget *vbox, *nbook;
        gint hght, wdth;
        GdkScreen* scr;

        dialog = gtk_dialog_new_with_buttons(_("Buffers"), GTK_WINDOW(mainwindow),
            GTK_DIALOG_MODAL, GTK_STOCK_CLEAR, 1, GTK_STOCK_CLOSE, 0, NULL);
        gtk_container_set_border_width(GTK_CONTAINER(dialog), 4);
        gtk_widget_set_tooltip_text(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog),
            1), _("Clear current buffer"));

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        gtk_box_set_spacing(GTK_BOX(vbox), 4);

        nbook = gtk_notebook_new();
        gtk_box_pack_start(GTK_BOX(vbox), nbook, TRUE, TRUE, 0);
        gui_dialog_connect_data(dialog, G_CALLBACK(show_buffers_response), nbook);
        for (i = 0; i < 3; i++) {
            GtkWidget *table, *hbar, *vbar, *thing;
            GtkAdjustment *a;

            table = gtk_table_new(2, 2, FALSE);
            gtk_notebook_append_page(GTK_NOTEBOOK(nbook), table, gtk_label_new(_(ccp_buffers[i].name)));

            thing = tracker_new();
            t = ccp_buffers[i].tracker = TRACKER(thing);
            tracker_set_print_numbers(t, FALSE);
            tracker_set_print_cursor(t, FALSE);
            tracker_set_selectable(t, FALSE);
            tracker_set_pattern(t, &ccp_buffers[i].pattern);
            gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 0, 1);

            hbar = gtk_hscrollbar_new(NULL);
            a = gtk_range_get_adjustment(GTK_RANGE(hbar));
            gtk_table_attach(GTK_TABLE(table), hbar, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
            g_signal_connect(thing, "xpanning", G_CALLBACK(set_hscrollbar2), a);
            g_signal_connect(thing, "xconf", G_CALLBACK(update_hscrollbar2), a);
            g_signal_connect(hbar, "value-changed", G_CALLBACK(hscrollbar_changed), t);

            vbar = gtk_vscrollbar_new(NULL);
            a = gtk_range_get_adjustment(GTK_RANGE(vbar));
            gtk_table_attach(GTK_TABLE(table), vbar, 1, 2, 0, 1, 0, GTK_FILL, 0, 0);
            g_signal_connect(thing, "patpos", G_CALLBACK(set_vscrollbar2), a);
            g_signal_connect(thing, "yconf", G_CALLBACK(update_vscrollbar2), a);
            g_signal_connect(vbar, "value-changed", G_CALLBACK(vscrollbar_changed), t);
        }
        gtk_widget_show_all(vbox);

        scr = gdk_screen_get_default();
        hght = gdk_screen_get_height(scr);
        wdth = gdk_screen_get_width(scr);
        gtk_widget_set_size_request(dialog, wdth >> 1, hght >> 1);
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ALWAYS);
    }
    /* A kind of crutch; we update tracker font each time the window is showing up.
       Instead settings item should be a GObject and inform subscribers on its change */
    for (i = 0; i < 3; i++) {
        t = ccp_buffers[i].tracker;
        tracker_set_font(t, gui_settings.tracker_font);
        tracker_set_backing_store(t, gui_settings.gui_use_backing_store);
        tracker_set_num_channels(t, ccp_buffers[i].num_chans);
    }
    gtk_window_present(GTK_WINDOW(dialog));
}
