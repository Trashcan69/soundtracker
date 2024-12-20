
/*
 * The Real SoundTracker - GUI configuration dialog (header)
 *
 * Copyright (C) 1999-2019 Michael Krause
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

#ifndef _ST_GUI_SETTINGS_H
#define _ST_GUI_SETTINGS_H

#include <gtk/gtk.h>
#include "sample-display.h"
#include "track-editor.h"

typedef struct {
    gchar** lines;
    gsize num, num_allocated;
} StrArray;

typedef struct gui_prefs {
    gchar* tracker_line_format;
    gchar* tracker_font;
    gboolean tracker_hexmode;
    gboolean tracker_upcase;
    gboolean tracker_ft2_volume;
    gboolean tracker_ft2_wide;
    gboolean tracker_ft2_tpm;
    gboolean tracker_vol_dec;
    gboolean highlight_rows;
    int highlight_rows_n;
    int highlight_rows_minor_n;
    gboolean clavier_colors_gtk;
    const gchar* clavier_font;

    gboolean advance_cursor_in_fx_columns;
    gboolean asynchronous_editing;
    gboolean change_patlen;
    gboolean try_polyphony;
    gboolean repeat_same_note;
    gboolean insert_noteoff;
    gboolean precise_timing;
    gboolean looped;
    gboolean ctrl_show_isel;

    gboolean tempo_bpm_update;
    gboolean rxx_bug_emu;
    gboolean use_filter;
    gboolean auto_switch;
    gboolean add_extension;
    gboolean check_xm_compat;
    gboolean ins_name_overwrite, smp_name_overwrite;

    gboolean gui_display_scopes;
    gboolean gui_disable_splash;
    gboolean gui_use_backing_store;

    gboolean save_geometry;
    gboolean save_settings_on_exit;

    gboolean sharp;
    gboolean bh;
    guint cpt_mask;

    int tracker_update_freq;
    int scopes_update_freq;
    int scopes_buffer_size;
    gboolean show_ins_smp;
    gdouble delay_comp;

    int st_window_x;
    int st_window_y;
    int st_window_w;
    int st_window_h;

    gboolean store_perm;
    guint32 permanent_channels;
    gint undo_size;

    SampleDisplayMode scopes_mode, editor_mode, sampling_mode;
    SelectionMode selection_mode;
    gboolean sample_play_loop;

    gchar* loadmod_path;
    gchar* savemod_path;
    gchar* savemodaswav_path;
    gchar* savesongasxm_path;
    gchar* loadsmpl_path;
    gchar* savesmpl_path;
    gchar* loadinstr_path;
    gchar* saveinstr_path;
    gchar* loadpat_path;
    gchar* savepat_path;

    gchar* rm_path;
    gchar* unzip_path;
    gchar* lha_path;
    gchar* gz_path;
    gchar* bz2_path;

    StrArray se_extension_path;

    guint file_out_channels;
    guint file_out_mixfreq;
    guint file_out_resolution;
} gui_prefs;

extern gui_prefs gui_settings;

void gui_settings_dialog(void);

void gui_settings_load_config(void);
void gui_settings_save_config(void);
void gui_settings_save_config_always(void);

void gui_settings_highlight_rows_changed(GtkSpinButton* spin);
void gui_settings_highlight_rows_minor_changed(GtkSpinButton* spin);

void gui_settings_make_path(const gchar* fn,
    gchar** store);
#define ASYNCEDIT (gui_settings.asynchronous_editing)

#endif /* _ST_GUI_SETTINGS_H */
