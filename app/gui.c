
/*
 * The Real SoundTracker - main user interface handling
 *
 * Copyright (C) 1998-2019 Michael Krause
 * Copyright (C) 2020-2024 Yury Aliaev
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

#if USE_SNDFILE
#include <sndfile.h>
#elif AUDIOFILE_VERSION
#include <audiofile.h>
#endif

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "poll.h"

#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "audio.h"
#include "audio-subs.h"
#include "clock.h"
#include "colors.h"
#include "extspinbutton.h"
#include "file-operations.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "gui.h"
#include "history.h"
#include "instrument-editor.h"
#include "keys.h"
#include "main.h"
#include "menubar.h"
#include "module-info.h"
#include "playlist.h"
#include "preferences.h"
#include "sample-editor.h"
#include "scope-group.h"
#include "st-subs.h"
#include "time-buffer.h"
#include "tips-dialog.h"
#include "track-editor.h"
#include "tracker.h"
#include "xm-player.h"

int gui_playing_mode = 0;
int notebook_current_page = NOTEBOOK_PAGE_FILE;
GtkWidget* editing_toggle;
GtkWidget *gui_curins_name, *gui_cursmpl_name;
GtkWidget* mainwindow = NULL;
GtkWidget *alt[2], *arrow[2];
ScopeGroup* scopegroup;
GtkBuilder* gui_builder;

static GtkWidget* gui_splash_window = NULL;
static cairo_surface_t* gui_splash_logo = NULL;
static GtkWidget* gui_splash_logo_area;
static GtkWidget* gui_splash_label;
static GtkWidget* gui_splash_close_button;

static gint snch_id, inch_id, tempo_spin_id, bpm_spin_id, db_id;
static gint chan_status_timer = -1;
static gint curinst = 0;
static GIOChannel *audio_backpipe_channel;
static gchar* current_filename = NULL;
static gboolean looping_cross = TRUE, stop_process;
static GtkWidget *mainwindow_upper_hbox, *mainwindow_second_hbox;
static GtkWidget* notebook;
static GtkWidget *spin_editpat, *spin_patlen, *spin_numchans;
static GtkWidget* cursmpl_spin;
static GtkAdjustment *adj_amplification, *adj_pitchbend;
static GtkWidget *spin_jump, *curins_spin, *spin_octave, *bpm_spin, *tempo_spin, *spin_songpos, *spin_pat;
static GtkWidget *label_jump, *label_octave, *label_songpos, *label_pat;
static GtkWidget *toggle_lock_editpat, *looping_toggle, *mask_toggle;
static GtkWidget* status_bar;
static GtkWidget* st_clock;
static GtkRecentManager* rcmgr;
static Playlist* playlist;
static file_op *loadmod, *savemod, *savexm, *loadpat, *savepat;
static gint notebook_prev_page = NOTEBOOK_PAGE_FILE;
#if USE_SNDFILE || AUDIOFILE_VERSION
static file_op* renderwav;
#endif
extern file_op *saveinstr, *loadinstr;

struct measure {
    const gchar* title;
    gint major;
    gint minor;
};

static struct measure measure_msr[] = {
    { "2/2", 16, 8 },
    { "3/2", 24, 8 },
    { "4/2", 32, 8 },
    { "2/4", 8, 4 },
    { "3/4", 12, 4 },
    { "4/4", 16, 4 },
    { "5/4", 20, 4 },
    { "6/4", 24, 4 },
    { "7/4", 28, 4 },
    { "3/8", 6, 6 },
    { "4/8", 8, 8 },
    { "5/8", 10, 10 },
    { "6/8", 12, 6 },
    { "9/8", 18, 6 },
    { "12/8", 24, 6 },
    { NULL }
};

struct {
    gboolean modified;
    guint handler;
} tidata = { 0 };

static const gchar* pixmaps[] = {
    DATADIR "/" PACKAGE "/play.xpm",
    DATADIR "/" PACKAGE "/play_cur.xpm",
    DATADIR "/" PACKAGE "/stop.xpm",
    DATADIR "/" PACKAGE "/lock.xpm",
    DATADIR "/" PACKAGE "/play_from.png",
    DATADIR "/" PACKAGE "/play_block.xpm",
    DATADIR "/" PACKAGE "/loop.png",
    DATADIR "/" PACKAGE "/mask.png",
    DATADIR "/" PACKAGE "/env_scale.png",
    DATADIR "/" PACKAGE "/env_inverse.png",
    DATADIR "/" PACKAGE "/loop-factory-reset.png"
};

static const char* status_messages[] = {
    N_("Ready."),
    N_("Playing song..."),
    N_("Playing pattern..."),
    N_("Loading module..."),
    N_("Module loaded."),
    N_("Saving module..."),
    N_("Module saved."),
    N_("Loading sample..."),
    N_("Sample loaded."),
    N_("Saving sample..."),
    N_("Sample saved."),
    N_("Loading instrument..."),
    N_("Instrument loaded."),
    N_("Saving instrument..."),
    N_("Instrument saved."),
    N_("Saving song..."),
    N_("Song saved."),
};

static gchar *base_message = NULL;

static GtkWidget* measurewindow = NULL;

static GtkWidget* gui_clipping_led;
static GdkPixbuf *led_normal, *led_clipping;

static int editing_pat = 0;
static int gui_ewc_startstop = 0;

/* gui event handlers */
static void current_instrument_changed(GtkSpinButton* spin);
static void current_instrument_name_changed(void);
static void current_sample_changed(GtkSpinButton* spin);
static void current_sample_name_changed(void);
static int keyevent(GtkWidget* widget, GdkEventKey* event, gpointer data);
static void gui_editpat_changed(GtkSpinButton* spin);
static void gui_patlen_changed(GtkSpinButton* spin);
static void gui_numchans_changed(GtkSpinButton* spin);
static void notebook_page_switched(GtkNotebook* notebook, gpointer page, int page_num);
static void gui_adj_amplification_changed(GtkAdjustment* adj, GtkSpinButton* spin);
static void gui_adj_pitchbend_changed(GtkAdjustment* adj);

/* mixer / player communication */
static void wait_for_player(void);
static void play_pattern(void);
static void play_song(gpointer start_from_current_row);

/* gui initialization / helpers */
static void gui_enable(int enable);
static void offset_current_pattern(int offset);
void gui_offset_current_instrument(int offset);
void gui_offset_current_sample(int offset);

static void gui_auto_switch_page(void);
static gboolean gui_load_xm(const char* filename, const char* localname);

static void
editing_toggled(GtkToggleButton* button, gpointer data)
{
    gboolean is_active = button->active;
    tracker_set_editing(tracker, is_active);
    if (is_active)
        show_editmode_status();
    else
        gui_statusbar_update(STATUS_IDLE, FALSE);
}

static void
gui_highlight_rows_toggled(GtkWidget* widget)
{
    gui_settings.highlight_rows = GTK_TOGGLE_BUTTON(widget)->active;

    tracker_redraw(tracker);
}

void gui_accidentals_clicked(GtkWidget* widget, gpointer data)
{
    GtkWidget* focus_widget = GTK_WINDOW(mainwindow)->focus_widget;

    if (GTK_IS_ENTRY(focus_widget)) { /* Emulate Ctrl + A if the cursor is in an entry */
        g_signal_emit_by_name(focus_widget, "move-cursor", GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1, FALSE, NULL);
        g_signal_emit_by_name(focus_widget, "move-cursor", GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1, TRUE, NULL);
        return;
    }
    gui_settings.sharp = !gui_settings.sharp;
    gtk_widget_hide(alt[gui_settings.sharp ? 1 : 0]);
    gtk_widget_show(alt[gui_settings.sharp ? 0 : 1]);
    tracker_redraw(tracker);
}

void gui_direction_clicked(GtkWidget* widget, gpointer data)
{
    gui_settings.advance_cursor_in_fx_columns = !gui_settings.advance_cursor_in_fx_columns;
    gtk_widget_hide(arrow[gui_settings.advance_cursor_in_fx_columns ? 0 : 1]);
    gtk_widget_show(arrow[gui_settings.advance_cursor_in_fx_columns ? 1 : 0]);
}

static gboolean
measure_close_requested(void)
{
    gtk_widget_hide(measurewindow);
    /* to make keyboard working immediately after closing the dialog */
    gui_unset_focus();
    return TRUE;
}

static void
measure_dialog()
{
    GtkObject* adj;
    GtkWidget *mainbox, *thing, *vbox;
    static GtkWidget* majspin;

    if (measurewindow != NULL) {
        gtk_window_set_position(GTK_WINDOW(measurewindow), GTK_WIN_POS_MOUSE);
        gtk_window_present(GTK_WINDOW(measurewindow));
        gtk_widget_grab_focus(majspin);
        return;
    }

    measurewindow = gtk_dialog_new_with_buttons(_("Row highlighting configuration"), GTK_WINDOW(mainwindow),
        GTK_DIALOG_MODAL, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
    gui_dialog_adjust(measurewindow, GTK_RESPONSE_CLOSE);
    gui_dialog_connect(measurewindow, G_CALLBACK(measure_close_requested));
    gtk_window_set_position(GTK_WINDOW(measurewindow), GTK_WIN_POS_MOUSE);

    vbox = gtk_dialog_get_content_area(GTK_DIALOG(measurewindow));
    mainbox = gtk_hbox_new(FALSE, 2);

    thing = gtk_label_new(_("Highlight rows (major / minor):"));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);

    adj = gtk_adjustment_new((double)gui_settings.highlight_rows_minor_n, 1, 16, 1, 2, 0.0);
    thing = extspinbutton_new(GTK_ADJUSTMENT(adj), 0, 0, FALSE);
    gtk_box_pack_end(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(thing), 0);
    g_signal_connect(thing, "value-changed",
        G_CALLBACK(gui_settings_highlight_rows_minor_changed), NULL);

    adj = gtk_adjustment_new((double)gui_settings.highlight_rows_n, 1, 32, 1, 2, 0.0);
    majspin = extspinbutton_new(GTK_ADJUSTMENT(adj), 0, 0, FALSE);
    gtk_box_pack_end(GTK_BOX(mainbox), majspin, FALSE, FALSE, 0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(majspin), 0);
    g_signal_connect(majspin, "value-changed",
        G_CALLBACK(gui_settings_highlight_rows_changed), NULL);

    gtk_box_pack_start(GTK_BOX(vbox), mainbox, TRUE, TRUE, 0);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 4);

    gtk_widget_show_all(measurewindow);
    gtk_widget_grab_focus(majspin);
}

static void
measure_changed(GtkWidget* widget, gpointer data)
{
    gint measure_chosen;
    guint maxmeasure = GPOINTER_TO_INT(data);

    if ((measure_chosen = gtk_combo_box_get_active(GTK_COMBO_BOX(widget))) <= (maxmeasure - 1)) {
        if (measurewindow && gtk_widget_get_visible(measurewindow))
            gtk_widget_hide(measurewindow);
        if ((gui_settings.highlight_rows_n != measure_msr[measure_chosen].major) || (gui_settings.highlight_rows_minor_n != measure_msr[measure_chosen].minor)) {
            gui_settings.highlight_rows_n = measure_msr[measure_chosen].major;
            gui_settings.highlight_rows_minor_n = measure_msr[measure_chosen].minor;
            tracker_redraw(tracker);
            /* to make keyboard working immediately after chosing the measure */
            gui_unset_focus();
        }
        /* Gtk+ stupidity: when combo box list is popped down, */
    } else if (measure_chosen == maxmeasure + 1)
        measure_dialog();
}

static void
popwin_hide(GtkWidget* widget, GParamSpec* ps, gpointer data)
{
    gboolean shown;
    guint maxmeasure = GPOINTER_TO_INT(data);

    g_object_get(G_OBJECT(widget), "popup-shown", &shown, NULL);
    if (!shown && gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) == maxmeasure + 1) /* Popup is hidden by clicking on "Other..." */
        measure_dialog();
}

static gboolean
gui_update_title_idle_func()
{
    gchar* title;

    if (current_filename) {
        gchar* bn;

        bn = g_path_get_basename(current_filename);
        title = g_strdup_printf(PACKAGE_NAME " " VERSION ": %s%s", tidata.modified ? "*" : "", bn);
        g_free(bn);
    } else
        title = g_strdup_printf(PACKAGE_NAME " " VERSION ": %s%s", tidata.modified ? "*" : "", _("<NoName>"));
    gtk_window_set_title(GTK_WINDOW(mainwindow), title);

    g_free(title);
    tidata.handler = 0;
    return FALSE;
}

GtkWidget*
gui_get_pixmap(GuiPixmap pm)
{
    g_assert(pm < GUI_PIXMAP_LAST);
    return gtk_image_new_from_file(pixmaps[pm]);
}

void gui_update_title(const gchar* filename)
{
    static gboolean was_modified = FALSE;

    tidata.modified = history_get_modified();

    /* To reduce overhead due to excessive calls of the gui_update_title() */
    if (filename == NULL && was_modified == tidata.modified)
        return;

    if (filename && g_strcmp0(filename, current_filename)) {
        if (current_filename) {
            g_free(current_filename);
        }
        current_filename = g_strdup(filename);
    }
    if (!tidata.handler)
        tidata.handler = g_idle_add((GSourceFunc)gui_update_title_idle_func, NULL);
    g_assert(tidata.handler != 0);

    was_modified = tidata.modified;
}

void gui_reset_title()
{
    if (current_filename)
        g_free(current_filename);

    current_filename = NULL;
    tidata.modified = FALSE;
    if (!tidata.handler)
        tidata.handler = g_idle_add((GSourceFunc)gui_update_title_idle_func, NULL);
    g_assert(tidata.handler != 0);
}

static gboolean
status_update_idle_func(const gchar *mess)
{
    g_assert(mess != NULL);
    gtk_label_set_text(GTK_LABEL(status_bar), mess);
    return FALSE;
}

void
gui_statusbar_update_message(gchar* message, gboolean force_update)
{
    base_message = message;

    /* Take care here... GUI callbacks can be called at this point. */
    if (force_update) {
        gtk_label_set_text(GTK_LABEL(status_bar), message);
        while (gtk_events_pending())
            gtk_main_iteration();
    } else
        g_idle_add((GSourceFunc)status_update_idle_func, base_message);
}

void
gui_statusbar_update(int message, gboolean force_update)
{
    gui_statusbar_update_message(_(status_messages[message]), force_update);
}

/* Allowing to output something over the base level message */
void
gui_statusbar_update_message_high(gchar* message)
{
    g_idle_add((GSourceFunc)status_update_idle_func, message);
}

/* Return to the base level message */
void 
gui_statusbar_restore_base_message(void)
{
    g_idle_add((GSourceFunc)status_update_idle_func, base_message);
}

static inline void
gui_mixer_play_pattern(int pattern,
    int row,
    int stop_after_row,
    int stoppos,
    int ch_start,
    int num_ch)
{
    audio_ctlpipe_write(AUDIO_CTLPIPE_PLAY_PATTERN, pattern, row, stop_after_row, (gint)gui_settings.looped,
        stoppos, ch_start, num_ch);
}

static inline void
gui_mixer_stop_playing(void)
{
    audio_ctlpipe_write(AUDIO_CTLPIPE_STOP_PLAYING);
}

static inline void
gui_mixer_set_songpos(int songpos)
{
    audio_ctlpipe_write(AUDIO_CTLPIPE_SET_SONGPOS, songpos);
}

static inline void
gui_mixer_set_pattern(int pattern)
{
    audio_ctlpipe_write(AUDIO_CTLPIPE_SET_PATTERN, pattern);
}

