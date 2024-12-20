
/*
 * The Real SoundTracker - main user interface handling (header)
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

#ifndef _GUI_H
#define _GUI_H

#include <config.h>

#include "gui-subs.h"
#include "xm.h"

#define UI_FILE DATADIR "/" PACKAGE "/" PACKAGE ".ui"

/* values for gui_playing_mode */
enum {
    PLAYING_SONG = 1,
    PLAYING_PATTERN,
    PLAYING_NOTE, /* is overridden by PLAYING_SONG / PLAYING_PATTERN */
};

extern int gui_playing_mode; /* one of the above or 0 */

/* Notebook page numbers.  Should be kept in sync
   when pages are added to notebook [See gui_init() in app/gui.c]. */

enum {
    NOTEBOOK_PAGE_FILE = 0,
    NOTEBOOK_PAGE_TRACKER,
    NOTEBOOK_PAGE_INSTRUMENT_EDITOR,
    NOTEBOOK_PAGE_SAMPLE_EDITOR,
    NOTEBOOK_PAGE_MODULE_INFO
};

typedef enum {
    GUI_PIXMAP_PLAY = 0,
    GUI_PIXMAP_PLAY_CUR,
    GUI_PIXMAP_STOP,
    GUI_PIXMAP_LOCK,
    GUI_PIXMAP_PLAY_FROM,
    GUI_PIXMAP_PLAY_BLOCK,
    GUI_PIXMAP_LOOP,
    GUI_PIXMAP_MASK,
    GUI_PIXMAP_ENV_SCALE,
    GUI_PIXMAP_ENV_INVERSE,
    GUI_PIXMAP_LOOP_FACTORY_RESET,
    GUI_PIXMAP_LAST
} GuiPixmap;

extern int notebook_current_page; /* one of the above */

#define GUI_ENABLED (gui_playing_mode != PLAYING_SONG && gui_playing_mode != PLAYING_PATTERN)
#define GUI_EDITING (GTK_TOGGLE_BUTTON(editing_toggle)->active)

extern GtkWidget* editing_toggle;
extern GtkWidget *gui_curins_name, *gui_cursmpl_name;
extern GtkWidget* mainwindow;

extern void show_editmode_status(void);

int gui_splash(void);
int gui_final(int argc, char* argv[]);

void gui_playlist_initialize(void);

void gui_play_note(int channel,
    int note,
    gboolean all);
void gui_play_note_full(const guint channel,
    const guint note,
    STSample* sample,
    const guint32 offset,
    const guint32 playend,
    const gboolean use_all_channels,
    const gint ins,
    const gint smp);
gboolean gui_play_note_no_repeat(guint32 keyval,
    gint modifiers,
    gboolean pressed,
    STSample* sample,
    gint start, gint playend,
    gint channel,
    gboolean full,
    gboolean* is_note,
    gboolean use_all_channels,
    const gint ins,
    const gint smp);
void gui_play_note_keyoff(int channel);
void gui_stop_note(int channel);

void gui_play_stop(void);
void gui_play_block(void);
void gui_play_current_pattern_row(void);

void gui_go_to_page(gint page);
static inline void gui_go_to_fileops_page(void)
{
    gui_go_to_page(NOTEBOOK_PAGE_FILE);
}

void gui_block_smplname_entry(gboolean block);
void gui_block_instrname_entry(gboolean block);

void gui_set_current_instrument(int);
void gui_set_current_sample(int);
void gui_set_current_pattern(int, gboolean);
void gui_set_current_position(int n);
void gui_update_pattern_data(void);

int gui_get_current_instrument(void);
int gui_get_current_sample(void);
int gui_get_current_pattern(void);
int gui_get_current_position(void);

int gui_get_current_jump_value(void);
int gui_get_current_octave_value(void);
void gui_set_jump_value(int value);

void gui_update_player_pos(const gdouble time,
    const gint songpos,
    const gint patno,
    const gint patpos,
    const gint tempo,
    const gint bpm);
void gui_clipping_indicator_update(const gboolean status);
void gui_set_channel_status_update_freq(const gint freq);

void gui_init_xm(int new_xm, gboolean updatechspin);
void gui_free_xm(void);
void gui_new_xm(void);

void gui_direction_clicked(GtkWidget* widget,
    gpointer data);
void gui_accidentals_clicked(GtkWidget* widget,
    gpointer data);

void gui_shrink_pattern(void);
void gui_expand_pattern(void);

void gui_save_current(void);

void gui_update_title(const gchar* filename);
void gui_reset_title(void);
void gui_offset_current_instrument(int offset);
void gui_offset_current_sample(int offset);

void gui_statusbar_update(int message,
    gboolean force_gui_update);
void gui_statusbar_update_message(gchar* message,
    gboolean force_update);
void gui_statusbar_update_message_high(gchar* message);
void gui_statusbar_restore_base_message(void);

static inline void
gui_unset_focus(void)
{
    gtk_window_set_focus(GTK_WINDOW(mainwindow), NULL);
}

GtkWidget* gui_get_pixmap(GuiPixmap);

void gui_log_pattern_full(XMPattern* pattern, const gchar* title, const gint pat,
    gint len, gint alloc_len, gint patpos);

static inline void gui_log_pattern(XMPattern* pattern, const gchar* title, const gint pat,
    gint len, gint alloc_len)
{
    gui_log_pattern_full(pattern, title, pat, len, alloc_len, -1);
}

void gui_log_song(const gchar* title, XM* mod);

gboolean
gui_handle_standard_keys(int shift,
    int ctrl,
    int alt,
    guint32 keyval,
    gint hwcode);

#endif /* _GUI_H */