static void
gui_recent_add_item(const gchar *filename)
{
    gchar* uri = g_filename_to_uri(filename, NULL, NULL);

    gtk_recent_manager_add_item(rcmgr, uri);
    g_free(uri);
}

static void
gui_save(const gchar* data, gchar* localname, gboolean save_smpls, gboolean switch_needed)
{
    gboolean need_free = FALSE;
    STSaveStatus status;

    if (!localname) {
        localname = gui_filename_from_utf8(data);
        need_free = TRUE;
    }

    if (!localname)
        return;

    gui_statusbar_update(STATUS_SAVING_MODULE, TRUE);
    if ((status = XM_Save(xm, localname, data, save_smpls, TRUE))) {
        if (status == STATUS_ERR) {
            static GtkWidget* dialog = NULL;

            gui_error_dialog(&dialog, _("Saving module failed"), FALSE);
        }
        gui_statusbar_update(STATUS_IDLE, FALSE);
    } else {
        if (switch_needed)
            gui_auto_switch_page();
        gui_statusbar_update(STATUS_MODULE_SAVED, FALSE);
        gui_update_title(data);
        gui_recent_add_item(localname);
        if (save_smpls)
            history_save();
    }

    if (need_free)
        g_free(localname);
}

void gui_save_current(void)
{
    if (current_filename)
        gui_save(current_filename, NULL, TRUE, FALSE);
    else
        fileops_open_dialog(savemod);
}

static const char* channelslabels[] = { N_("Mono"), N_("Stereo") };
static const char* res_labels[] = { N_("8 bit"), N_("16 bit") };
struct FormatDialog{
    gboolean is_stereo, is_16bit;
    guint freq;
    GtkWidget* config_dialog;
    GtkWidget *prefs_channels_w[ARRAY_SIZE(channelslabels)], *prefs_res_w[ARRAY_SIZE(res_labels)];
    GtkTreeModel* model;
};

static void
prefs_channels_changed(GtkToggleButton* w, struct FormatDialog* self)
{
    if (gtk_toggle_button_get_active(w)) {
        gint curr;

        if ((curr = find_current_toggle(self->prefs_channels_w, ARRAY_SIZE(channelslabels))) >= 0)
            self->is_stereo = curr;
    }
}

static void
prefs_res_changed(GtkToggleButton* w, struct FormatDialog* self)
{
    if (gtk_toggle_button_get_active(w)) {
        gint curr;

        if ((curr = find_current_toggle(self->prefs_res_w, ARRAY_SIZE(res_labels))) >= 0)
            self->is_16bit = curr;
    }
}

static void
prefs_mixfreq_changed(GtkComboBox* prefs_mixfreq, struct FormatDialog* self)
{
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(prefs_mixfreq), &iter))
        gtk_tree_model_get(self->model, &iter, 0, &self->freq, -1);
}

static gboolean
format_dialog(struct FormatDialog* self,
    const gchar* title,
    const gboolean is_stereo,
    const gboolean is_16bit,
    const guint32 freq)
{
    gint response;

    if (!self->config_dialog) {
        static const guint mixfreqs[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 64000, 88200, 96000 };
        GtkWidget *mainbox, *box2, *thing;
        GtkListStore* ls;
        GtkCellRenderer* cell;
        GtkTreeIter iter;
        guint i;

        self->is_stereo = is_stereo;
        self->is_16bit = is_16bit;
        self->freq = freq;

        self->config_dialog = gtk_dialog_new_with_buttons(_(title), GTK_WINDOW(mainwindow),
            GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(self->config_dialog, GTK_RESPONSE_OK);

        mainbox = gtk_dialog_get_content_area(GTK_DIALOG(self->config_dialog));
        gtk_box_set_spacing(GTK_BOX(mainbox), 2);

        box2 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

        thing = gtk_label_new(_("Channels:"));
        gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
        make_radio_group_full_ext(channelslabels, box2, self->prefs_channels_w, FALSE, TRUE,
            (void (*)())prefs_channels_changed, self, TRUE);
        gui_set_radio_active(self->prefs_channels_w, self->is_stereo);

        box2 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

        thing = gtk_label_new(_("Resolution:"));
        gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
        make_radio_group_full_ext(res_labels, box2, self->prefs_res_w, FALSE, TRUE,
            (void (*)())prefs_res_changed, self, TRUE);
        gui_set_radio_active(self->prefs_res_w, self->is_16bit);

        box2 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

        thing = gtk_label_new(_("Frequency [Hz]:"));
        gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
        ls = gtk_list_store_new(1, G_TYPE_UINT);
        self->model = GTK_TREE_MODEL(ls);
        thing = gtk_combo_box_new_with_model(self->model);
        g_object_unref(ls);
        for (i = 0; i < ARRAY_SIZE(mixfreqs); i++) {
            gtk_list_store_append(GTK_LIST_STORE(self->model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(self->model), &iter, 0, mixfreqs[i], -1);
        }
        cell = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(thing), cell, TRUE);
        gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(thing), cell, "text", 0, NULL);
        gui_set_active_combo_item(thing, self->model, self->freq);
        gtk_box_pack_end(GTK_BOX(box2), thing, FALSE, TRUE, 0);
        g_signal_connect(thing, "changed",
            G_CALLBACK(prefs_mixfreq_changed), self);

        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 4);
        gtk_widget_show_all(self->config_dialog);
    }
    response = gtk_dialog_run(GTK_DIALOG(self->config_dialog));
    gtk_widget_hide(self->config_dialog);

    return response == GTK_RESPONSE_OK;
}

static void proc_stop(GtkWidget* w, gint id, GtkSpinner* s)
{
    stop_process = TRUE;
    gtk_spinner_stop(s);
    gtk_widget_hide(w);
}

static gboolean proc_delete(void)
{
    return TRUE;
}

static GtkWidget*
show_process_window(const gchar* text)
{
    static GtkWidget *dialog = NULL, *spinner, *label;

    if (!dialog) {
        GtkWidget *vbox, *hbox;

        dialog = gtk_dialog_new_with_buttons("", GTK_WINDOW(mainwindow),
            GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

        hbox = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
        label = gtk_label_new("");
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        spinner = gtk_spinner_new();
        gtk_widget_set_size_request(spinner, 20, 20);
        gtk_box_pack_end(GTK_BOX(hbox), spinner, FALSE, FALSE, 0);
        gtk_widget_show_all(vbox);

        g_signal_connect(dialog, "delete_event", G_CALLBACK(proc_delete), NULL);
        g_signal_connect(dialog, "response", G_CALLBACK(proc_stop), spinner);
    }
    stop_process = FALSE;
    gtk_label_set_text(GTK_LABEL(label), _(text));
    gtk_spinner_start(GTK_SPINNER(spinner));
    gtk_window_present(GTK_WINDOW(dialog));

    return dialog;
}

#define SNDBUF_SIZE 16384

#if USE_SNDFILE || AUDIOFILE_VERSION
static void
save_wav(const gchar* fn, const gchar* path)
{
    static struct FormatDialog fd = {0};

    gint8* sndbuf;
    STMixerFormat format;
    guint32 num_samples, num_rendered;
    GtkWidget* pw;
#if USE_SNDFILE
    SNDFILE* outfile;
    SF_INFO sfinfo;
#else
    AFfilehandle outfile;
#endif

    if (!format_dialog(&fd, N_("File output"),
        gui_settings.file_out_channels - 1,
        gui_settings.file_out_resolution >> 4,
        gui_settings.file_out_mixfreq))
        return;
    gui_settings.file_out_channels = fd.is_stereo + 1;
    gui_settings.file_out_resolution = (fd.is_16bit << 3) + 8;
    gui_settings.file_out_mixfreq = fd.freq;

#if USE_SNDFILE
    sfinfo.channels = gui_settings.file_out_channels;
    sfinfo.samplerate = gui_settings.file_out_mixfreq;
    sfinfo.format = SF_FORMAT_WAV |
        (gui_settings.file_out_resolution == 16 ? SF_FORMAT_PCM_16 : SF_FORMAT_PCM_U8);

    errno = 0;
    outfile = sf_open(path, SFM_WRITE, &sfinfo);
#else
    AFfilesetup outfilesetup;

    outfilesetup = afNewFileSetup();
    afInitFileFormat(outfilesetup, AF_FILE_WAVE);
    afInitChannels(outfilesetup, AF_DEFAULT_TRACK, gui_settings.file_out_channels);
    afInitRate(outfilesetup, AF_DEFAULT_TRACK, gui_settings.file_out_mixfreq);
    afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK,
        gui_settings.file_out_resolution == 16 ? AF_SAMPFMT_TWOSCOMP : AF_SAMPFMT_UNSIGNED,
        gui_settings.file_out_resolution);
    errno = 0;
    outfile = afOpenFile(path, "w", outfilesetup);
    if (outfile)
        afFreeFileSetup(outfilesetup);
#endif

    if (!outfile) {
        gui_errno_dialog(mainwindow, _("Can't open file for writing"), errno);
        return;
    }

    /* In case we're running setuid root... */
    if (chown(path, getuid(), getgid()) == -1)
        gui_errno_dialog(mainwindow, _("Can't change file ownership"), errno);

    sndbuf = malloc(SNDBUF_SIZE);
    if (!sndbuf) {
        static GtkWidget* dialog = NULL;

        gui_error_dialog(&dialog, _("Can't allocate mix buffer."), FALSE);
        return;
    }

    gui_play_stop();
    audio_prepare_for_rendering(AUDIO_RENDER_SONG, 0, 0, 0, 0, 0);

    num_samples = (SNDBUF_SIZE >> (gui_settings.file_out_channels - 1));
#if USE_SNDFILE
    /* libsndfile can handle only 16-bit samples, even when it saves a 8-bit wav */
    num_samples = num_samples >> 1;
#else
    num_samples = num_samples >> (gui_settings.file_out_resolution >> 4);
    if (gui_settings.file_out_resolution == 16) {
#endif
#ifdef WORDS_BIGENDIAN
        format = ST_MIXER_FORMAT_S16_BE;
#else
        format = ST_MIXER_FORMAT_S16_LE;
#endif
#if !USE_SNDFILE
    } else
        format = ST_MIXER_FORMAT_U8;
#endif
    format |= (gui_settings.file_out_channels == 2 ? ST_MIXER_FORMAT_STEREO : 0);

    pw = show_process_window(N_("Saving..."));
    do {
        gint num_written;

        num_rendered = audio_mix(sndbuf, num_samples, gui_settings.file_out_mixfreq, format, FALSE, NULL);
#if USE_SNDFILE
        num_written = sf_writef_short(outfile, (short int*)sndbuf, num_rendered);
#else
        num_written = afWriteFrames(outfile, AF_DEFAULT_TRACK, sndbuf, num_rendered);
#endif
        if (num_written != num_rendered) {
            gui_errno_dialog(mainwindow, _("An error occured while writing to file"), errno);
            break;
        }
        while (gtk_events_pending())
            gtk_main_iteration();
    } while (num_rendered == num_samples && !stop_process);
    gtk_widget_hide(pw);

    audio_cleanup_after_rendering();
#if USE_SNDFILE
    sf_close(outfile);
#else
    afCloseFile(outfile);
#endif
    g_free(sndbuf);
}
#endif /* USE_SNDFILE || AUDIOFILE_VERSION */

static void
gui_render_to_sample(audio_render_target what,
    gint pattern,
    gint patpos,
    gint stoppos,
    gint ch_start,
    gint num_ch)
{
    static struct FormatDialog fd = {0};

    GtkWidget* pw;
    STMixerFormat format;
    guint32 num_samples, num_rendered, length = 0;
    STSampleChain *renderbufs = NULL, *oldbuf;

    if (!format_dialog(&fd, N_("Sample rendering"), TRUE, TRUE, 44100))
        return;
    format = fd.is_16bit ? ST_MIXER_FORMAT_S16_LE : ST_MIXER_FORMAT_S8;
    if (fd.is_stereo)
        format |= ST_MIXER_FORMAT_STEREO;

    gui_play_stop();
    audio_prepare_for_rendering(what, pattern, patpos, stoppos, ch_start, num_ch);
    num_samples = (SNDBUF_SIZE >> (fd.is_16bit + fd.is_stereo));

    pw = show_process_window(N_("Rendering"));
    do {
        STSampleChain *newbuf = malloc(sizeof(STSampleChain));

        if (!newbuf) {
            gui_oom_error();
            sample_editor_clear_buffers(renderbufs);
            return;
        }
        newbuf->next = NULL;
        newbuf->data = malloc(SNDBUF_SIZE);
        if (!newbuf->data) {
            gui_oom_error();
            sample_editor_clear_buffers(renderbufs);
            return;
        }
        if (!renderbufs)
            renderbufs = newbuf;
        else {
            oldbuf->next = newbuf;
        }
        oldbuf = newbuf;

        num_rendered = audio_mix(newbuf->data, num_samples, fd.freq, format, FALSE, NULL);
        newbuf->length = num_rendered << (fd.is_16bit + fd.is_stereo);
        length += newbuf->length;

        while (gtk_events_pending())
            gtk_main_iteration();
    } while (num_rendered == num_samples && !stop_process);
    gtk_widget_hide(pw);

    if (!stop_process)
        sample_editor_chain_to_sample(renderbufs, length, format, fd.freq,
            N_("<just rendered>"), N_("rendered"), N_("Sample rendering"));
    sample_editor_clear_buffers(renderbufs);
    audio_cleanup_after_rendering();
}

void
gui_song_to_sample(void) {
    gui_render_to_sample(AUDIO_RENDER_SONG, 0, 0, 0, 0, 0);
}

void
gui_pattern_to_sample(void) {
    gui_render_to_sample(AUDIO_RENDER_PATTERN, editing_pat, 0, -1, -1, -1);
}

void
gui_track_to_sample(void) {
    gui_render_to_sample(AUDIO_RENDER_TRACK, editing_pat, 0, -1, tracker->cursor_ch, 1);
}

void
gui_block_to_sample(void) {
    gint ch_start, row_start, n_ch, n_rows;

    tracker_get_selection_rect(tracker, &ch_start, &row_start, &n_ch, &n_rows);
    if (ch_start >= 0 && row_start >= 0)
        gui_render_to_sample(AUDIO_RENDER_BLOCK, editing_pat,
            row_start, row_start + n_rows, ch_start, n_ch);
}

typedef struct {
    gint len, channels, pat, patpos;
    /* Manual 2D indexing is supposed */
    XMNote notes[1];
} PatternArg;

static void
gui_pattern_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    Tracker* t;
    PatternArg* pa = arg;
    XMNote* tmp_notes;
    gint i, tmp_length, src_chaddr, dst_chaddr;
    XMPattern* pattern;

    g_assert(IS_TRACKER(data));
    t = TRACKER(data);

    pattern = (pa->pat < 0) ? t->curpattern : &xm->patterns[pa->pat];
    tmp_notes = g_malloc(MAX(pattern->length, pa->len) * pa->channels * sizeof(XMNote));

    /* Current pattern is already set by undo routines */
    tmp_length = pa->len;
    pa->len = pattern->length;
    st_set_pattern_length(pattern, tmp_length);
    for (i = 0, src_chaddr = 0, dst_chaddr = 0; i < pa->channels;
        i++, dst_chaddr += pa->len, src_chaddr += tmp_length) {
        memcpy(&tmp_notes[dst_chaddr], pattern->channels[i], pa->len * sizeof(XMNote));
        memcpy(pattern->channels[i], &pa->notes[src_chaddr], tmp_length * sizeof(XMNote));
    }
    memcpy(pa->notes, tmp_notes, pa->len * pa->channels * sizeof(XMNote));

    if (pa->pat >= 0 && redo)
        gui_set_current_pattern(pa->pat, TRUE);
    gui_update_pattern_data();
    if (pa->pat < 0) {
        tracker_set_pattern(t, NULL);
        tracker_set_pattern(t, pattern);
    }
    if (pa->patpos >=0)
        track_editor_set_patpos(pa->patpos);

    g_free(tmp_notes);
}

void
gui_log_pattern_full(XMPattern* pattern, const gchar* title, const gint pat,
    gint len, gint alloc_len, gint patpos)
{
    gsize asize, chansize;
    PatternArg* arg;
    gint i, chaddr;

    len = len > 0 ? len : pattern->length;
    alloc_len = alloc_len > 0 ? alloc_len : pattern->length;
    g_assert(len <= alloc_len);

    chansize = len * sizeof(XMNote);
    asize = sizeof(PatternArg) + (alloc_len * xm->num_channels - 1) * sizeof(XMNote);
    arg = g_malloc(asize);

    arg->len = len;
    arg->pat = pat;
    arg->patpos = patpos;
    arg->channels = xm->num_channels;
    for (i = 0, chaddr = 0; i < xm->num_channels; i++, chaddr += len)
        memcpy(&arg->notes[chaddr], pattern->channels[i], chansize);

    history_log_action(HISTORY_ACTION_POINTER, _(title),
        HISTORY_FLAG_LOG_POS | HISTORY_FLAG_LOG_PAT |
        HISTORY_SET_PAGE(NOTEBOOK_PAGE_TRACKER),
        gui_pattern_undo, tracker, asize, arg);
}

struct SongArg {
    guint8 lengths[XM_NUM_PATTERNS];
    XMNote notes[1];
};

static void
song_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    struct SongArg* sa = arg;
    XM* mod = data;
    XMNote* tmp_notes = NULL;
    gsize alloc_len = 0, addr;
    gint i, j;

    for (i = 0, addr = 0; i < XM_NUM_PATTERNS; i++) {
        /* Taking stored length value rather than that from a pattern
           to feel a little bit safer */
        const gsize len = sa->lengths[i], chansize = len * sizeof(XMNote);

        if (len > alloc_len) {
            if (tmp_notes) {
                g_free(tmp_notes);
            }
            alloc_len = len;
            tmp_notes = g_new(XMNote, alloc_len);
        }
        for (j = 0; j < mod->num_channels; j++, addr += len) {
            memcpy(tmp_notes, mod->patterns[i].channels[j], chansize);
            memcpy(mod->patterns[i].channels[j], &sa->notes[addr], chansize);
            memcpy(&sa->notes[addr], tmp_notes, chansize);
        }
    }

    g_free(tmp_notes);
    tracker_redraw(tracker);
}

void
gui_log_song(const gchar* title, XM* mod)
{
    gint i, j;
    struct SongArg* arg;
    gsize len = 0, arg_size, addr, chansize;

    for (i = 0; i < XM_NUM_PATTERNS; i++)
        len += mod->patterns[i].length;
    arg_size = sizeof(struct SongArg) + sizeof(XMNote) * (len * mod->num_channels - 1);
    arg = g_malloc(arg_size);

    for (i = 0, addr = 0; i < XM_NUM_PATTERNS; i++) {
        arg->lengths[i] = len = mod->patterns[i].length;
        chansize = len * sizeof(XMNote);
        for (j = 0; j < mod->num_channels; j++, addr += len) {
            memcpy(&arg->notes[addr], mod->patterns[i].channels[j], chansize);
        }
    }

    history_log_action(HISTORY_ACTION_POINTER, _(title),
        HISTORY_FLAG_LOG_POS | HISTORY_FLAG_LOG_PAT |
        HISTORY_SET_PAGE(NOTEBOOK_PAGE_TRACKER),
        song_undo, mod, arg_size, arg);
}

void gui_find_unused_pattern(void)
{
    gint n = st_find_first_unused_and_empty_pattern(xm);

    if (n != -1)
        gui_set_current_pattern(n, TRUE);
}

void gui_copy_to_unused_pattern(void)
{
    gint n = st_find_first_unused_and_empty_pattern(xm);
    XMPattern* epat = &xm->patterns[editing_pat];

    if (n != -1 && !st_is_empty_pattern(epat)) {
        XMPattern* newpat = &xm->patterns[n];

        gui_log_pattern(newpat, N_("Pattern duplicating"), n,
            newpat->length, MAX(newpat->length, epat->length));
        gui_play_stop();
        st_copy_pattern(newpat, epat);
        gui_set_current_pattern(n, TRUE);
    }
}

static void
gui_shrink_callback(XMPattern* data)
{
    gui_log_pattern(data, N_("Pattern shrinking"), -1, data->length, data->length);
    st_shrink_pattern(data);
    gui_update_pattern_data();
    tracker_set_pattern(tracker, NULL);
    tracker_set_pattern(tracker, data);
}

void gui_shrink_pattern()
{
    XMPattern* patt = tracker->curpattern;

    if (st_check_if_odd_are_not_empty(patt)) {
        if (gui_ok_cancel_modal(mainwindow,
                _("Odd pattern rows contain data which will be lost after shrinking.\n"
                  "Do you want to continue anyway?")))
            gui_shrink_callback(patt);
    } else {
        gui_shrink_callback(patt);
    }
}

static void
gui_expand_callback(XMPattern* data)
{
    gui_log_pattern(data, N_("Pattern expanding"), -1,
        data->length, MIN(data->length << 1, 256));
    st_expand_pattern(data);
    gui_update_pattern_data();
    tracker_set_pattern(tracker, NULL);
    tracker_set_pattern(tracker, data);
}

void gui_expand_pattern()
{
    XMPattern* patt = tracker->curpattern;

    if (patt->length > 128) {
        if (gui_ok_cancel_modal(mainwindow,
                _("The pattern is too long for expanding.\n"
                  "Some data at the end of the pattern will be lost.\n"
                  "Do you want to continue anyway?")))
            gui_expand_callback(patt);
    } else {
        gui_expand_callback(patt);
    }
}

static gboolean
gui_pattern_length_correct(FILE* f, int length, gint reply)
{
    XMPattern* patt = tracker->curpattern;

    switch (reply) {
    case GTK_RESPONSE_YES: /* Yes! */
        st_set_pattern_length(patt, length);
        gui_update_pattern_data(); /* Falling through */
    case GTK_RESPONSE_NO: /* No! */
        return TRUE;
    case 2: /* Cancel, do nothing */
    default:
        break;
    }
    return FALSE;
}

static gboolean
load_xm_full(const gchar* fn, const gchar* localpath)
{
    gboolean success = TRUE;

    static GtkWidget* dialog = NULL;

    if (history_get_modified()) {
        gint response;

        if (!dialog)
            dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                _("Are you sure you want to free the current project?\nAll changes will be lost!"));

        response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_hide(dialog);
        if (response == GTK_RESPONSE_OK) {
            success = gui_load_xm(fn, localpath);
            gui_auto_switch_page();
        }
    } else {
        success = gui_load_xm(fn, localpath);
        gui_auto_switch_page();
    }

    return success;
}

static inline void
load_xm(const gchar* fn, const gchar* localpath)
{
    load_xm_full(fn, localpath);
}

static void
save_song(const gchar* fn, gchar* localname)
{
    gui_save(fn, localname, TRUE, TRUE); /* with samples */
}

static void
save_xm(const gchar* fn, gchar* localname)
{
    gui_save(fn, localname, FALSE, TRUE); /* without samples */
}

static void
save_pat(gchar* fn, gchar* localname)
{
    xm_xp_save(localname, fn, tracker->curpattern, xm);
}

static void
load_pat(const gchar* fn, const gchar* localname)
{
    gint length;
    gboolean will_load;
    FILE* f;
    static GtkWidget *dialog = NULL;

    XMPattern* patt = tracker->curpattern;

    f = gui_fopen(localname, fn, "rb");
    if (!f)
        return;

    if (xm_xp_load_header(f, &length)) {
        gint oldlength = patt->length;

        if (length == oldlength)
            will_load = TRUE;
        else {
            gint response;

            if (!dialog) {
                dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                    _("The length of the pattern being loaded doesn't match with that of current pattern in module.\n"
                      "Do you want to change the current pattern length?"));
                gtk_dialog_add_buttons(GTK_DIALOG(dialog), GTK_STOCK_YES, GTK_RESPONSE_YES, GTK_STOCK_NO, GTK_RESPONSE_NO,
                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
            }

            response = gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_hide(dialog);
            will_load = gui_pattern_length_correct(f, length, response);
        }
        if (will_load) {
            gui_log_pattern(patt, N_("Pattern loading"), -1, oldlength, MAX(length, oldlength));
            if (xm_xp_load(f, length, patt, xm)) {
                tracker_set_pattern(tracker, NULL);
                tracker_set_pattern(tracker, patt);
            }
        }
    }
    fclose(f);
}

static void
current_instrument_changed(GtkSpinButton* spin)
{
    STInstrument* i = &xm->instruments[curinst = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin)) - 1];
    STSample* s = &i->samples[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    instrument_editor_set_instrument(i, curinst);
    sample_editor_set_sample(s);
    modinfo_set_current_instrument(curinst);
}

static void
current_instrument_name_changed(void)
{
    gchar* term;
    gint curins;

    STInstrument* i = &xm->instruments[curins = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin)) - 1];

    history_log_entry(GTK_ENTRY(gui_curins_name), _("Instrument name changing"), 22 * 4,
        HISTORY_FLAG_LOG_INS, i->utf_name);

    g_utf8_strncpy(i->utf_name, gtk_entry_get_text(GTK_ENTRY(gui_curins_name)), 22);
    term = g_utf8_offset_to_pointer(i->utf_name, 23);
    term[0] = 0;
    i->needs_conversion = TRUE;
    modinfo_update_instrument(curins);
}

static void
current_sample_changed(GtkSpinButton* spin)
{
    int smpl;

    STInstrument* i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin)) - 1];
    STSample* s = &i->samples[smpl = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    sample_editor_set_sample(s);
    modinfo_set_current_sample(smpl);
}

static void
current_sample_name_changed(void)
{
    gchar* term;
    gint cursmpl;
    STInstrument* i = &xm->instruments[gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin)) - 1];
    STSample* s = &i->samples[cursmpl = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))];

    history_log_entry(GTK_ENTRY(gui_cursmpl_name), _("Sample name changing"), 22 * 4,
        HISTORY_FLAG_LOG_INS | HISTORY_FLAG_LOG_SMP, s->utf_name);

    g_utf8_strncpy(s->utf_name, gtk_entry_get_text(GTK_ENTRY(gui_cursmpl_name)), 22);
    term = g_utf8_offset_to_pointer(i->utf_name, 23);
    term[0] = 0;
    s->needs_conversion = TRUE;
    modinfo_update_sample(cursmpl);
}

static void
spin_db_changed(GtkSpinButton* db, GtkAdjustment* adj)
{
    gtk_adjustment_set_value(adj, 20.0 - gtk_spin_button_get_value(db));
}

void gui_go_to_page(gint page)
{
    if (notebook_current_page != page) {
        if (notebook_current_page == NOTEBOOK_PAGE_FILE)
            fileops_restore_subpage();
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),
            page);
    }
}

typedef struct {
    GtkWidget *dialog, *ilist;
    gint num;
} ISelector;

static void
instrument_selected(GtkTreeView* tv,
    GtkTreePath* path,
    GtkTreeViewColumn* column,
    ISelector* s)
{
    gint* indices = gtk_tree_path_get_indices(path);

    gui_set_current_instrument(indices[0] + 1);
    gtk_widget_hide(s->dialog);
}

static void
iselector_response(GtkWidget* dialog,
    gint response,
    ISelector* s)
{
    if (response == GTK_RESPONSE_OK) {
        gint index = gui_list_get_selection_index(gtk_tree_view_get_selection(GTK_TREE_VIEW(s->ilist)));

        if (index != -1)
            gui_set_current_instrument(index + 1);
    }
    gtk_widget_hide(s->dialog);
}

static gboolean
iselector_keypress(GtkWidget* widget,
    GdkEventKey* event,
    ISelector* s)
{
    gint digit = -1;

    switch (event->keyval) {
    case '0':
    case GDK_KEY_KP_0:
    case GDK_KEY_KP_Insert:
        digit = 0;
        break;
    case '1':
    case GDK_KEY_KP_1:
    case GDK_KEY_KP_End:
        digit = 1;
        break;
    case '2':
    case GDK_KEY_KP_2:
    case GDK_KEY_KP_Down:
        digit = 2;
        break;
    case '3':
    case GDK_KEY_KP_3:
    case GDK_KEY_KP_Page_Down:
        digit = 3;
        break;
    case '4':
    case GDK_KEY_KP_4:
    case GDK_KEY_KP_Left:
        digit = 4;
        break;
    case '5':
    case GDK_KEY_KP_5:
    case GDK_KEY_KP_Begin:
        digit = 5;
        break;
    case '6':
    case GDK_KEY_KP_6:
    case GDK_KEY_KP_Right:
        digit = 6;
        break;
    case '7':
    case GDK_KEY_KP_7:
    case GDK_KEY_KP_Home:
        digit = 7;
        break;
    case '8':
    case GDK_KEY_KP_8:
    case GDK_KEY_KP_Up:
        digit = 8;
        break;
    case '9':
    case GDK_KEY_KP_9:
    case GDK_KEY_KP_Page_Up:
        digit = 9;
        break;
    case GDK_KEY_Escape:
        gtk_widget_hide(s->dialog);
        return TRUE;
    default:
        break;
    }

    if (digit >= 0) {
        if (s->num == -1 && digit)
            s->num = digit;
        else {
            if (s->num <= 12)
                s->num = s->num * 10 + digit;
            if (s->num > 128)
                s->num = 128;
        }
        gui_list_select(s->ilist, s->num - 1, TRUE, 0.5);
        return TRUE;
    }

    return FALSE;
}

static gboolean
iselector_keyrelease(GtkWidget* widget,
    GdkEventKey* event,
    ISelector* s)
{
    if ((event->keyval == GDK_KEY_Control_L) && s->num > 0) {
        gui_set_current_instrument(s->num);
        gtk_widget_hide(s->dialog);
        return TRUE;
    }

    return FALSE;
}

static void
gui_show_instr_selector(const gint first_digit)
{
    static ISelector isel = {.dialog = NULL, .num = -1};
    static GtkListStore* list_store;
    GtkTreeModel* model;
    GtkTreeIter iter;
    gint i;

    if (!isel.dialog) {
        GtkWidget* vbox;
        static const gchar* ititles[2] = { "n", N_("Instrument Name")};
        const gfloat ialignments[2] = { 0.5, 0.0 };
        const gboolean iexpands[2] = { FALSE, TRUE };
        GType itypes[2] = { G_TYPE_INT, G_TYPE_STRING };

        isel.dialog = gtk_dialog_new_with_buttons(_("Quick instrument selection"), GTK_WINDOW(mainwindow),
            GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(isel.dialog, GTK_RESPONSE_OK);
        gui_dialog_connect_data(isel.dialog, G_CALLBACK(iselector_response), &isel);
        gtk_widget_add_events(isel.dialog, GDK_KEY_RELEASE_MASK);
        g_signal_connect(isel.dialog, "key_press_event",
            G_CALLBACK(iselector_keypress), &isel);
        g_signal_connect(isel.dialog, "key_release_event",
            G_CALLBACK(iselector_keyrelease), &isel);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(isel.dialog));
        isel.ilist = gui_list_in_scrolled_window_full(2, ititles, vbox, itypes, ialignments,
            iexpands, GTK_SELECTION_BROWSE, TRUE, TRUE, NULL,
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        g_signal_connect(isel.ilist, "row_activated",
            G_CALLBACK(instrument_selected), &isel);
        gui_set_list_size(isel.ilist, -1, 14);
        gtk_widget_show_all(vbox);

        list_store = GUI_GET_LIST_STORE(isel.ilist);

        model = gui_list_freeze(isel.ilist);
        for (i = 1; i <= XM_NUM_INSTRUMENTS; i++) {
            gtk_list_store_append(list_store, &iter);
            gtk_list_store_set(list_store, &iter, 0, i, 1, xm->instruments[i - 1].utf_name, -1);
        }
        gui_list_thaw(isel.ilist, model);
    } else {
        model = gui_list_freeze(isel.ilist);
        for (i = 0; i < XM_NUM_INSTRUMENTS; i++) {
            if (!gui_list_get_iter(i, model, &iter))
                break; /* Some bullshit happens :-/ */
            gtk_list_store_set(list_store, &iter, 0, i + 1, 1, xm->instruments[i].utf_name, -1);
        }
        gui_list_thaw(isel.ilist, model);
    }

    isel.num = first_digit;
    gtk_window_set_position(GTK_WINDOW(isel.dialog), GTK_WIN_POS_MOUSE);
    gtk_window_present(GTK_WINDOW(isel.dialog));
    gtk_widget_grab_focus(isel.ilist);

    if (first_digit == -1) {
        gui_list_select(isel.ilist, curinst, TRUE, 0.5);
        isel.num = first_digit;
    } else
        gui_list_select(isel.ilist, first_digit - 1, TRUE, 0.5);
}

gboolean
gui_handle_standard_keys(int shift,
    int ctrl,
    int alt,
    guint32 keyval,
    gint hwcode)
{
    gboolean handled = FALSE, b;
    gint m = keys_get_key_meaning(keyval, ENCODE_MODIFIERS(shift, ctrl, alt), hwcode);
    int currpos;

    if (KEYS_MEANING_TYPE(m) == KEYS_MEANING_FUNC) {
        gint step = 1;

        switch (KEYS_MEANING_VALUE(m)) {
        case KEY_PLAY_SNG:
            play_song(GINT_TO_POINTER(FALSE));
            handled = TRUE;
            break;
        case KEY_REC_SNG:
            play_song(GINT_TO_POINTER(FALSE));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), TRUE);
            handled = TRUE;
            break;
        case KEY_PLAY_PAT:
            play_pattern();
            handled = TRUE;
            break;
        case KEY_REC_PAT:
            play_pattern();
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), TRUE);
            handled = TRUE;
            break;
        case KEY_PLAY_CUR:
            play_song(GINT_TO_POINTER(TRUE));
            handled = TRUE;
            break;
        case KEY_REC_CUR:
            play_song(GINT_TO_POINTER(TRUE));
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), TRUE);
            handled = TRUE;
            break;
        case KEY_PREV_POS_F:
            step = 10;
        case KEY_PREV_POS:
            /* previous position */
            currpos = playlist_get_position(playlist) - step;
            if (currpos < 0)
                currpos = 0;
            playlist_set_position(playlist, currpos);
            handled = TRUE;
            break;
        case KEY_NEXT_POS_F:
            step = 10;
        case KEY_NEXT_POS:
            /* previous position */
            currpos = playlist_get_position(playlist) + step;
            if (currpos >= xm->song_length)
                currpos = xm->song_length - 1;
            playlist_set_position(playlist, currpos);
            handled = TRUE;
            break;
        default:
            break;
        }
        if (handled)
            return TRUE;
    }

    switch (keyval) {
    case GDK_F1 ... GDK_F7:
        if (!shift && !ctrl && !alt) {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_octave), keyval - GDK_F1);
            handled = TRUE;
        }
        break;
    case '1' ... '8':
        if (notebook_current_page != NOTEBOOK_PAGE_SAMPLE_EDITOR && ctrl) {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_jump), keyval - '0');
            handled = TRUE;
        } else if (alt) {
            switch (keyval) {
            case '1' ... '5':
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), keyval - '1');
                break;
            default:
                break;
            }
            handled = TRUE;
        }
        break;
    case GDK_Home:
        if (ctrl) {
            /* first position */
            playlist_set_position(playlist, 0);
            handled = TRUE;
        }
        break;
    case GDK_End:
        if (ctrl) {
            /* first position */
            playlist_set_position(playlist, xm->song_length - 1);
            handled = TRUE;
        }
        break;
    case GDK_Left:
        if (ctrl) {
            /* previous instrument */
            gui_offset_current_instrument(shift ? -5 : -1);
            handled = TRUE;
        } else if (alt) {
            /* previous pattern */
            offset_current_pattern(shift ? -10 : -1);
            handled = TRUE;
        }
        break;
    case GDK_Right:
        if (ctrl) {
            /* next instrument */
            gui_offset_current_instrument(shift ? 5 : 1);
            handled = TRUE;
        } else if (alt) {
            /* next pattern */
            offset_current_pattern(shift ? 10 : 1);
            handled = TRUE;
        }
        break;
    case GDK_Up:
        if (ctrl) {
            /* next sample */
            gui_offset_current_sample(shift ? 4 : 1);
            handled = TRUE;
        }
        break;
    case GDK_Down:
        if (ctrl) {
            /* previous sample */
            gui_offset_current_sample(shift ? -4 : -1);
            handled = TRUE;
        }
        break;
    case ' ':
        if (alt || shift)
            break;

        handled = TRUE;

        b = GUI_ENABLED;
        if (!b)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), FALSE);
        gui_play_stop();
        if (ctrl)
            break;

        if (notebook_current_page == NOTEBOOK_PAGE_TRACKER && b)
            /* toggle editing mode (only if we haven't been in playing mode) */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), !GUI_EDITING);

        /* Hmm, maybe handle keyevents by the pages first and only than by the default GUI handler? */
        if (notebook_current_page == NOTEBOOK_PAGE_MODULE_INFO)
            modinfo_stop_cycling();
        break;
    case GDK_Escape:
        if (ctrl || alt || shift || notebook_current_page == NOTEBOOK_PAGE_FILE)
            break;
        /* toggle editing mode, even if we're in playing mode */
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), !GUI_EDITING);
        handled = TRUE;
        break;
    case GDK_KP_1:
    case GDK_KP_End:
        gui_show_instr_selector(1);
        handled = TRUE;
        break;
    case GDK_KP_2:
    case GDK_KP_Down:
        gui_show_instr_selector(2);
        handled = TRUE;
        break;
    case GDK_KP_3:
    case GDK_KP_Page_Down:
        gui_show_instr_selector(3);
        handled = TRUE;
        break;
    case GDK_KP_4:
    case GDK_KP_Left:
        gui_show_instr_selector(4);
        handled = TRUE;
        break;
    case GDK_KP_5:
    case GDK_KP_Begin:
        gui_show_instr_selector(5);
        handled = TRUE;
        break;
    case GDK_KP_6:
    case GDK_KP_Right:
        gui_show_instr_selector(6);
        handled = TRUE;
        break;
    case GDK_KP_7:
    case GDK_KP_Home:
        gui_show_instr_selector(7);
        handled = TRUE;
        break;
    case GDK_KP_8:
    case GDK_KP_Up:
        gui_show_instr_selector(8);
        handled = TRUE;
        break;
    case GDK_KP_9:
    case GDK_KP_Page_Up:
        gui_show_instr_selector(9);
        handled = TRUE;
        break;
    default:
        break;
    }

    return handled;
}

static gboolean
keyevent(GtkWidget* widget,
    GdkEventKey* event,
    gpointer data)
{
    static gboolean (*handle_page_keys[])(int, int, int, guint32, gboolean) = {
        fileops_page_handle_keys,
        track_editor_handle_keys,
        instrument_editor_handle_keys,
        sample_editor_handle_keys,
        modinfo_page_handle_keys,
    };
    static gboolean ctrl_just_pressed = FALSE;
    gboolean pressed = (gboolean)GPOINTER_TO_INT(data);
    gboolean handled = FALSE;
    gboolean entry_focus = GTK_IS_ENTRY(GTK_WINDOW(mainwindow)->focus_widget);

    if (!entry_focus && gtk_widget_get_visible(notebook)) {
        int shift = event->state & GDK_SHIFT_MASK;
        int ctrl = event->state & GDK_CONTROL_MASK;
        int alt = event->state & GDK_MOD1_MASK;

        handled = handle_page_keys[notebook_current_page](shift, ctrl, alt, event->keyval, pressed);

        if (handled)
            ctrl_just_pressed = FALSE;
        else {
            if (pressed) {
                handled = handled || gui_handle_standard_keys(shift, ctrl, alt, event->keyval, event->hardware_keycode);
                if (event->keyval == GDK_KEY_Control_L && !alt && !shift && gui_settings.ctrl_show_isel)
                    ctrl_just_pressed = TRUE;
                else
                    ctrl_just_pressed = FALSE;
            } else if ((event->keyval == GDK_KEY_Control_L)
                      && ctrl_just_pressed) {
                ctrl_just_pressed = FALSE;
                gui_show_instr_selector(-1);
            }
            switch (event->keyval) {
                /* from gtk+-1.2.8's gtkwindow.c. These keypresses need to
                   be stopped in any case. */
            case GDK_Up:
            case GDK_Down:
            case GDK_Left:
            case GDK_Right:
            case GDK_KP_Up:
            case GDK_KP_Down:
            case GDK_KP_Left:
            case GDK_KP_Right:
            case GDK_Tab:
            case GDK_ISO_Left_Tab:
                handled = TRUE;
                break;
            /* It's easier to do it here rather than in file-operations.c */
            case GDK_Escape:
                if (notebook_current_page == NOTEBOOK_PAGE_FILE)
                    gui_go_to_page(notebook_prev_page);
                break;
            }
        }

        if (handled) {
            if (pressed) {
                g_signal_stop_emission_by_name(G_OBJECT(widget), "key-press-event");
            } else {
                g_signal_stop_emission_by_name(G_OBJECT(widget), "key-release-event");
            }
        }
    } else {
        if (pressed) {
            switch (event->keyval) {
            case GDK_Tab:
                g_signal_stop_emission_by_name(G_OBJECT(widget), "key-press-event");
                gui_unset_focus();
                break;
            case GDK_Return:
            case GDK_KP_Enter:
                if (notebook_current_page == NOTEBOOK_PAGE_FILE) {
                    GtkWidget* parent = GTK_WINDOW(mainwindow)->focus_widget;

                    if (GTK_IS_ENTRY(parent) && /* File name entry */
                        !strncmp("GtkFileChooserEntry", gtk_widget_get_name(parent), 19))
                        fileops_enter_pressed();
                    else {
                    /* Checking if it is the directory creation entry. In this case we shouldn't
                       block signal propagation */
                        do {
                            parent = gtk_widget_get_parent(parent);
                            if (parent && GTK_IS_FILE_CHOOSER(parent))
                                break;
                        } while (parent);
                            /* Other entry having GtkFileChooser as a parent --- seems to be
                               the directory creation entry */
                        if (parent)
                            /* Leaving switch {} without stopping signal propagation */
                            break;
                    }
                }
                g_signal_stop_emission_by_name(G_OBJECT(widget), "key-press-event");
                handled = TRUE;
                gui_unset_focus();
                break;
            }
        }
    }

    return handled;
}

static void
gui_playlist_position_changed(Playlist* p,
    const gint newpos)
{
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songpos), newpos);
    if (gui_playing_mode == PLAYING_SONG) {
        // This will only be executed when the user changes the song position manually
        event_waiter_start(audio_songpos_ew);
        gui_mixer_set_songpos(newpos);
    } else {
        if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_lock_editpat))) {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_editpat),
                playlist_get_nth_pattern(p, newpos));
        }
    }
}

static void
gui_playlist_entry_changed(Playlist* p,
    gint pat)
{
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_lock_editpat)))
        gui_set_current_pattern(pat, TRUE);
}

static void
gui_playlist_songlength_changed(Playlist* p,
    const gint len)
{
    gtk_adjustment_set_upper(gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin_songpos)), len - 1);
    gtk_spin_button_update(GTK_SPIN_BUTTON(spin_songpos));
}

static void
gui_editpat_changed(GtkSpinButton* spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    if (n != editing_pat) {
        /* If we are in 'playing pattern' mode and asynchronous
	 * editing is disabled, make the audio thread jump to the new
	 * pattern, too. I think it would be cool to have this for
	 * 'playing song' mode, too, but then modifications in
	 * gui_update_player_pos() will be necessary. */
        if (gui_playing_mode == PLAYING_PATTERN && !ASYNCEDIT) {
            gui_mixer_set_pattern(n);
        } else
            gui_set_current_pattern(n, FALSE);
    }
}

static void
gui_patlen_changed(GtkSpinButton* spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);
    XMPattern* pat = &xm->patterns[editing_pat];

    if (n != pat->length) {
        history_log_spin_button(spin, _("Pattern length setting"),
            0, pat->length);
        st_set_pattern_length(pat, n);
        playlist_set_pattern_length(playlist, editing_pat, n);
        tracker_set_pattern(tracker, NULL);
        tracker_set_pattern(tracker, pat);
    }
}

static void
gui_numchans_changed(GtkSpinButton* spin)
{
    int n = gtk_spin_button_get_value_as_int(spin);

    if (n & 1) {
        gtk_spin_button_set_value(spin, --n);
        return;
    }

    if (xm->num_channels != n) {
        history_log_spin_button(spin, _("Number of channels setting"),
            0, xm->num_channels);
        gui_play_stop();
        tracker_set_pattern(tracker, NULL);
        st_set_num_channels(xm, n);
        gui_init_xm(0, FALSE);
    }
}

static void
gui_tempo_changed(GtkSpinButton *spin)
{
    gint value = gtk_spin_button_get_value_as_int(spin);

    history_log_spin_button(spin, _("Tempo setting"), 0, xm->tempo);
    xm->tempo = value;

    if (gui_playing_mode) {
        event_waiter_start(audio_tempo_ew);
    }
    audio_ctlpipe_write(AUDIO_CTLPIPE_SET_TEMPO, value);
}

static void
gui_bpm_changed(GtkSpinButton *spin)
{
    gint value = gtk_spin_button_get_value_as_int(spin);

    history_log_spin_button(spin, _("BPM setting"), 0, xm->bpm);
    xm->bpm = value;

    if (gui_playing_mode) {
        event_waiter_start(audio_bpm_ew);
    }
    audio_ctlpipe_write(AUDIO_CTLPIPE_SET_BPM, value);
}

static void
gui_adj_amplification_changed(GtkAdjustment* adj, GtkSpinButton* spin)
{
    gdouble b = 20.0 - gtk_adjustment_get_value(adj), c = powf(10.0, b / 20.0);

    g_signal_handler_block(G_OBJECT(spin), db_id);
    gtk_spin_button_set_value(spin, b);
    g_signal_handler_unblock(G_OBJECT(spin), db_id);

    audio_ctlpipe_write(AUDIO_CTLPIPE_SET_AMPLIFICATION, c);
}

static void
gui_adj_pitchbend_changed(GtkAdjustment* adj)
{
    gdouble b = adj->value;

    audio_ctlpipe_write(AUDIO_CTLPIPE_SET_PITCHBEND, b);
}

static void
gui_reset_pitch_bender(void)
{
    gtk_adjustment_set_value(adj_pitchbend, 0.0);
}

static void
looping_toggled(GtkToggleButton* tbn, GtkCheckMenuItem* menuitem)
{
    gui_play_stop();
    gui_settings.looped = gtk_toggle_button_get_active(tbn);
    if (looping_cross) {
        looping_cross = FALSE;
        gtk_check_menu_item_set_active(menuitem, gui_settings.looped);
        looping_cross = TRUE;
    }
}

void
gui_looping_toggled(GtkCheckMenuItem* menuitem)
{
    if (looping_cross) {
        looping_cross = FALSE;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(looping_toggle),
            gtk_check_menu_item_get_active(menuitem));
        looping_cross = TRUE;
    }
}

void
gui_toggle_masking(void)
{
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mask_toggle),
        !(gui_settings.cpt_mask & MASK_ENABLE));
}

static void
notebook_page_switched(GtkNotebook* notebook,
    gpointer page,
    int page_num)
{
    /* Sample editor may post its own status messages above
       ST global ones. So we restore the last message when 
       leaving the sample editor */
    if (notebook_current_page == NOTEBOOK_PAGE_SAMPLE_EDITOR)
        gui_statusbar_restore_base_message();

    notebook_prev_page = notebook_current_page;
    notebook_current_page = page_num;

    /* Sample editor can post its own status messages */
    if (page_num == NOTEBOOK_PAGE_SAMPLE_EDITOR)
        sample_editor_update_status();
}

/* gui_update_player_pos() is responsible for updating various GUI
   features according to the current player position. ProTracker and
   FastTracker scroll the patterns while the song is playing, and we
   need the time-buffer and event-waiter interfaces here for correct
   synchronization (we're called from
   track-editor.c::tracker_timeout() which hands us time-correct data)

   We have an ImpulseTracker-like editing mode as well ("asynchronous
   editing"), which disables the scrolling, but still updates the
   current song position spin buttons, for example. */

void gui_update_player_pos(const gdouble time,
    const gint songpos,
    const gint patno,
    const gint patpos,
    const gint tempo,
    const gint bpm)
{
    if (gui_playing_mode == PLAYING_NOTE)
        return;

    if (gui_playing_mode == PLAYING_SONG) {
        if (event_waiter_ready(audio_songpos_ew, time)) {
            /* The following check prevents excessive calls of set_position() */
            if (songpos != playlist_get_position(playlist)) {
                playlist_freeze_signals(playlist);
                playlist_set_position(playlist, songpos);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_songpos), songpos);
                playlist_thaw_signals(playlist);
            }
        }
        if (!ASYNCEDIT) {
            /* The following is a no-op if we're already in the right pattern */
            gui_set_current_pattern(xm->pattern_order_table[songpos], TRUE);
        }
    }

    if (!ASYNCEDIT) {
        if (gui_playing_mode == PLAYING_PATTERN)
            gui_set_current_pattern(patno, FALSE);
        track_editor_set_patpos(patpos);

        if (notebook_current_page == 0) {
            /* Prevent accumulation of X drawing primitives */
            gdk_flush();
        }
    }

    if (gui_settings.tempo_bpm_update) {
        if (event_waiter_ready(audio_tempo_ew, time)) {
            static gint old_tempo = 0;

            if (tempo != old_tempo) {
                g_signal_handler_block(G_OBJECT(tempo_spin), tempo_spin_id);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(tempo_spin), tempo);
                old_tempo = tempo;
                g_signal_handler_unblock(G_OBJECT(tempo_spin), tempo_spin_id);
            }
        }
        if (event_waiter_ready(audio_bpm_ew, time)) {
            static gint old_bpm = 0;

            if (bpm != old_bpm) {
                g_signal_handler_block(G_OBJECT(bpm_spin), bpm_spin_id);
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(bpm_spin), bpm);
                old_bpm = bpm;
                g_signal_handler_unblock(G_OBJECT(bpm_spin), bpm_spin_id);
            }
        }
    }
}

void gui_clipping_indicator_update(const gboolean status)
{
    static gboolean prev_status = FALSE;

    if (status != prev_status) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(gui_clipping_led),
            status ? led_clipping : led_normal);
        gtk_widget_queue_draw(gui_clipping_led);
        prev_status = status;
    }
}

static void
ch_status_foreach_func(gpointer data,
    gpointer user_data)
{
    audio_channel_status* p = data;
    gint newinst, newsmpl;

    switch (p->command) {
    case AUDIO_COMMAND_STOP_PLAYING:
        newinst = -1;
        newsmpl = -1;
        if (gui_playing_mode == PLAYING_PATTERN || gui_playing_mode == PLAYING_SONG)
            instrument_editor_indicate_key(p->channel, -1);
        break;
    case AUDIO_COMMAND_START_PLAYING:
        newinst = p->instr;
        newsmpl = p->sample;
        if (newinst == curinst + 1 &&
            (gui_playing_mode == PLAYING_PATTERN || gui_playing_mode == PLAYING_SONG))
            instrument_editor_indicate_key(p->channel, p->note - 1);
        break;
    default:
        return;
    }

    if (gui_settings.show_ins_smp)
        scope_group_update_channel_status(scopegroup, p->channel, newinst, newsmpl);
}

static gboolean
channel_status_timeout(gpointer data)
{
    gdouble time1 = current_driver->get_play_time(current_driver_object);

    time_buffer_foreach(audio_channels_status_tb, time1, ch_status_foreach_func, NULL);
    scope_group_timeout(scopegroup, time1);

    return TRUE;
}

static void
channel_status_start_updating(void)
{
    if (chan_status_timer != -1)
        return;

    chan_status_timer = g_timeout_add(1000 / gui_settings.scopes_update_freq,
        channel_status_timeout, NULL);
}

static void
channel_status_stop_updating(void)
{
    if (chan_status_timer == -1)
        return;

    g_source_remove(chan_status_timer);
    chan_status_timer = -1;
    scope_group_stop_updating(scopegroup);
}

void
gui_set_channel_status_update_freq(const gint freq)
{
    scope_group_set_update_freq(scopegroup, freq);

    if (chan_status_timer != -1) {
        channel_status_stop_updating();
        channel_status_start_updating();
    }
}

static gboolean
read_mixer_pipe(GIOChannel* source,
    GIOCondition condition,
    gpointer data)
{
    audio_backpipe_id a;
    int fd = g_io_channel_unix_get_fd(source);
    struct pollfd pfd = { fd, POLLIN, 0 };
    int x, err = 0;

    static char* msgbuf = NULL;
    static int msgbuflen = 0;
    static GtkWidget *dialog_err = NULL, *dialog_warn = NULL;

    while (poll(&pfd, 1, 0) > 0) {
        if (read(fd, &a, sizeof(a)) != sizeof(a)) {
            static GtkWidget* dialog = NULL;
            gui_error_dialog(&dialog, _("Connection with audio thread failed!"), FALSE);
        }

        switch (a) {
        case AUDIO_BACKPIPE_PLAYING_STOPPED:
            gui_statusbar_update(STATUS_IDLE, FALSE);
            clock_stop(CLOCK(st_clock));

            if (gui_ewc_startstop > 0) {
                /* can be equal to zero when the audio subsystem decides to stop playing on its own. */
                gui_ewc_startstop--;
            }
            gui_playing_mode = 0;
            channel_status_stop_updating();
            tracker_stop_updating();
            sample_editor_stop_updating();
            instrument_editor_indicate_key(-1, 0);
            gui_enable(1);
            break;

        case AUDIO_BACKPIPE_PLAYING_STARTED:
            gui_statusbar_update(STATUS_PLAYING_SONG, FALSE);
            /* fall through */

        case AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED:
            if (a == AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED)
                gui_statusbar_update(STATUS_PLAYING_PATTERN, FALSE);
            clock_set_seconds(CLOCK(st_clock), 0);
            clock_start(CLOCK(st_clock));

            gui_ewc_startstop--;
            gui_playing_mode = (a == AUDIO_BACKPIPE_PLAYING_STARTED) ? PLAYING_SONG : PLAYING_PATTERN;
            if (!ASYNCEDIT) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(editing_toggle), FALSE);
            }
            gui_enable(0);
            channel_status_start_updating();
            tracker_start_updating();
            sample_editor_start_updating();
            break;

        case AUDIO_BACKPIPE_PLAYING_NOTE_STARTED:
            gui_ewc_startstop--;
            if (!gui_playing_mode) {
                gui_playing_mode = PLAYING_NOTE;
                channel_status_start_updating();
                tracker_start_updating();
                sample_editor_start_updating();
            }
            break;

        case AUDIO_BACKPIPE_DRIVER_OPEN_FAILED:
            gui_ewc_startstop--;
            break;

        case AUDIO_BACKPIPE_ERRNO_MESSAGE:
            readpipe(fd, &x, sizeof(x));
            err = x;
            /* No break, we are falling through */
        case AUDIO_BACKPIPE_ERROR_MESSAGE:
        case AUDIO_BACKPIPE_WARNING_MESSAGE:
            gui_statusbar_update(STATUS_IDLE, FALSE);
            readpipe(fd, &x, sizeof(x));
            if (msgbuflen < x + 1) {
                g_free(msgbuf);
                msgbuf = g_new(char, x + 1);
                msgbuflen = x + 1;
            }
            readpipe(fd, msgbuf, x + 1);
            switch (a) {
            case AUDIO_BACKPIPE_ERROR_MESSAGE:
                gui_error_dialog(&dialog_err, msgbuf, TRUE);
                break;
            case AUDIO_BACKPIPE_WARNING_MESSAGE:
                gui_warning_dialog(&dialog_warn, msgbuf, TRUE);
                break;
            case AUDIO_BACKPIPE_ERRNO_MESSAGE:
                gui_errno_dialog(mainwindow, msgbuf, err);
                break;
            default:
                break;
            }
            break;

        default:
            fprintf(stderr, "\n\n*** read_mixer_pipe: unexpected backpipe id %d\n\n\n", a);
            g_assert_not_reached();
            break;
        }
    }
    return TRUE;
}

static void
wait_for_player(void)
{
    struct pollfd pfd = { audio_get_backpipe(), POLLIN, 0 };

    gui_ewc_startstop++;
    while (gui_ewc_startstop != 0) {
        g_return_if_fail(poll(&pfd, 1, -1) > 0);
        read_mixer_pipe(audio_backpipe_channel, G_IO_IN, NULL);
    }
}

static void play_song(gpointer data)
{
    const gboolean current = GPOINTER_TO_INT(data);
    int sp = playlist_get_position(playlist);
    int pp = current ? tracker->patpos : 0;

    g_assert(xm != NULL);

    gui_play_stop();
    playlist_enable(playlist, FALSE);
    audio_ctlpipe_write(AUDIO_CTLPIPE_PLAY_SONG, sp, pp, (gint)gui_settings.looped);
    wait_for_player();
}

static void
play_pattern(void)
{
    gui_play_stop();
    gui_mixer_play_pattern(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)), 0, 0, -1, -1, -1);
    wait_for_player();
}

void
gui_play_block(void)
{
    gint ch_start, row_start, n_ch, n_rows;

    gui_play_stop();
    tracker_get_selection_rect(tracker, &ch_start, &row_start, &n_ch, &n_rows);
    if (ch_start >= 0 && row_start >= 0) {
        gui_mixer_play_pattern(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)),
            row_start, 0, row_start + n_rows, ch_start, n_ch);
        wait_for_player();
    }
}

void
gui_play_current_pattern_row(void)
{
    gui_play_stop();
    gui_mixer_play_pattern(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)), tracker->patpos, 1, -1, -1, -1);
    gui_ewc_startstop++;
}

void gui_play_stop(void)
{
    gui_mixer_stop_playing();
    wait_for_player();
    gui_clipping_indicator_update(FALSE);
    playlist_enable(playlist, TRUE);
}

void gui_init_xm(int new_xm, gboolean updatechspin)
{
    audio_ctlpipe_write(AUDIO_CTLPIPE_INIT_PLAYER);
    tracker_reset(tracker);
    if (new_xm) {
        gui_playlist_initialize();
        editing_pat = -1;
        gui_set_current_pattern(xm->pattern_order_table[0], TRUE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(curins_spin), 1);
        current_instrument_changed(GTK_SPIN_BUTTON(curins_spin));
        modinfo_set_current_sample(0);
        modinfo_update_all();
    } else {
        gint i = editing_pat;
        editing_pat = -1;
        gui_set_current_pattern(i, TRUE);
    }
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tempo_spin), xm->tempo);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(bpm_spin), xm->bpm);
    track_editor_set_num_channels(xm->num_channels);
    if (updatechspin)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_numchans), xm->num_channels);
    scope_group_set_num_channels(scopegroup, xm->num_channels);
}

void gui_free_xm(void)
{
    gui_play_stop();
    instrument_editor_set_instrument(NULL, 0);
    sample_editor_set_sample(NULL);
    tracker_set_pattern(tracker, NULL);
    XM_Free(xm);
    xm = NULL;
}

void gui_new_xm(void)
{
    xm = XM_New();

    if (!xm) {
        g_critical("Whooops, having memory problems?");
    }
    gui_init_xm(1, TRUE);
    history_clear(FALSE);
}

static gboolean
gui_load_xm(const char* filename, const char* localname)
{
    gchar* newname = NULL;
    XM *new_xm = NULL;

    gui_statusbar_update(STATUS_LOADING_MODULE, TRUE);

    if (localname)
        new_xm = File_Load(localname, filename);
    else {
        newname = gui_filename_from_utf8(filename);
        if (newname)
            new_xm = File_Load(newname, filename);
    }

    if (!new_xm) {
        gui_statusbar_update(STATUS_IDLE, FALSE);
    } else {
        history_skip = TRUE;
        gui_free_xm();
        xm = new_xm;
        gui_init_xm(1, TRUE);
        gui_statusbar_update(STATUS_MODULE_LOADED, FALSE);
        gui_update_title(filename);
        gui_recent_add_item(localname ? localname : newname);
        history_skip = FALSE;
        history_clear(FALSE);
    }

    if (newname)
        g_free(newname);

    return (new_xm != NULL);
}

void gui_play_note(int channel,
    int note,
    gboolean all)
{
    int instrument = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));

    audio_ctlpipe_write(AUDIO_CTLPIPE_PLAY_NOTE, channel, note, instrument, (gint)all);
    gui_ewc_startstop++;
}

void gui_play_note_full(const guint channel,
    const guint note,
    STSample* sample,
    const guint32 offset,
    const guint32 playend,
    const gboolean all_channels,
    const gint ins,
    const gint smp)
{
    audio_ctlpipe_write(AUDIO_CTLPIPE_PLAY_NOTE_FULL,
        (gint)channel, (gint)note, sample, (gint)offset, (gint)playend, (gint)all_channels, ins, smp);
    gui_ewc_startstop++;
}

gboolean gui_play_note_no_repeat(guint32 keyval,
    gint modifiers,
    gboolean pressed,
    STSample* sample,
    gint start, gint playend,
    gint channel,
    gboolean full_stop,
    gboolean* is_note,
    gboolean all_channels,
    const gint ins,
    const gint smp)
{
    gboolean handled = FALSE;
    static gint playing = -1;
    gint i = keys_get_key_meaning(keyval, modifiers, -1);

    if (i != -1 && KEYS_MEANING_TYPE(i) == KEYS_MEANING_NOTE) {
        i += 12 * gui_get_current_octave_value() + 1;
        if (!pressed) {
            /* Autorepeat fake keyoff */
            if (keys_is_key_pressed(keyval, modifiers))
                return FALSE;

            if (playing == i) {
                full_stop ? gui_play_stop() : gui_stop_note(channel);
                playing = -1;
                handled = TRUE;
            }
        } else
            if (i < NUM_NOTES && sample != NULL && playing != i) {
                playing = i;
                gui_play_note_full(channel, i, sample, start, playend, all_channels, ins, smp);
                handled = TRUE;
            }
        if (is_note)
            *is_note = TRUE;
    }

    return handled;
}

void gui_play_note_keyoff(int channel)
{
    audio_ctlpipe_write(AUDIO_CTLPIPE_PLAY_NOTE_KEYOFF, channel);
}

void gui_stop_note(int channel)
{
    audio_ctlpipe_write(AUDIO_CTLPIPE_STOP_NOTE, channel);
}

static void
gui_enable(int enable)
{
    if (!ASYNCEDIT) {
        gtk_widget_set_sensitive(vscrollbar, enable);
        gtk_widget_set_sensitive(spin_patlen, enable);
    }
}

void gui_set_current_pattern(int p, gboolean updatespin)
{
    if (editing_pat == p)
        return;

    editing_pat = p;
    tracker_set_pattern(tracker, &xm->patterns[p]);
    if (updatespin)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_editpat), p);
    gui_update_pattern_data();
}

void gui_update_pattern_data(void)
{
    int p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat));

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_patlen), xm->patterns[p].length);
}

int gui_get_current_pattern(void)
{
    return editing_pat;
}

static void
offset_current_pattern(int offset)
{
    int nv;

    nv = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_editpat)) + offset;
    if (nv < 0)
        nv = 0;
    else if (nv > 255)
        nv = 255;

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_editpat), nv);
}

void gui_block_smplname_entry(gboolean block)
{
    block ? g_signal_handler_block(G_OBJECT(gui_cursmpl_name), snch_id)
          : g_signal_handler_unblock(G_OBJECT(gui_cursmpl_name), snch_id);
}

void gui_block_instrname_entry(gboolean block)
{
    block ? g_signal_handler_block(G_OBJECT(gui_curins_name), inch_id)
          : g_signal_handler_unblock(G_OBJECT(gui_curins_name), inch_id);
}

void gui_set_current_instrument(int n)
{
    GtkWidget* focus;

    g_return_if_fail(n >= 1 && n <= XM_NUM_INSTRUMENTS);
    if (n != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin))) {
        focus = GTK_WINDOW(mainwindow)->focus_widget; /* gtk_spin_button_set_value changes focus widget */
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(curins_spin), n);
        gtk_window_set_focus(GTK_WINDOW(mainwindow), focus);
    }
}

static gboolean find_instr(XMPattern* p,
    gint* startpos,
    gint* startch,
    gint instr)
{
    gint pos, ch = *startch;

    for (pos = *startpos; pos < p->length; pos++) {
        for (; ch < tracker->num_channels; ch++)
            if (p->channels[ch][pos].instrument == instr) {
                *startpos = pos;
                *startch = ch;
                return TRUE;
            }
        ch = 0;
    }

    return FALSE;
}

void gui_find_instrument(void)
{
    gint curins = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
    gint pos = tracker->patpos;
    gint ch = tracker->cursor_ch;
    gint pat = editing_pat;
    gint ord = playlist_get_position(playlist);
    gint stopord = xm->song_length;
    gint i;

    static GtkWidget* dialog = NULL;

    /* Step the position by one with possible wrap */
    if (++ch >= tracker->num_channels) {
        ch = 0;
        if (++pos >= xm->patterns[pat].length) {
            pos = 0;
            if (++ord >= stopord) /* Here stopord == length */
                ord = 0;
            pat = playlist_get_nth_pattern(playlist, ord);
        }
    }

    for (i = 0; i < 2; i++) {
        for (;;) {
            if (find_instr(&xm->patterns[pat], &pos, &ch, curins)) {
                track_editor_set_patpos(pos);
                tracker_set_cursor_channel(tracker, ch);
                if (ord != playlist_get_position(playlist))
                    playlist_set_position(playlist, ord);
                return;
            }
            if (++ord >= stopord)
                break;
            pat = playlist_get_nth_pattern(playlist, ord);
            ch = 0;
            pos = 0;
        }

        if (!gui_ok_cancel_modal(mainwindow,
            _("Instrument is not found. Start search from the beginning?")))
            return;
        ord = 0;
        pat = playlist_get_nth_pattern(playlist, ord);
        /* We allow also search in the current pattern. The right choice would be looking for
           only in the part before cursor, but using the whole pattern will not lead to
           notable overhead */
        stopord = playlist_get_position(playlist) + 1;
    }

    gui_info_dialog(&dialog, _("The current instrument is not used in the module."), FALSE);
}

void gui_set_current_sample(int n)
{
    GtkWidget* focus;

    g_return_if_fail(n >= 0 && n <= 127);
    if (n != gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin))) {
        focus = GTK_WINDOW(mainwindow)->focus_widget; /* gtk_spin_button_set_value changes focus widget */
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(cursmpl_spin), n);
        gtk_window_set_focus(GTK_WINDOW(mainwindow), focus);
    }
}

void gui_set_current_position(int n)
{
    g_return_if_fail(n >= 0 && n < xm->song_length);
    playlist_set_position(playlist, n);
}

int gui_get_current_sample(void)
{
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin));
}

int gui_get_current_octave_value(void)
{
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_octave));
}

int gui_get_current_position(void)
{
    return playlist_get_position(playlist);
}

int gui_get_current_jump_value(void)
{
    if (!GUI_ENABLED && !ASYNCEDIT)
        return 0;
    else
        return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_jump));
}

void gui_set_jump_value(int value)
{
    if (GUI_ENABLED || ASYNCEDIT) {
        g_return_if_fail(value >= 0 && value <= 16);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_jump), value);
    }
}

int gui_get_current_instrument(void)
{
    return gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
}

void gui_offset_current_instrument(int offset)
{
    int nv, v;

    v = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(curins_spin));
    nv = v + offset;

    if (nv < 1)
        nv = 1;
    else if (nv > XM_NUM_INSTRUMENTS)
        nv = XM_NUM_INSTRUMENTS;

    gui_set_current_instrument(nv);
}

void gui_offset_current_sample(int offset)
{
    int nv, v;

    v = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(cursmpl_spin));
    nv = v + offset;

    if (nv < 0)
        nv = 0;
    else if (nv > 127)
        nv = 127;

    gui_set_current_sample(nv);
}

static void
gui_insert_pattern(Playlist* p, const gint pos)
{
    playlist_insert_pattern(p, pos, editing_pat);
}

static void
gui_add_free_pattern(GtkWidget* w, Playlist* p)
{
    playlist_add_pattern(p, -1);
}

static void
gui_add_free_pattern_and_copy(Playlist* p)
{
    playlist_add_pattern(p, editing_pat);
}

void gui_playlist_initialize(void)
{
    playlist_set_xm(playlist, xm);
}

void gui_auto_switch_page(void)
{
    if (gui_settings.auto_switch)
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),
            1);
}

static gint
gui_splash_logo_expose(GtkWidget* widget,
    GdkEventExpose* event,
    gpointer data)
{
    cairo_t* cr = gdk_cairo_create(widget->window);
    cairo_set_source_surface(cr, gui_splash_logo, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);

    return TRUE;
}

static void
gui_splash_close(void)
{
    gtk_widget_destroy(gui_splash_window);
    gui_splash_window = NULL;
}

static void
gui_splash_set_label(const gchar* text,
    gboolean update)
{
    char buf[256];

    strcpy(buf, PACKAGE_NAME " v" VERSION " - ");
    strncat(buf, text, 255 - strlen(buf));

    gtk_label_set_text(GTK_LABEL(gui_splash_label), buf);

    while (update && gtk_events_pending()) {
        gtk_main_iteration();
    }
}

void
gui_toggle_fullscreen(GtkCheckMenuItem* cmi)
{
    gboolean is_fullscreen = gtk_check_menu_item_get_active(cmi);

    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), !is_fullscreen);
    (is_fullscreen ? gdk_window_fullscreen : gdk_window_unfullscreen)(gtk_widget_get_window(mainwindow));
    (is_fullscreen ? gtk_widget_hide : gtk_widget_show)(mainwindow_upper_hbox);

    (is_fullscreen ? gtk_widget_hide : gtk_widget_show)(editing_toggle);
    (is_fullscreen ? gtk_widget_hide : gtk_widget_show)(label_octave);
    (is_fullscreen ? gtk_widget_hide : gtk_widget_show)(spin_octave);
    (is_fullscreen ? gtk_widget_hide : gtk_widget_show)(label_jump);
    (is_fullscreen ? gtk_widget_hide : gtk_widget_show)(spin_jump);

    (is_fullscreen ? gtk_widget_show : gtk_widget_hide)(label_songpos);
    (is_fullscreen ? gtk_widget_show : gtk_widget_hide)(spin_songpos);
    (is_fullscreen ? gtk_widget_show : gtk_widget_hide)(label_pat);
    (is_fullscreen ? gtk_widget_show : gtk_widget_hide)(spin_pat);
}

int gui_splash(void)
{
    GtkWidget *vbox, *thing;
    GtkWidget *logo_area, *frame;
    gchar* buf;

    gui_splash_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    buf = g_strdup_printf(_("%s Startup"), PACKAGE_NAME);
    gtk_window_set_title(GTK_WINDOW(gui_splash_window), buf);
    g_free(buf);
    gtk_window_set_position(GTK_WINDOW(gui_splash_window), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(gui_splash_window), FALSE);
    gtk_window_set_modal(GTK_WINDOW(gui_splash_window), TRUE);

    g_signal_connect(gui_splash_window, "delete_event",
        G_CALLBACK(gui_splash_close), NULL);

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_container_add(GTK_CONTAINER(gui_splash_window), vbox);

    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

    /* Show splash screen if enabled and image available. */

    gui_splash_logo = cairo_image_surface_create_from_png(DATADIR "/" PACKAGE "/soundtracker_splash.png");
    if (gui_splash_logo) {
        thing = gtk_hseparator_new();
        gtk_widget_show(thing);
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

        thing = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
        gtk_widget_show(thing);

        frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
        gtk_container_add(GTK_CONTAINER(thing), frame);
        gtk_widget_show(frame);

        gui_splash_logo_area = logo_area = gtk_drawing_area_new();
        gtk_container_add(GTK_CONTAINER(frame), logo_area);
        gtk_widget_show(logo_area);

        g_signal_connect(logo_area, "expose_event",
            G_CALLBACK(gui_splash_logo_expose),
            NULL);

        gtk_widget_set_size_request(logo_area,
            cairo_image_surface_get_width(gui_splash_logo),
            cairo_image_surface_get_height(gui_splash_logo));
    }

    /* Show version number. */

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

    gui_splash_label = thing = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    gui_splash_set_label(_("Loading..."), FALSE);

    /* Show tips if enabled. */

    if (tips_dialog_show_tips) {
        thing = gtk_hseparator_new();
        gtk_widget_show(thing);
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

        tips_box_populate(vbox, FALSE);
    }

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

    if (gui_splash_logo || tips_dialog_show_tips) {
        gui_splash_close_button = thing = gtk_button_new_with_label(_("Use SoundTracker!"));
        gtk_widget_show(thing);
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
        g_signal_connect(thing, "clicked",
            (gui_splash_close), NULL);
        gtk_widget_set_sensitive(thing, FALSE);
    }

    gtk_widget_show(vbox);
    gtk_widget_show(gui_splash_window);

    gui_splash_set_label(_("Loading..."), TRUE);

    return 1;
}

gboolean
quit_requested(void)
{
    if (history_get_modified()) {
        if (gui_ok_cancel_modal(mainwindow,
                _("Are you sure you want to quit?\nAll changes will be lost!")))
            gtk_main_quit();
    } else {
        gtk_main_quit();
    }
    return TRUE;
}

static gboolean
is_sep(GtkTreeModel* model, GtkTreeIter* iter, gpointer data)
{
    GtkTreePath* path = gtk_tree_model_get_path(model, iter);
    gint index = GPOINTER_TO_INT(data);
    gint* indices = gtk_tree_path_get_indices(path);
    gint curindex = indices[0];

    gtk_tree_path_free(path);
    return curindex == index;
}

static void
recent_selected(GtkRecentChooser *rcc)
{
    gchar* uri = gtk_recent_chooser_get_current_uri(rcc);
    gchar* fname = g_filename_from_uri(uri, NULL, NULL);

    if (fname) {
        gchar* utfname = gui_filename_to_utf8(fname);

        if (utfname) {
            if (!load_xm_full(utfname, fname))
                gtk_recent_manager_remove_item(rcmgr, uri, NULL);
            g_free(utfname);
        }
    }

    g_free(uri);
    g_free(fname);
}

static void scroll_songpos(GtkWidget* w, const gint direction)
{
    gint currpos = CLAMP(playlist_get_position(playlist) + direction, 0, xm->song_length - 1);

    playlist_set_position(playlist, currpos);
}

void gui_amp_estimate(void)
{
    static GtkWidget* est_dialog = NULL;
    gint response;

    if (!est_dialog) {
        GtkWidget *vbox, *thing;

        est_dialog = gtk_dialog_new_with_buttons(_("Amplification estimation"), GTK_WINDOW(mainwindow),
            GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(est_dialog, GTK_RESPONSE_OK);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(est_dialog));
        thing = gtk_label_new(_("You are about to start the procedure for automatic amplification estimation.\n"
            "This may take some time."));
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
        gtk_widget_show(thing);

        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 4);
        gtk_widget_show_all(vbox);
    }

    response = gtk_dialog_run(GTK_DIALOG(est_dialog));
    gtk_widget_hide(est_dialog);

    if (response == GTK_RESPONSE_OK) {
        gint rate = audio_get_playback_rate();

        if (rate != -1) {
#define EST_BUFSIZE 65536
#ifdef WORDS_BIGENDIAN
            const gint format = ST_MIXER_FORMAT_S16_BE | ST_MIXER_FORMAT_STEREO;
#else
            const gint format = ST_MIXER_FORMAT_S16_LE | ST_MIXER_FORMAT_STEREO;
#endif
            const guint32 num_samples =
                (EST_BUFSIZE / mixer_get_resolution(format)) >> mixer_is_format_stereo(format);
            guint32 num_rendered;
            gint16* est_buf = malloc(EST_BUFSIZE);
            gboolean clipping;
            gint16 min_val, max_val;
            gfloat amp = 1.0;
            GtkWidget* pw;

            pw = show_process_window(N_("Estimating..."));
            gui_play_stop();
            do {
                /* We start estimation from the normal amplification level */
                audio_set_amplification(amp);
                audio_prepare_for_rendering(AUDIO_RENDER_SONG, 0, 0, 0, 0, 0);

                min_val = max_val = 0;
                clipping = FALSE;
                do {
                    guint i;

                    num_rendered = audio_mix(est_buf, num_samples, rate, format, FALSE, &clipping);
                    for(i = 0; i < EST_BUFSIZE / sizeof(gint16); i++) {
                        if (est_buf[i] > max_val)
                            max_val = est_buf[i];
                        if (est_buf[i] < min_val)
                            min_val = est_buf[i];
                    }
                    while (gtk_events_pending())
                        gtk_main_iteration();
                } while (num_rendered == num_samples && !clipping && !stop_process);

                audio_cleanup_after_rendering();
                /* Clipping occured, decrease amplification and make the next attempt */
                if (clipping)
                    amp /= 2.0;
            } while (clipping && !stop_process);
            gtk_widget_hide(pw);

            if (stop_process) /* restoring the old value on explicit stop */
                audio_set_amplification(powf(10.0, (20.0 - gtk_adjustment_get_value(adj_amplification)) / 20.0));
            else {
                max_val = MAX(abs(min_val), abs(max_val));
                if (max_val == 0) /* Nothing to do for a silent module, restoring the old value */
                    audio_set_amplification(powf(10.0, (20.0 - gtk_adjustment_get_value(adj_amplification)) / 20.0));
                else {
                    /* 0.9 to be at the safe side */
                    amp = CLAMP(amp * 0.9 * 32768.0 / max_val, 0.01, 10.0);
                    gtk_adjustment_set_value(adj_amplification, 20.0 * (1.0 - log10f(amp)));
                }
            }
            g_free(est_buf);
        }
    }
}

static void mask_changed(GtkToggleButton* btn, gpointer data)
{
    guint mask = GPOINTER_TO_INT(data);

    if (gtk_toggle_button_get_active(btn))
        gui_settings.cpt_mask |= mask;
    else
        gui_settings.cpt_mask &= ~mask;
}

static void mask_toggled(GtkToggleButton* btn)
{
    if (gtk_toggle_button_get_active(btn))
        gui_settings.cpt_mask |= MASK_ENABLE;
    else
        gui_settings.cpt_mask &= ~MASK_ENABLE;
}

static void col_toggled(GtkToggleButton* btn,
    GtkToggleButton** w)
{
    guint i;
    gboolean state = gtk_toggle_button_get_active(btn);

    for (i = 0; i < MASK_ROWS; i++)
        gtk_toggle_button_set_active(w[i], state);
}

void gui_show_mask_window(void)
{
    static GtkWidget* dialog = NULL;
    static GtkWidget* cbutton[MASK_COLS][MASK_ROWS];

    if (!dialog) {
        GtkWidget *vbox, *table, *thing;
        guint i, j;
        const gchar* labels[] = {
            N_("Note"), N_("Instrument"), N_("Volume"), N_("Eff type"), N_("Eff param")
        };
        const gchar* col_labels[] = {
            N_("Cutting"), N_("Copying"), N_("Pasting"), N_("Transp")
        };

        dialog = gtk_dialog_new_with_buttons(_("Masking"), GTK_WINDOW(mainwindow),
            GTK_DIALOG_MODAL, GTK_STOCK_CLOSE, 0, NULL);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        table = gtk_table_new(MASK_ROWS + 1, MASK_COLS, TRUE);
        gtk_table_set_row_spacings(GTK_TABLE(table), 2);
        gtk_table_set_col_spacings(GTK_TABLE(table), 4);
        gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

        for (i = 0; i < MASK_ROWS; i++) {
            thing = gtk_label_new(_(labels[i]));
            gtk_misc_set_alignment(GTK_MISC(thing), 0.0, 0.0);
            gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, i + 1, i + 2);
        }

        for (i = 0; i < MASK_COLS; i++) {
            GtkWidget* tbutton = gtk_toggle_button_new_with_label(_(col_labels[i]));

            gtk_table_attach_defaults(GTK_TABLE(table), tbutton, i + 1, i + 2, 0, 1);

            for (j = 0; j < MASK_ROWS; j++) {
                GtkWidget* alignment;

                cbutton[i][j] = thing = gtk_check_button_new();
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing),
                    gui_settings.cpt_mask & cpt_masks[j + i * MASK_ROWS]);
                alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
                gtk_container_add(GTK_CONTAINER(alignment), thing);
                gtk_table_attach_defaults(GTK_TABLE(table), alignment, i + 1, i + 2, j + 1, j + 2);
                g_signal_connect(thing, "toggled",
                    G_CALLBACK(mask_changed), GINT_TO_POINTER(cpt_masks[j + i * MASK_ROWS]));
            }
            g_signal_connect(tbutton, "toggled",
                G_CALLBACK(col_toggled), cbutton[i]);
        }
        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 4);

        gtk_widget_show_all(vbox);
    }
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(dialog);
}

static gboolean mask_brelease(GtkWidget* w, GdkEventButton* event)
{
    if (event->button != 3)
        return FALSE;

    gui_show_mask_window();
    return TRUE;
}

static void
spin_songpos_changed(GtkSpinButton* sb, Playlist* p)
{
    playlist_set_position(p, gtk_spin_button_get_value_as_int(sb));
}

int gui_final(int argc, char* argv[])
{
    GtkWidget *thing, *mainvbox, *controlbox, *hbox, *button, *mainvbox0, *pmw, *vbox, *recent;
    gint i, selected;
    GError* error = NULL;
    GtkListStore* ls;
    GtkTreeIter iter;
    GtkRecentFilter* rcfilter;
    static gchar buf[128];

    static const gchar* xm_f[] = { N_("FastTracker modules (*.xm)"), "*.[xX][mM]", NULL };
    static const gchar* mod_f[] = { N_("Original SoundTracker modules (*.mod)"), "*.[mM][oO][dD]", NULL };
    static const gchar** mod_formats[] = { xm_f, mod_f, NULL };
    static const gchar* save_mod_f[] = { N_("FastTracker modules (*.xm)"), "*.[xX][mM]", NULL };
    static const gchar** save_mod_formats[] = { save_mod_f, NULL };
#if USE_SNDFILE || AUDIOFILE_VERSION
    static const gchar* wav_f[] = { N_("Microsoft RIFF (*.wav)"), "*.[wW][aA][vV]", NULL };
    static const gchar** wav_formats[] = { wav_f, NULL };
#endif
    static const gchar* xp_f[] = { N_("Extended pattern (*.xp)"), "*.[xX][pP]", NULL };
    static const gchar** xp_formats[] = { xp_f, NULL };

    const icon_set gui_icons[] =
         {{"st-history", "history", SIZES_MENU},
         {NULL}};

    audio_backpipe_channel = g_io_channel_unix_new(audio_get_backpipe());
    g_io_add_watch(audio_backpipe_channel, G_IO_IN, read_mixer_pipe, NULL);

    gui_builder = gtk_builder_new();
    if (!gtk_builder_add_from_file(gui_builder, UI_FILE, &error)) {
        g_critical(_("%s.\n%s startup is aborted\nFailed GUI description file: %s\n"),
            error->message, PACKAGE, UI_FILE);
        g_error_free(error);
        return 0;
    }

    gui_add_icons(gui_icons);

    mainwindow = gui_get_widget(gui_builder, "mainwindow", UI_FILE);
    if (!mainwindow)
        return 0;
    gtk_window_set_title(GTK_WINDOW(mainwindow), PACKAGE_NAME " " VERSION);

    if (gui_splash_window) {
        gtk_window_set_transient_for(GTK_WINDOW(gui_splash_window),
            GTK_WINDOW(mainwindow));
    }

    if (gui_settings.st_window_x != -666) {
        gtk_window_set_default_size(GTK_WINDOW(mainwindow),
            gui_settings.st_window_w,
            gui_settings.st_window_h);
        gtk_window_move(GTK_WINDOW(mainwindow),
            gui_settings.st_window_x,
            gui_settings.st_window_y);
    }

    loadmod = fileops_dialog_create(DIALOG_LOAD_MOD, _("Load Module"), &gui_settings.loadmod_path, load_xm, TRUE, FALSE, mod_formats, N_("Load the selected module into the tracker"), NULL);
    savemod = fileops_dialog_create(DIALOG_SAVE_MOD, _("Save Module"), &gui_settings.savemod_path, save_song, TRUE, TRUE, save_mod_formats, N_("Save the current module"), "xm");
#if USE_SNDFILE || AUDIOFILE_VERSION
    renderwav = fileops_dialog_create(DIALOG_SAVE_MOD_AS_WAV, _("Render WAV"), &gui_settings.savemodaswav_path, save_wav, TRUE, TRUE, wav_formats, N_("Render the current module as WAV file"), "wav");
#endif
    savexm = fileops_dialog_create(DIALOG_SAVE_SONG_AS_XM, _("Save XM without samples..."), &gui_settings.savesongasxm_path, save_xm, FALSE, TRUE, save_mod_formats, NULL, "xm");
    loadpat = fileops_dialog_create(DIALOG_LOAD_PATTERN, _("Load current pattern..."), &gui_settings.loadpat_path, load_pat, FALSE, FALSE, xp_formats, NULL, NULL);
    savepat = fileops_dialog_create(DIALOG_SAVE_PATTERN, _("Save current pattern..."), &gui_settings.savepat_path, save_pat, FALSE, TRUE, xp_formats, NULL, "xp");

    mainvbox0 = gui_get_widget(gui_builder, "mainvbox0", UI_FILE);
    if (!mainvbox0)
        return 0;

    mainvbox = gtk_vbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(mainvbox), 4);
    gtk_box_pack_start(GTK_BOX(mainvbox0), mainvbox, TRUE, TRUE, 0);
    gtk_widget_show(mainvbox);

    /* The upper part of the window */
    mainwindow_upper_hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainvbox), mainwindow_upper_hbox, FALSE, TRUE, 0);
    gtk_widget_show(mainwindow_upper_hbox);

    /* Program List */
    thing = playlist_new(mainwindow);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    playlist = PLAYLIST(thing);
    g_signal_connect(playlist, "current_position_changed",
        G_CALLBACK(gui_playlist_position_changed), NULL);
    g_signal_connect(playlist, "entry_changed",
        G_CALLBACK(gui_playlist_entry_changed), NULL);
    g_signal_connect(playlist, "song_length_changed",
        G_CALLBACK(gui_playlist_songlength_changed), NULL);
    g_signal_connect(playlist, "insert_pattern",
        G_CALLBACK(gui_insert_pattern), NULL);
    g_signal_connect(playlist, "add_copy",
        G_CALLBACK(gui_add_free_pattern_and_copy), NULL);
    playlist_enable(playlist, TRUE);

    thing = gui_get_widget(gui_builder, "add_free", UI_FILE);
    g_signal_connect(thing, "activate",
        G_CALLBACK(gui_add_free_pattern), playlist);
    thing = gui_get_widget(gui_builder, "add_copy", UI_FILE);
    g_signal_connect_swapped(thing, "activate",
        G_CALLBACK(gui_add_free_pattern_and_copy), playlist);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    /* Basic editing commands and properties */

    controlbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), controlbox, FALSE, TRUE, 0);
    gtk_widget_show(controlbox);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(controlbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    pmw = gtk_image_new_from_file(pixmaps[GUI_PIXMAP_PLAY]);
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(play_song), GINT_TO_POINTER(FALSE));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Play Song"));
    gtk_widget_show_all(thing);

    pmw = gtk_image_new_from_file(pixmaps[GUI_PIXMAP_PLAY_CUR]);
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(play_pattern), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Play Pattern"));
    gtk_widget_show_all(thing);

    pmw = gtk_image_new_from_file(pixmaps[GUI_PIXMAP_PLAY_FROM]);
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(play_song), GINT_TO_POINTER(TRUE));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Play From Cursor"));
    gtk_widget_show_all(thing);

    pmw = gtk_image_new_from_file(pixmaps[GUI_PIXMAP_PLAY_BLOCK]);
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(gui_play_block), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Play Block"));
    gtk_widget_show_all(thing);

    pmw = gtk_image_new_from_file(pixmaps[GUI_PIXMAP_STOP]);
    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(gui_play_stop), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Stop"));
    gtk_widget_show_all(thing);

    pmw = gtk_image_new_from_file(pixmaps[GUI_PIXMAP_LOOP]);
    looping_toggle = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(looping_toggle), pmw);
    thing = gui_get_widget(gui_builder, "settings_loop_playback", UI_FILE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(looping_toggle), gui_settings.looped);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(thing), gui_settings.looped);
    gtk_box_pack_end(GTK_BOX(hbox), looping_toggle, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(looping_toggle, _("Loop Playback"));
    gtk_widget_show_all(looping_toggle);
    g_signal_connect(looping_toggle, "toggled",
        G_CALLBACK(looping_toggled), thing);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(controlbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_label_new(_("Pat"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    spin_editpat = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 255, 1.0, 10.0, 0.0)), 1, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_editpat));
    gtk_widget_set_tooltip_text(spin_editpat, _("Edited pattern"));
    gtk_box_pack_start(GTK_BOX(hbox), spin_editpat, FALSE, TRUE, 0);
    gtk_widget_show(spin_editpat);
    g_signal_connect(spin_editpat, "value-changed",
        G_CALLBACK(gui_editpat_changed), NULL);

    pmw = gtk_image_new_from_file(pixmaps[GUI_PIXMAP_LOCK]);
    toggle_lock_editpat = thing = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(thing), pmw);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("When enabled, browsing the playlist does not change the edited pattern."));
    gtk_widget_show_all(thing);

    spin_patlen = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(64, 1, 256, 1.0, 16.0, 0.0)), 1, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_patlen));
    gtk_widget_set_tooltip_text(spin_patlen, _("Pattern Length"));
    gtk_box_pack_end(GTK_BOX(hbox), spin_patlen, FALSE, TRUE, 0);
    g_signal_connect(spin_patlen, "value-changed",
        G_CALLBACK(gui_patlen_changed), NULL);
    gtk_widget_show(spin_patlen);

    thing = gtk_label_new(_("Length"));
    gtk_box_pack_end(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(controlbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_label_new(_("Number of Channels:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    spin_numchans = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(8, 2, 32, 2.0, 8.0, 0.0)), 1, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_numchans));
    gtk_box_pack_end(GTK_BOX(hbox), spin_numchans, FALSE, TRUE, 0);
    g_signal_connect(spin_numchans, "value-changed",
        G_CALLBACK(gui_numchans_changed), NULL);
    gtk_widget_show(spin_numchans);

    hbox = gtk_hbox_new(FALSE, 4);
    thing = gui_labelled_spin_button_new(_("Tempo"), 1, 31, &tempo_spin, gui_tempo_changed, NULL, TRUE, &tempo_spin_id);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);

    thing = gui_labelled_spin_button_new("BPM", 32, 255, &bpm_spin, gui_bpm_changed, NULL, TRUE, &bpm_spin_id);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);

    pmw = gtk_image_new_from_file(pixmaps[GUI_PIXMAP_MASK]);
    mask_toggle = thing = gtk_toggle_button_new();
    gtk_container_add(GTK_CONTAINER(mask_toggle), pmw);
    gtk_widget_set_tooltip_text(thing, _("Left click to enable / disable masking, right click to configure"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing),
        gui_settings.cpt_mask & MASK_ENABLE);
    g_signal_connect(thing, "button-release-event",
        G_CALLBACK(mask_brelease), NULL);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(mask_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controlbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show_all(hbox);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(controlbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    vbox = gtk_vbox_new(FALSE, 0);
    alt[0] = gtk_image_new_from_file(DATADIR "/" PACKAGE "/sharp.xpm");
    gtk_box_pack_start(GTK_BOX(vbox), alt[0], FALSE, FALSE, 0);

    alt[1] = gtk_image_new_from_file(DATADIR "/" PACKAGE "/flat.xpm");
    gtk_widget_show(alt[gui_settings.sharp ? 0 : 1]);
    gtk_box_pack_start(GTK_BOX(vbox), alt[1], FALSE, FALSE, 0);
    gtk_widget_show(vbox);

    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), vbox);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Set preferred accidental type"));
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(gui_accidentals_clicked), NULL);

    add_empty_hbox(hbox);
    thing = gtk_toggle_button_new_with_label(_("Measure"));
    gtk_widget_set_tooltip_text(thing, _("Enable row highlighting"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.highlight_rows);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(gui_highlight_rows_toggled), NULL);
    gtk_widget_show(thing);

    selected = -1;
    ls = gtk_list_store_new(1, G_TYPE_STRING);

    for (i = 0; measure_msr[i].title != NULL; i++) {
        gtk_list_store_append(ls, &iter);
        gtk_list_store_set(ls, &iter, 0, measure_msr[i].title, -1);
        if ((measure_msr[i].major == gui_settings.highlight_rows_n) && (measure_msr[i].minor == gui_settings.highlight_rows_minor_n))
            selected = i;
    }
    if (selected == -1)
        selected = i + 1;
    gtk_list_store_append(ls, &iter); /* separator */
    gtk_list_store_set(ls, &iter, 0, "", -1);
    gtk_list_store_append(ls, &iter);
    gtk_list_store_set(ls, &iter, 0, _("Other..."), -1);

    thing = gui_combo_new(ls);
    gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(thing), is_sep, GINT_TO_POINTER(i), NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(thing), selected);

    gtk_widget_set_tooltip_text(thing, _("Row highlighting configuration"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    g_signal_connect(thing, "changed", G_CALLBACK(measure_changed), GINT_TO_POINTER(i));
    g_signal_connect(thing, "notify::popup-shown", G_CALLBACK(popwin_hide), GINT_TO_POINTER(i));

    add_empty_hbox(hbox);

    vbox = gtk_vbox_new(FALSE, 0);
    arrow[0] = gtk_image_new_from_file(DATADIR "/" PACKAGE "/downarrow.xpm");
    gtk_box_pack_start(GTK_BOX(vbox), arrow[0], FALSE, FALSE, 0);

    arrow[1] = gtk_image_new_from_file(DATADIR "/" PACKAGE "/rightarrow.xpm");
    gtk_box_pack_start(GTK_BOX(vbox), arrow[1], FALSE, FALSE, 0);
    gtk_widget_show(arrow[gui_settings.advance_cursor_in_fx_columns ? 1 : 0]);
    gtk_widget_show(vbox);

    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), vbox);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Change effect column editing direction"));
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(gui_direction_clicked), NULL);

    /* Scopes Group or Instrument / Sample Listing */

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    scopegroup = SCOPE_GROUP(scope_group_new(gui_pixbuf_new_from_file(DATADIR "/" PACKAGE "/muted.png"),
        gui_pixbuf_new_from_file(DATADIR "/" PACKAGE "/unmuted.png")));
    gtk_widget_show(GTK_WIDGET(scopegroup));
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), GTK_WIDGET(scopegroup), TRUE, TRUE, 0);
    gui_set_channel_status_update_freq(gui_settings.scopes_update_freq);

    /* Amplification and Pitchbender */

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    hbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), hbox, FALSE, TRUE, 0);

    adj_amplification = GTK_ADJUSTMENT(gtk_adjustment_new(20.0, 0.0, 61.0, 1.0, 1.0, 1.0));
    thing = gtk_vscale_new(adj_amplification);
    gtk_widget_set_tooltip_text(thing, _("Global amplification"));
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);

    button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, TRUE, 0);
    gtk_widget_show(button);
    g_signal_connect(button, "clicked", G_CALLBACK(gui_amp_estimate), NULL);
    gtk_widget_set_tooltip_text(button, _("Estimate the best amplification by pressing on the clipping indicator"));

    led_normal = gui_pixbuf_new_from_file(DATADIR "/" PACKAGE "/led_off.xpm");
    led_clipping = gui_pixbuf_new_from_file(DATADIR "/" PACKAGE "/led_on.xpm");
    gui_clipping_led = thing = gtk_image_new_from_pixbuf(led_normal);
    gtk_container_add(GTK_CONTAINER(button), thing);
    gtk_widget_show(thing);

    hbox = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(mainwindow_upper_hbox), hbox, FALSE, TRUE, 0);

    adj_pitchbend = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, -20.0, +20.0, 1, 1, 1));
    thing = gtk_vscale_new(adj_pitchbend);
    gtk_widget_set_tooltip_text(thing, _("Pitchbend"));
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    g_signal_connect(adj_pitchbend, "value_changed",
        G_CALLBACK(gui_adj_pitchbend_changed), NULL);

    thing = gtk_button_new_with_label("R");
    gtk_widget_set_tooltip_text(thing, _("Reset pitchbend to its normal value"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(gui_reset_pitch_bender), NULL);

    /* Instrument, sample, editing status */

    mainwindow_second_hbox = hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainvbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    editing_toggle = thing = gtk_check_button_new_with_label(_("Editing"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(G_OBJECT(thing), "toggled",
        G_CALLBACK(editing_toggled), NULL);

    label_octave = gtk_label_new(_("Octave"));
    gtk_box_pack_start(GTK_BOX(hbox), label_octave, FALSE, TRUE, 0);
    gtk_widget_show(label_octave);

    spin_octave = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(3.0, 0.0, 6.0, 1.0, 1.0, 0.0)), 0, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_octave));
    gtk_box_pack_start(GTK_BOX(hbox), spin_octave, FALSE, TRUE, 0);
    gtk_widget_show(spin_octave);

    label_jump = gtk_label_new(_("Jump"));
    gtk_box_pack_start(GTK_BOX(hbox), label_jump, FALSE, TRUE, 0);
    gtk_widget_show(label_jump);

    spin_jump = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(1.0, 0.0, 16.0, 1.0, 1.0, 0.0)), 0, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_jump));
    gtk_box_pack_start(GTK_BOX(hbox), spin_jump, FALSE, TRUE, 0);
    gtk_widget_show(spin_jump);

    /* The following 4 widgets are hidden and shown only in full-screen mode */
    label_songpos = gtk_label_new(_("Position"));
    gtk_box_pack_start(GTK_BOX(hbox), label_songpos, FALSE, TRUE, 0);

    spin_songpos = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 255.0, 1.0, 1.0, 0.0)), 0, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_songpos));
    gtk_box_pack_start(GTK_BOX(hbox), spin_songpos, FALSE, TRUE, 0);
    g_signal_connect(spin_songpos, "value-changed",
        G_CALLBACK(spin_songpos_changed), playlist);

    label_pat = gtk_label_new(_("Pattern"));
    gtk_box_pack_start(GTK_BOX(hbox), label_pat, FALSE, TRUE, 0);

    spin_pat = extspinbutton_new(playlist_get_songpat_adjustment(playlist), 0, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(spin_pat));
    gtk_box_pack_start(GTK_BOX(hbox), spin_pat, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Instr"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    curins_spin = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(1.0, 1.0, 128.0, 1.0, 16.0, 0.0)), 0, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(curins_spin));
    gtk_box_pack_start(GTK_BOX(hbox), curins_spin, FALSE, TRUE, 0);
    gtk_widget_show(curins_spin);
    g_signal_connect(curins_spin, "value-changed",
        G_CALLBACK(current_instrument_changed), NULL);

    inch_id = gui_get_text_entry(22, current_instrument_name_changed, &gui_curins_name);
    gtk_box_pack_start(GTK_BOX(hbox), gui_curins_name, TRUE, TRUE, 0);
    gtk_widget_show(gui_curins_name);
    gtk_widget_set_size_request(gui_curins_name, 100, -1);

    thing = gtk_label_new(_("Sample"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    cursmpl_spin = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 127.0, 1.0, 4.0, 0.0)), 0, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(cursmpl_spin));
    gtk_box_pack_start(GTK_BOX(hbox), cursmpl_spin, FALSE, TRUE, 0);
    gtk_widget_show(cursmpl_spin);
    g_signal_connect(cursmpl_spin, "value-changed",
        G_CALLBACK(current_sample_changed), NULL);

    snch_id = gui_get_text_entry(22, current_sample_name_changed, &gui_cursmpl_name);
    gtk_box_pack_start(GTK_BOX(hbox), gui_cursmpl_name, TRUE, TRUE, 0);
    gtk_widget_show(gui_cursmpl_name);
    gtk_widget_set_size_request(gui_cursmpl_name, 100, -1);

    thing = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, -40.0, 20.0, 1.0, 5.0, 0.0)), 0, 0, TRUE);
    extspinbutton_disable_size_hack(EXTSPINBUTTON(thing));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_set_tooltip_text(thing, _("Global amplification"));
    gtk_widget_show(thing);
    db_id = g_signal_connect(thing, "value-changed",
        G_CALLBACK(spin_db_changed), adj_amplification);
    g_signal_connect(adj_amplification, "value_changed",
        G_CALLBACK(gui_adj_amplification_changed), thing);

    thing = gtk_label_new(_("dB"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    /* The notebook */

    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(mainvbox), notebook, TRUE, TRUE, 0);
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_widget_show(notebook);
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 0);
    g_signal_connect(notebook, "switch_page",
        G_CALLBACK(notebook_page_switched), NULL);

    fileops_page_create(GTK_NOTEBOOK(notebook));
    tracker_page_create(GTK_NOTEBOOK(notebook));
    g_signal_connect(tracker, "ctrl_scroll",
        G_CALLBACK(scroll_songpos), NULL);
    instrument_page_create(GTK_NOTEBOOK(notebook));
    sample_editor_page_create(GTK_NOTEBOOK(notebook));
    modinfo_page_create(GTK_NOTEBOOK(notebook));

    // Activate tracker page
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook),
        1);
    notebook_current_page = 1;

    /* Status Bar */

    hbox = gtk_hbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
    gtk_box_pack_start(GTK_BOX(mainvbox), hbox, FALSE, TRUE, 0);
    gtk_widget_show(hbox);

    thing = gtk_frame_new(NULL);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(thing), GTK_SHADOW_IN);

    snprintf(buf, sizeof(buf), _("Welcome to %s!"), PACKAGE_NAME);
    status_bar = gtk_label_new(NULL);
    gui_statusbar_update_message(buf, FALSE);
    gtk_misc_set_alignment(GTK_MISC(status_bar), 0.0, 0.5);
    gtk_misc_set_padding(GTK_MISC(status_bar), 4, 0);
    gtk_widget_show(status_bar);
    gtk_container_add(GTK_CONTAINER(thing), status_bar);

    thing = gtk_frame_new(NULL);
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_frame_set_shadow_type(GTK_FRAME(thing), GTK_SHADOW_IN);

    st_clock = clock_new();
    gtk_widget_show(st_clock);
    gtk_container_add(GTK_CONTAINER(thing), st_clock);
    gtk_widget_set_size_request(st_clock, 48, 20);
    clock_set_format(CLOCK(st_clock), _("%M:%S"));
    clock_set_seconds(CLOCK(st_clock), 0);

    rcmgr = gtk_recent_manager_get_default();
    recent = gtk_recent_chooser_menu_new_for_manager(rcmgr);
    gtk_recent_chooser_menu_set_show_numbers(GTK_RECENT_CHOOSER_MENU(recent), TRUE);
    gtk_recent_chooser_set_local_only(GTK_RECENT_CHOOSER(recent), TRUE);
    gtk_recent_chooser_set_limit(GTK_RECENT_CHOOSER(recent), 10);
    gtk_recent_chooser_set_show_tips(GTK_RECENT_CHOOSER(recent), TRUE);
    gtk_recent_chooser_set_sort_type(GTK_RECENT_CHOOSER(recent), GTK_RECENT_SORT_MRU);
    rcfilter = gtk_recent_filter_new();
    gtk_recent_filter_add_application(rcfilter, PACKAGE);
    gtk_recent_chooser_set_filter(GTK_RECENT_CHOOSER(recent), rcfilter);
    thing = gui_get_widget(gui_builder, "recent_item", UI_FILE);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(thing), recent);
    g_signal_connect(recent, "item_activated", G_CALLBACK(recent_selected), NULL);

    /* capture all key presses */
    gtk_widget_add_events(GTK_WIDGET(mainwindow), GDK_KEY_RELEASE_MASK);
    g_signal_connect(mainwindow, "key-press-event", G_CALLBACK(keyevent), GINT_TO_POINTER(1));
    g_signal_connect(mainwindow, "key_release_event", G_CALLBACK(keyevent), GINT_TO_POINTER(0));

    /* This structure should be placed after all GUI construction to make all pointers
       to the file operation objects correctly initialized */
    struct menu_callback cb[] = {
        { "file_open", fileops_open_dialog, loadmod },
        { "file_save_as", fileops_open_dialog, savemod },
#if USE_SNDFILE || AUDIOFILE_VERSION
        { "file_save_wav", fileops_open_dialog, renderwav },
#endif
        { "file_save_xm", fileops_open_dialog, savexm },
        { "module_clear_all", menubar_clear_clicked, GINT_TO_POINTER(1) },
        { "module_clear_patterns", menubar_clear_clicked, GINT_TO_POINTER(0) },
        { "edit_cut", menubar_handle_cutcopypaste, GINT_TO_POINTER(0) },
        { "edit_copy", menubar_handle_cutcopypaste, GINT_TO_POINTER(1) },
        { "edit_paste", menubar_handle_cutcopypaste, GINT_TO_POINTER(2) },
        { "edit_track_increment_cmd", menubar_handle_edit_menu, GINT_TO_POINTER(0) },
        { "edit_track_decrement_cmd", menubar_handle_edit_menu, GINT_TO_POINTER(1) },
        { "popup_increment_cmd", menubar_handle_edit_menu, GINT_TO_POINTER(0) },
        { "popup_decrement_cmd", menubar_handle_edit_menu, GINT_TO_POINTER(1) },
        { "edit_selection_transpose_up", menubar_handle_edit_menu, GINT_TO_POINTER(2) },
        { "edit_selection_transpose_down", menubar_handle_edit_menu, GINT_TO_POINTER(3) },
        { "edit_selection_transpose_12up", menubar_handle_edit_menu, GINT_TO_POINTER(4) },
        { "edit_selection_transpose_12down", menubar_handle_edit_menu, GINT_TO_POINTER(5) },
        { "pattern_load", fileops_open_dialog, loadpat },
        { "pattern_save", fileops_open_dialog, savepat },
        { "track_current_permanent", menubar_toggle_perm_wrapper, GINT_TO_POINTER(0) },
        { "track_all_permanent", menubar_toggle_perm_wrapper, GINT_TO_POINTER(1) },
        { "instrument_load", fileops_open_dialog, loadinstr },
        { "instrument_save", fileops_open_dialog, saveinstr },
        { NULL }
    };

    /* All widgets and main data structures are created, now it's possible to connect signals */
    gtk_builder_connect_signals(gui_builder, tracker);
    history_init(gui_builder);

    for (i = 0; cb[i].widget_name; i++) {
        GtkWidget* w = gui_get_widget(gui_builder, cb[i].widget_name, UI_FILE);

        if (w)
            g_signal_connect_swapped(w, "activate", G_CALLBACK(cb[i].fn), cb[i].data);
    }

    if (argc == 2) {
        gchar* utfname = gui_filename_to_utf8(argv[1]);
        if (utfname) {
            if (!gui_load_xm(utfname, argv[1]))
                gui_new_xm();
            g_free(utfname);
        }
    } else {
        gui_new_xm();
    }

    menubar_init_prefs();
    fileops_page_post_create();
    gtk_widget_realize(mainwindow);
    di_init_display(mainwindow);

    colors_init(mainwindow, COLORS_GET_PREFS);
    gtk_window_set_icon_from_file(GTK_WINDOW(mainwindow), DATADIR "/" PACKAGE "/soundtracker-icon.png", NULL);
    gtk_widget_show(mainwindow);

    if (!keys_init()) {
        return 0;
    }

    if (gui_splash_window) {
        if (!gui_settings.gui_disable_splash && (gui_splash_logo || tips_dialog_show_tips)) {
            gdk_window_raise(gui_splash_window->window);
            gui_splash_set_label(_("Ready."), TRUE);
            if (gui_splash_logo) {
                gtk_widget_add_events(gui_splash_logo_area,
                    GDK_BUTTON_PRESS_MASK);
                g_signal_connect(gui_splash_logo_area, "button_press_event",
                    G_CALLBACK(gui_splash_close),
                    NULL);
            }
            gtk_widget_set_sensitive(gui_splash_close_button, TRUE);
        } else {
            gui_splash_close();
        }
    }

    gui_unset_focus();
    g_object_unref(gui_builder);
    return 1;
}
