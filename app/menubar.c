
/*
 * The Real SoundTracker - GUI (menu bar)
 *
 * Copyright (C) 1999-2019 Michael Krause
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

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "audioconfig.h"
#include "cheat-sheet.h"
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
#include "midi-settings.h"
#include "module-info.h"
#include "preferences.h"
#include "sample-editor.h"
#include "scope-group.h"
#include "st-subs.h"
#include "tips-dialog.h"
#include "track-editor.h"
#include "tracker-settings.h"
#include "transposition.h"
#include "xm-player.h"

extern GtkBuilder* gui_builder;

static GtkWidget* mark_mode;

static gboolean mark_mode_toggle_ignore = FALSE;

extern ScopeGroup* scopegroup;

void about_dialog(void)
{
    const gchar* const authors[] = {
        "Michael Krause <rawstyle@soundtracker.org>",
        "Yury Aliaev <mutab0r@rambler.ru>",
        "Jon Forsberg <zzed@cyberdude.com>",
        "Arthibus Gissehel <gissehel@vachefolle.com>",
        "Stefan Hager <b.hager@eduhi.at>",
        "Maik Naeher <m_naeher@informatik.uni-kl.de>",
        "Darin Ohashi <dmohashi@algonquin.uwaterloo.ca>",
        "Michael Schwendt <sidplay@geocities.com>",
        "Erik Thiele <erikyyy@studbox.uni-stuttgart.de>",
        "Kai Vehmanen <kai.vehmanen@wakkanet.fi>",
        "Yuuki Ninomiya <gm@smn.enjoy.ne.jp>",
        "Martin Andersson <t3tr1s@yahoo.com>",
        "Conrad Parker <conradp@cse.unsw.edu.au>",
        "Tomasz Maka <pasp@ll.pl>",
        "Nicolas Leveille <nicolas.leveille@free.fr>",
        "Jason Nunn <jsno@downunder.net.au>",
        "Luc Tanguay <luc.tanguay@bell.ca>",
        "Tijs van Bakel <smoke@casema.net>",
        "Olivier Glorieux <sglorzlinux@netcourrier.com>",
        "Rikard Bosnjakovic <bos@hack.org>",
        "Fabian Giesen <fg@farb-rausch.de>",
        "Felix Bohmann <fb@farb-rausch.de>",
        "Aki Kemppainen <kemppaia@assari.cc.tut.fi>",
        "Mardy <mardy@users.sourceforge.net>",
        "Thomas Uwe Gruettmueller <sloyment@gmx.net>",
        "Wilbern Cobb <wcobb@openbsd.org>",
        "Erik de Castro Lopo <erikd-100@mega-nerd.com>",
        "Anthony Van Groningen <avan@uwm.edu>",
        "Henri Jylkk\303\244 <hj@bsdmail.org>",
        "Frank Knappe <knappe@tu-harburg.de>",
        "Mathias Meyer <mathmeye@users.sourceforge.net>",
        "David Binderman <dcb314@hotmail.com>",
        "Jaan Pullerits <jaan@surnuaed.ee>",
        "Olivier Guilyardi <ml@xung.org>",
        "Markku Reunanen <marq@iki.fi>",
        "Miroslav Shatlev <coding@shaltev.org>",
        "Frank Haferkorn <WillyFoobar@gmx.de>",
        "St\303\251phane K. <stephanek@brutele.be>",
        "Matthew Berardi <metaquanta@users.sourceforge.net>",
        "Bernardo Innocenti <bernie@codewiz.org>",
        NULL };
    const gchar translators[] = "Colin Marquardt <colin.marquardt@gmx.de>\nThomas R. Koll <tomk32@tomk32.de>\n"
        "Atsushi Yamagata <yamagata@plathome.co.jp>\nYuuki Ninomiya <gm@smn.enjoy.ne.jp>\n"
        "Junichi Uekawa <dancer@netfort.gr.jp>\nYuri Bongiorno <yurix@tiscalinet.it>\n"
        "Zbigniew Chyla <chyla@alice.ci.pwr.wroc.pl>\n"
        "German Jose Gomez Garcia <german@pinon.ccu.uniovi.es>\n"
        "Vicente E. Llorens <vllorens@mundofree.com>\nMichael Shigorin <mike@altlinux.ru>\n"
        "Yury Aliaev <mutab0r@rambler.ru>\nMichel Robitaille <robitail@IRO.UMontreal.CA>\n"
        "europeen <karlkawada@wanadoo.fr>\nMatej Erman <matej.erman@guest.arnes.si>\n"
        "Xos\303\251 Anxo Pereira Torreiro <pereira@iponet.es>\nChristian Rose <menthos@menthos.com>\n"
        "Denis Lackovic <delacko@fly.srk.fer.hr>\nKeld Simonsen <keld@dkuug.dk>\n"
        "Petter Johan Olsen <petter.olsen@cc.uit.no>\nAndrej Kacian <andrej@kacian.sk>\n"
        "Steven Michael Murphy <murf@e-tools.com>\nAysun Kiran <aysunkir@yahoo.com>\n"
        "Clytie Siddall <clytie@riverland.net.au>";

    gchar *license = g_strdup_printf(_("%s is free software: you can redistribute it and/or modify it under the terms "
        "of the GNU General Public License as published by the Free Software Foundation; either version 2 "
        "of the License, or (at your option) any later version.\n\n"
        "%s is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the "
        "implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General "
        "Public License for more details.\n\n"
        "You should have received a copy of the GNU General Public License along with %s"
        ". If not, see: http://www.gnu.org/licenses/"), PACKAGE_NAME, PACKAGE_NAME, PACKAGE_NAME);

    gtk_show_about_dialog(GTK_WINDOW(mainwindow), "program_name", PACKAGE_NAME, "version", VERSION,
        "authors", authors, "license", license, "wrap-license", TRUE, "copyright",
        _("\302\251 1998-2019 Michael Krause\n\302\251 2020-2024 Yury Aliaev\n"
        "\n\nIncludes OpenCP player from Niklas Beisert and Tammo Hinrichs."),
        "translator-credits", translators,
        NULL);
    g_free(license);
}

static void
menubar_clear(gboolean all)
{
    if (all) {
        gui_free_xm();
        gui_new_xm();
    } else {
        gui_play_stop();
        st_clean_song(xm);
        gui_init_xm(1, TRUE);
    }
    history_clear(FALSE);
    gui_reset_title();
}

void menubar_clear_clicked(gpointer b)
{
    if (history_get_modified()) {
        if (gui_ok_cancel_modal((mainwindow),
                _("Are you sure you want to do this? All changes will be lost.\n"
                  "You even will not be able to revert your action!")))
            menubar_clear(GPOINTER_TO_INT(b));
    } else {
        menubar_clear(GPOINTER_TO_INT(b));
    }
}

void menubar_backing_store_toggled(GtkCheckMenuItem* widget)
{
    gui_settings.gui_use_backing_store = gtk_check_menu_item_get_active(widget);
    tracker_set_backing_store(tracker, gui_settings.gui_use_backing_store);
}

void menubar_scopes_toggled(GtkCheckMenuItem* widget)
{
    gui_settings.gui_display_scopes = gtk_check_menu_item_get_active(widget);
    scope_group_enable_scopes(scopegroup, gui_settings.gui_display_scopes);
}

void
menubar_show_inssmp_toggled(GtkCheckMenuItem* widget)
{
    if (!(gui_settings.show_ins_smp = gtk_check_menu_item_get_active(widget))) {
        guint i;

        /* Turn off instrument / sample indication in channels */
        for (i = 0; i < XM_NUM_CHAN; i++)
            scope_group_update_channel_status(scopegroup, i, -1, -1);
    }
}


void menubar_splash_toggled(GtkCheckMenuItem* widget)
{
    gui_settings.gui_disable_splash = gtk_check_menu_item_get_active(widget);
}

void menubar_mark_mode_toggled(GtkCheckMenuItem* widget)
{
    if (!mark_mode_toggle_ignore) {
        gboolean state = gtk_check_menu_item_get_active(widget);
        tracker_mark_selection(tracker, state);
    }
}

void menubar_save_settings_on_exit_toggled(GtkCheckMenuItem* widget)
{
    gui_settings.save_settings_on_exit = gtk_check_menu_item_get_active(widget);
}

void menubar_save_settings_now(void)
{
    gui_settings_save_config();
    keys_save_config();
    audioconfig_save_config();
    trackersettings_write_settings();
#if defined(DRIVER_ALSA_MIDI)
    midi_save_config();
#endif
    prefs_save();
}

void menubar_handle_cutcopypaste(gpointer a)
{
    static const gchar* signals[] = { "cut-clipboard", "copy-clipboard", "paste-clipboard" };

    Tracker* t = tracker;

    STInstrument* curins = &xm->instruments[gui_get_current_instrument() - 1];
    GtkWidget* focus_widget = GTK_WINDOW(mainwindow)->focus_widget;
    gint i = GPOINTER_TO_INT(a);

    if (GTK_IS_ENTRY(focus_widget)) {
        g_signal_emit_by_name(focus_widget, signals[i], NULL);
        return;
    }

    switch (i) {
    case 0: //Cut
        switch (notebook_current_page) {
        case NOTEBOOK_PAGE_TRACKER:
            if (GUI_EDITING)
                track_editor_cut_selection(NULL, t);
            break;
        case NOTEBOOK_PAGE_INSTRUMENT_EDITOR:
        case NOTEBOOK_PAGE_MODULE_INFO:
            instrument_editor_cut_instrument(curins);
            instrument_editor_update(TRUE);
            sample_editor_update();
            break;
        case NOTEBOOK_PAGE_SAMPLE_EDITOR:
            sample_editor_copy_cut_common(TRUE, TRUE);
            break;
        }
        break;
    case 1: //Copy
        switch (notebook_current_page) {
        case NOTEBOOK_PAGE_TRACKER:
            track_editor_copy_selection(NULL, t);
            break;
        case NOTEBOOK_PAGE_INSTRUMENT_EDITOR:
        case NOTEBOOK_PAGE_MODULE_INFO:
            instrument_editor_copy_instrument(curins);
            break;
        case NOTEBOOK_PAGE_SAMPLE_EDITOR:
            sample_editor_copy_cut_common(TRUE, FALSE);
            break;
        }
        break;
    case 2: //Paste
        switch (notebook_current_page) {
        case NOTEBOOK_PAGE_TRACKER:
            if (GUI_EDITING)
                track_editor_paste_selection(NULL, t, TRUE);
            break;
        case NOTEBOOK_PAGE_INSTRUMENT_EDITOR:
        case NOTEBOOK_PAGE_MODULE_INFO:
            instrument_editor_paste_instrument(curins);
            instrument_editor_update(TRUE);
            sample_editor_update();
            break;
        case NOTEBOOK_PAGE_SAMPLE_EDITOR:
            sample_editor_paste_clicked();
            break;
        }
        break;
    }
}

void menubar_handle_edit_menu(gpointer a)
{
    if (GUI_EDITING) {
        Tracker* t = tracker;

        switch (GPOINTER_TO_INT(a)) {
        case 0:
            track_editor_cmd_mvalue(t, TRUE); /* increment CMD value */
            break;
        case 1:
            track_editor_cmd_mvalue(t, FALSE); /* decrement CMD value */
            break;
        case 2:
            transposition_transpose_selection(t, +1);
            break;
        case 3:
            transposition_transpose_selection(t, -1);
            break;
        case 4:
            transposition_transpose_selection(t, +12);
            break;
        case 5:
            transposition_transpose_selection(t, -12);
            break;
        default:
            break;
        }
    }
}

void menubar_settings_tracker_next_font(void)
{
    trackersettings_cycle_font_forward(TRACKERSETTINGS(trackersettings));
}

void menubar_settings_tracker_prev_font(void)
{
    trackersettings_cycle_font_backward(TRACKERSETTINGS(trackersettings));
}

void menubar_toggle_perm_wrapper(gpointer all)
{
    track_editor_toggle_permanentness(tracker, GPOINTER_TO_INT(all));
}

void menubar_mute_toggle(void)
{
    scope_group_toggle_channel(scopegroup, tracker->cursor_ch);
}

void menubar_solo_channel(void)
{
    scope_group_solo_channel(scopegroup, tracker->cursor_ch);
}

void menubar_all_channels_on(void)
{
    scope_group_all_channels_on(scopegroup);
}

void menubar_init_prefs()
{
    GtkWidget* display_scopes = gui_get_widget(gui_builder, "settings_display_scopes", UI_FILE);
    GtkWidget* show_inssmp = gui_get_widget(gui_builder, "settings_show_ins_smp", UI_FILE);
    GtkWidget* backing_store = gui_get_widget(gui_builder, "settings_tracker_flicker_free", UI_FILE);
#if !defined(DRIVER_ALSA)
    GtkWidget* settings_midi = gui_get_widget(gui_builder, "settings_midi", UI_FILE);
#endif
#if USE_SNDFILE == 0 && !defined(AUDIOFILE_VERSION)
    GtkWidget* savewav = gui_get_widget(gui_builder, "file_save_wav", UI_FILE);
#endif
    GtkWidget* disable_splash = gui_get_widget(gui_builder, "settings_disable_splash", UI_FILE);
    GtkWidget* save_onexit = gui_get_widget(gui_builder, "settings_save_on_exit", UI_FILE);

    mark_mode = gui_get_widget(gui_builder, "edit_selection_mark_mode", UI_FILE);

    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(display_scopes), gui_settings.gui_display_scopes);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_inssmp), gui_settings.show_ins_smp);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(backing_store), gui_settings.gui_use_backing_store);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(disable_splash), gui_settings.gui_disable_splash);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(save_onexit), gui_settings.save_settings_on_exit);

#if USE_SNDFILE == 0 && !defined(AUDIOFILE_VERSION)
    gtk_widget_set_sensitive(savewav, FALSE);
#endif
#if !defined(DRIVER_ALSA)
    gtk_widget_set_sensitive(settings_midi, FALSE);
#endif
}

void menubar_block_mode_set(const gboolean state, const gboolean ignore)
{
    mark_mode_toggle_ignore = ignore;
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mark_mode), state);
    mark_mode_toggle_ignore = FALSE;
}

/* Move unused patterns to the end of the pattern space */
static void menubar_pack_patterns()
{
    int i, j, last, used;

    /* Get number of last used pattern and number of used patterns */
    for (i = 0, last = 0, used = 0; i < XM_NUM_PATTERNS; i++) {
        if (st_is_pattern_used_in_song(xm, i)) {
            last = i;
            used++;
        }
    }

    if (used <= last) {
        /* Put unused patterns to the end */
        for (i = 0; i < used;)
            if (!st_is_pattern_used_in_song(xm, i)) {
                for (j = i; j < last; j++)
                    st_exchange_patterns(xm, j, j + 1);
            } else {
                i++;
            }
        gui_playlist_initialize();
    }
}

/* Clear patterns which are not in the playlist */
static void menubar_clear_unused_patterns()
{
    gint i;

    for (i = 0; i < XM_NUM_PATTERNS; i++)
        if (!st_is_pattern_used_in_song(xm, i) && !st_is_empty_pattern(&xm->patterns[i]))
            st_clear_pattern(&xm->patterns[i]);

    tracker_redraw(tracker);
}

/* Optimize -- clear everything unused */

#define TYPE_OPT_DIALOG (opt_dialog_get_type())
#define OPT_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_OPT_DIALOG, OptDialog))

typedef struct _OptDialog OptDialog;
typedef struct _OptDialogClass OptDialogClass;

struct _OptDialog {
    GtkDialog dialog;
    GtkWidget *label1, *label2, *label3;
    GtkWidget *check1, *check2, *check3, *check4, *check5, *check6, *check7, *check8;
    gboolean *i_t, *s_t;
    gint used_ins, used_smp, a, b, f;
};

struct _OptDialogClass {
    GtkDialogClass parent_class;
};

static void
opt_dialog_init(OptDialog* d)
{
};

static void
opt_dialog_class_init(OptDialogClass* c)
{
};

G_DEFINE_TYPE(OptDialog, opt_dialog, GTK_TYPE_DIALOG);

static gboolean instr_is_used(XM* mod,
    XMNote* xmnote,
    const gint ins,
    gpointer data)
{
    OptDialog* arg = data;

    gint instr = xmnote->instrument - 1;
    gint note = xmnote->note;

    /* Specifying instrument without a note is rather
        a misuse of XM specification. So we will not
        take this into account. */
    if (note && note != XM_PATTERN_NOTE_OFF) {
        gint sindex = instr * XM_NUM_SAMPLES +
            xm->instruments[instr].samplemap[note - 1];

        if (!arg->i_t[instr]) {
            arg->i_t[instr] = TRUE;
            arg->used_ins++;
        }
        if (!arg->s_t[sindex]) {
            arg->s_t[sindex] = TRUE;
            arg->used_smp++;
        }
    }

    return TRUE;
}

static void
delete_instr_toggled(GtkToggleButton* tb,
    GtkWidget* w)
{
    gtk_widget_set_sensitive(w, gtk_toggle_button_get_active(tb));
}

static void
update_statistics(OptDialog* dia)
{
    gint i, j, d;
    gchar* infbuf;

    memset(dia->i_t, 0, sizeof(dia->i_t[0]) * XM_NUM_INSTRUMENTS);
    memset(dia->s_t, 0, sizeof(dia->i_t[0]) * XM_NUM_INSTRUMENTS * XM_NUM_SAMPLES);
    dia->used_ins = dia->used_smp = 0;

    for (i = 0, dia->b = 0, d = 0; i < XM_NUM_PATTERNS; i++) {
        if (st_is_pattern_used_in_song(xm, i))
            d++;
        else if (!st_is_empty_pattern(&xm->patterns[i]))
            dia->b++;
    }

    infbuf = g_strdup_printf(_("Unused patterns: %d (used: %d)"), dia->b, d);
    gtk_label_set_text(GTK_LABEL(dia->label1), infbuf);
    g_free(infbuf);
    gtk_widget_set_sensitive(dia->check1, dia->b > 0);
    gtk_widget_set_sensitive(dia->check8, dia->b > 0 &&
        !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check1)));

    st_for_each_note(xm, instr_is_used,
        (dia->b > 0 && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check8))
        && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check1))) ?
        ST_FLAGS_INSTR | ST_FLAGS_UNUSED : ST_FLAGS_INSTR, dia);

    for (i = 0, dia->a = 0, dia->f = 0; i < XM_NUM_INSTRUMENTS; i++) {
        if (dia->i_t[i]) {
            for (j = 0; j < XM_NUM_SAMPLES; j++) {
                if (!dia->s_t[j + i * XM_NUM_SAMPLES] &&
                    xm->instruments[i].samples[j].sample.length) {
                    /* In the sample table now _unused_ samples are marked */
                    dia->s_t[j + i * XM_NUM_SAMPLES] = TRUE;
                    dia->a++;
                } else
                    dia->s_t[j + i * XM_NUM_SAMPLES] = FALSE;
            }
            dia->i_t[i] = FALSE;
        } else {
            if (st_instrument_num_samples(&xm->instruments[i])) {
                /* In the same manner the instrument table is 'inverted' */
                dia->i_t[i] = TRUE;
                dia->f++;
            }
        }
    }

    infbuf = g_strdup_printf(_("Unused instruments: %d (used: %d)"), dia->f, dia->used_ins);
    gtk_label_set_text(GTK_LABEL(dia->label2), infbuf);
    g_free(infbuf);
    gtk_widget_set_sensitive(dia->check3, dia->f > 0);

    infbuf = g_strdup_printf(_("Unused samples: %d (used: %d)"), dia->a, dia->used_smp);
    gtk_label_set_text(GTK_LABEL(dia->label3), infbuf);
    g_free(infbuf);
    gtk_widget_set_sensitive(dia->check4, dia->a > 0 &&
        (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check3)) || dia->f == 0));
}

static void
clear_unused_toggled(GtkToggleButton* tb,
    OptDialog* dia)
{
    gtk_widget_set_sensitive(dia->check8,
        !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tb)));
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check8)))
        update_statistics(dia);
}

void menubar_optimize_module(void)
{
    static OptDialog *dia = NULL;
    gboolean instr_table[XM_NUM_INSTRUMENTS];
    gboolean* sample_table = g_new(gboolean,
        XM_NUM_INSTRUMENTS * XM_NUM_SAMPLES);
    gint i, j, response;

    if (!dia) {
        GtkWidget *vbox, *thing;

        dia = OPT_DIALOG(g_object_new(opt_dialog_get_type(),
        "has-separator", FALSE, "title", _("Module Optimization"), "modal", TRUE,
        "transient-for", GTK_WINDOW(mainwindow), NULL));
        gtk_dialog_add_buttons(GTK_DIALOG(dia), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(GTK_WIDGET(dia), GTK_RESPONSE_OK);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(dia));
        gtk_box_set_spacing(GTK_BOX(vbox), 4);

        dia->label1 = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(dia->label1), 0.0, 0.5);
        gtk_box_pack_start(GTK_BOX(vbox), dia->label1, FALSE, FALSE, 0);
        dia->check1 = gtk_check_button_new_with_label(_("Clear unused patterns"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dia->check1), TRUE);
        g_signal_connect(dia->check1, "toggled",
            G_CALLBACK(clear_unused_toggled), dia);
        gtk_box_pack_start(GTK_BOX(vbox), dia->check1, FALSE, FALSE, 0);
        dia->check2 = gtk_check_button_new_with_label(_("Pack patterns"));
        gtk_box_pack_start(GTK_BOX(vbox), dia->check2, FALSE, FALSE, 0);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dia->check2), TRUE);

        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);

        thing = gtk_table_new(5, 2, TRUE);
        for (i = 0; i < 5; i++)
            gtk_table_set_row_spacing(GTK_TABLE(thing), i, 4);
        gtk_table_set_col_spacing(GTK_TABLE(thing), 0, 4);
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);

        dia->label2 = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(dia->label2), 0.0, 0.5);
        gtk_table_attach_defaults(GTK_TABLE(thing), dia->label2, 0, 1, 0, 1);
        dia->check8 = gtk_check_button_new_with_label(_("Including unused pats"));
        gtk_widget_set_tooltip_text(dia->check8,
            _("Including instruments and samples from all non-empty patterns"));
        gtk_table_attach_defaults(GTK_TABLE(thing), dia->check8, 1, 2, 0, 1);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dia->check8), FALSE);
        g_signal_connect_swapped(dia->check8, "toggled",
            G_CALLBACK(update_statistics), dia);
        dia->label3 = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(dia->label3), 0.0, 0.5);
        gtk_table_attach_defaults(GTK_TABLE(thing), dia->label3, 0, 1, 1, 2);
        dia->check3 = gtk_check_button_new_with_label(_("Delete unused instruments"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dia->check3), TRUE);
        gtk_table_attach_defaults(GTK_TABLE(thing), dia->check3, 0, 1, 2, 3);
        dia->check4 = gtk_check_button_new_with_label(_("Delete unused samples"));
        gtk_table_attach_defaults(GTK_TABLE(thing), dia->check4, 0, 1, 3, 4);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dia->check4), FALSE);
        g_signal_connect(dia->check3, "toggled",
            G_CALLBACK(delete_instr_toggled), dia->check4);
        dia->check5 = gtk_check_button_new_with_label(_("Preserve instrument names"));
        gtk_table_attach_defaults(GTK_TABLE(thing), dia->check5, 1, 2, 2, 3);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dia->check5), TRUE);
        dia->check6 = gtk_check_button_new_with_label(_("Preserve sample names"));
        gtk_table_attach_defaults(GTK_TABLE(thing), dia->check6, 1, 2, 3, 4);
        gtk_widget_set_tooltip_text(dia->check6,
        _("This affects cleaning both of the unused samples and instruments"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dia->check6), TRUE);

        dia->check7 = gtk_check_button_new_with_label(_("Clear sample data after loop"));
        gtk_table_attach_defaults(GTK_TABLE(thing), dia->check7, 0, 2, 4, 5);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dia->check7), FALSE);

        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);

        thing = gtk_label_new(_("Note that you will not be able to revert this and previous actions!"));
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
        gtk_widget_show_all(vbox);
    }
    dia->i_t = instr_table;
    dia->s_t = sample_table;

    update_statistics(dia);

    response = gtk_dialog_run(GTK_DIALOG(dia));
    gtk_widget_hide(GTK_WIDGET(dia));

    if (response == GTK_RESPONSE_OK) {
        gboolean done = FALSE, update = FALSE, seu = FALSE;
        gboolean clean_ins = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check3));
        gboolean clean_ins_name = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check5));
        gboolean clean_smp_name = !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check6));

        if (dia->b && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check1))) {
            menubar_clear_unused_patterns();
            done = TRUE;
        }
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check2))) {
            menubar_pack_patterns();
            done = TRUE;
        }
        if (dia->f && clean_ins) {
            for (i = 0; i < XM_NUM_INSTRUMENTS; i++)
                if (instr_table[i])
                    st_clean_instrument_full2(&xm->instruments[i], NULL,
                        clean_ins_name, clean_smp_name);
            done = TRUE;
            update = TRUE;
            seu = TRUE;
        }
        if ((!dia->f || clean_ins)
            && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check4)) && dia->a) {
            for (i = 0; i < XM_NUM_INSTRUMENTS; i++)
                if (!instr_table[i])
                    for (j = 0; j < XM_NUM_SAMPLES; j++)
                        if (sample_table[i * XM_NUM_SAMPLES + j])
                            st_clean_sample_full(&xm->instruments[i].samples[j], NULL, NULL,
                                clean_smp_name);
            done = TRUE;
            update = TRUE;
            seu = TRUE;
        }
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dia->check7))) {
            for (i = 0; i < XM_NUM_INSTRUMENTS; i++)
                for (j = 0; j < XM_NUM_SAMPLES; j++) {
                    STSample* s = &xm->instruments[i].samples[j];

                    if (IS_SAMPLE_LOOPED(s->sample) &&
                        s->sample.length > s->sample.loopend) {
                        gint16* newdata = g_new(gint16, s->sample.loopend);
                        gint16* olddata = s->sample.data;

                        memcpy(newdata, s->sample.data, sizeof(gint16) * s->sample.loopend);
                        g_mutex_lock(&s->sample.lock);
                        s->sample.data = newdata;
                        s->sample.length = s->sample.loopend;
                        g_mutex_unlock(&s->sample.lock);
                        g_free(olddata);

                        done = TRUE;
                        seu = TRUE;
                    }
                }
        }

        if (seu)
            sample_editor_update();
        if (update) {
            instrument_editor_update(FALSE);
            modinfo_update_all();
        }
        if (done)
            history_clear(TRUE);
    }
    g_free(sample_table);
}

#define TYPE_ADJUST_DIALOG (adjust_dialog_get_type())
#define ADJUST_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_ADJUST_DIALOG, AdjustDialog))

typedef struct _AdjustDialog AdjustDialog;
typedef struct _AdjustDialogClass AdjustDialogClass;

struct _AdjustDialog {
    GtkDialog dialog;
    GtkWidget *amp_spin, *setto_spin, *vbox, *setnotesvol_check;
    GSList *group, *radio_list;
    gint current_radio;
};

struct _AdjustDialogClass {
    GtkDialogClass parent_class;
};

static void
adjust_dialog_init(AdjustDialog* d)
{
    d->radio_list = NULL;
    d->group = NULL;
    d->current_radio = 0;
};

static void
adjust_dialog_class_init(AdjustDialogClass* c)
{
};

G_DEFINE_TYPE(AdjustDialog, adjust_dialog, GTK_TYPE_DIALOG);

struct unit {
    guchar ins, smp;
    gint value;
};

typedef struct {
    guint n;
    gboolean pan;
    struct unit values[1];
} AmpAdjArg;

static void
vol_pan_adjust_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    XM* my_xm = data;
    AmpAdjArg* aaa = arg;
    guint i;

    for (i = 0; i < aaa->n; i++) {
        STSample* s = &my_xm->instruments[aaa->values[i].ins].samples[aaa->values[i].smp];
        gint val = aaa->pan ? s->panning : s->volume;

        if (aaa->pan)
            s->panning = aaa->values[i].value;
        else
            s->volume = aaa->values[i].value;
        aaa->values[i].value = val;
    }
    sample_editor_update();
}

struct SetNotesVolArg
{
    gfloat amp;
    gint ins;
    gboolean skip, overrange, is_logged;
};

static gboolean
adj_vol(guchar* vol, const gfloat amp)
{
    gboolean overrange = FALSE;
    gint ivol;
    gfloat newvol = (gfloat)(*vol) * amp;

    if (newvol > 64.0) {
        newvol = 64.0;
        overrange = TRUE;
    }
    if (!(ivol = lrintf(newvol)) && *vol) {
        ivol = 1;
        overrange = TRUE;
    }
    *vol = ivol;

    return overrange;
}

static void
one_time_logging(struct SetNotesVolArg* arg,
    XM* mod)
{
    if (!arg->is_logged) {
        arg->is_logged = TRUE;
        gui_log_song(N_("Notes' volume adjusting"), xm);
    }
}

static gboolean
set_notes_vol(XM* mod,
    XMNote* note,
    const gint ins,
    gpointer data)
{
    struct SetNotesVolArg* arg = data;

    if (!arg->ins || ((arg->skip == TRUE) ^ (arg->ins == ins))) {
        if (note->volume >= XM_NOTE_VOLUME_MIN && note->volume <= XM_NOTE_VOLUME_MAX) {
            one_time_logging(arg, mod);
            note->volume -= XM_NOTE_VOLUME_MIN;
            arg->overrange = adj_vol(&note->volume, arg->amp);
            note->volume += XM_NOTE_VOLUME_MIN;
        }
        if (note->fxtype == xmpCmdVolume) {
            one_time_logging(arg, mod);
            arg->overrange |= adj_vol(&note->fxparam, arg->amp);
        }
    }

    return TRUE;
}

static void
amp_adjust(const gfloat amp, const gboolean all,
    const gboolean skip_current, const gboolean setto,
    const gboolean notes_vol)
{
    guint i, j, k, n;
    AmpAdjArg* arg;
    gsize asize;
    gboolean overrange = FALSE, all_instr;
    gint curins = gui_get_current_instrument() - 1, start, end;

    if (all) { /* All instruments, maybe without the current */
        all_instr = !skip_current;
        start = 0;
        end = XM_NUM_INSTRUMENTS;
        for (i = 0, n = 0; i < XM_NUM_INSTRUMENTS; i++)
            n += st_instrument_num_samples(&xm->instruments[i]);
    } else { /* Current instrument only */
        all_instr = TRUE;
        start = curins;
        end = start + 1;
        n = st_instrument_num_samples(&xm->instruments[curins]);
    }
    if (!n) /* Instrument without samples, nothing to do */
        return;

    asize = sizeof(AmpAdjArg) + sizeof(struct unit) * (n - 1);
    arg = g_malloc(asize);
    arg->n = n;
    arg->pan = FALSE;

    for (i = start, k = 0; i < end; i++) {
        STInstrument* instr = &xm->instruments[i];

        for (j = 0; j < XM_NUM_SAMPLES; j++) {
            if (instr->samples[j].sample.length != 0) {
                if (all_instr || i != curins) {
                    arg->values[k].ins = i;
                    arg->values[k].smp = j;
                    arg->values[k++].value = instr->samples[j].volume;

                    if (setto)
                        instr->samples[j].volume = lrintf(amp);
                    else
                        overrange = adj_vol(&instr->samples[j].volume, amp);
                }
            }
        }
    }

    if (notes_vol && !setto) {
        struct SetNotesVolArg snv_arg = {.amp = amp,
            .ins = all && !skip_current ? 0 : curins + 1,
            .skip = skip_current, .overrange = FALSE, .is_logged = FALSE};

        st_for_each_note(xm, set_notes_vol, ST_FLAGS_VOLUME | ST_FLAGS_FXTYPE, &snv_arg);
        overrange |= snv_arg.overrange;
        tracker_redraw(tracker);
    }

    if (overrange) {
        static GtkWidget* dlg = NULL;

        gui_warning_dialog(&dlg, _("Some samples had either too low or too hight volume.\n"
            "After the amplification adjustment the ratio between volumes of the samples is changed."), FALSE);
    }

    history_log_action(HISTORY_ACTION_POINTER, _("Samples' volume adjusting"),
        all ? 0 : HISTORY_FLAG_LOG_INS, vol_pan_adjust_undo, xm, asize, arg);
    sample_editor_update();
}

static void
pan_adjust(const gint val, gboolean setto)
{
    guint j, k, n;
    AmpAdjArg* arg;
    gsize asize;
    gint curins = gui_get_current_instrument() - 1;
    STInstrument* instr = &xm->instruments[curins];

    n = st_instrument_num_samples(instr);
    if (!n) /* Instrument without samples, nothing to do */
        return;

    asize = sizeof(AmpAdjArg) + sizeof(struct unit) * (n - 1);
    arg = g_malloc(asize);
    arg->n = n;
    arg->pan = TRUE;

    for (j = 0, k = 0 ; j < XM_NUM_SAMPLES; j++) {
        if (instr->samples[j].sample.length != 0) {
            gint oldpan;

            arg->values[k].ins = curins;
            arg->values[k].smp = j;
            oldpan = arg->values[k++].value = instr->samples[j].panning;

            instr->samples[j].panning = setto ?
                val : val + oldpan * (128 - abs(val)) / 128;
        }
    }

    history_log_action(HISTORY_ACTION_POINTER, _("Samples' panning adjusting"),
        HISTORY_FLAG_LOG_INS, vol_pan_adjust_undo, xm, asize, arg);
    sample_editor_update();
}
static void
volpan_radio_toggled(GtkToggleButton* cb,
    AdjustDialog* ad)
{
    if (gtk_toggle_button_get_active(cb)) {
        gint i;
        GSList* l;

        for(i = 0, l = ad->radio_list; l; l = l->next, i++)
            if (l->data == cb)
                break;
        ad->current_radio = i;
    }
}

static void
adj_dialog_response(GtkDialog* dialog)
{
    gtk_dialog_response(dialog, GTK_RESPONSE_OK);
}

static GtkWidget*
adjust_dialog_line(AdjustDialog* dialog,
    const gchar * const title,
    const gfloat val,
    const gfloat from,
    const gfloat to,
    const gfloat step,
    const gfloat page,
    const gint digits)
{
    GtkWidget *mainbox, *thing;
    GtkObject* adj;

    mainbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(dialog->vbox), mainbox, TRUE, TRUE, 0);

    thing = gtk_radio_button_new_with_label(dialog->group, _(title));
    dialog->group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(thing));
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
    dialog->radio_list = g_slist_append(dialog->radio_list, thing);
    g_signal_connect(thing, "toggled",
        G_CALLBACK(volpan_radio_toggled), dialog);

    adj = gtk_adjustment_new(val, from, to, step, page, 0.0);
    thing = extspinbutton_new(GTK_ADJUSTMENT(adj), 1.0, digits, FALSE);
    gtk_box_pack_end(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
    g_signal_connect_swapped(thing, "activate",
        G_CALLBACK(adj_dialog_response), dialog);

    return thing;
}

static GtkWidget*
adjust_dialog_new(const gchar * const title)
{
    GtkWidget *vbox;
    AdjustDialog* dialog = ADJUST_DIALOG(g_object_new(adjust_dialog_get_type(),
        "has-separator", TRUE, "title", title, "modal", TRUE,
        "transient-for", GTK_WINDOW(mainwindow), NULL));

    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

    vbox = dialog->vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(vbox), 4);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

    dialog->amp_spin = adjust_dialog_line(dialog, N_("Amplification"),
        1.0, 0.1, 10.0, 0.01, 1.0, 2);
    dialog->setnotesvol_check = gtk_check_button_new_with_label(_("Adjust volume of notes"));
    gtk_widget_set_tooltip_text(dialog->setnotesvol_check,
        _("Also adjust explicitly specified volume of notes in the patterns. "
        "Note that this action requires double undo: one for notes, another one for instruments."));
    gtk_box_pack_start(GTK_BOX(ADJUST_DIALOG(dialog)->vbox),
        dialog->setnotesvol_check, FALSE, FALSE, 0);
    dialog->setto_spin = adjust_dialog_line(dialog, N_("Set to"),
        32.0, 0.0, 64.0, 1.0, 10.0, 0);

    return GTK_WIDGET(dialog);
}

void menubar_adjust_volumes_dialog(void)
{
    static GtkWidget *dialog = NULL, *amp_except_check;
    GtkWidget* amp_spin;
    AdjustDialog* ad;
    gint resp;

    if (!dialog) {
        dialog = adjust_dialog_new(_("All samples' vol / pan adjusting"));
        amp_except_check = gtk_check_button_new_with_label(_("Except current instrument"));
        gtk_box_pack_start(GTK_BOX(ADJUST_DIALOG(dialog)->vbox),
            amp_except_check, FALSE, FALSE, 0);
        gtk_widget_show_all(dialog);
    }

    ad = ADJUST_DIALOG(dialog);
    amp_spin = ad->amp_spin;
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_widget_grab_focus(amp_spin);
    resp = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(dialog);

    if (resp == GTK_RESPONSE_OK)
        amp_adjust(gtk_spin_button_get_value(GTK_SPIN_BUTTON(ad->current_radio == 0 ?
            amp_spin : ad->setto_spin)),
            TRUE, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(amp_except_check)),
            ad->current_radio == 1,
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ad->setnotesvol_check)));
}

void menubar_adjust_instr_volpan_dialog(void)
{
    static GtkWidget *dialog = NULL, *pan_adj_spin, *pan_setto_spin;
    GtkWidget* amp_spin;
    AdjustDialog* ad;
    gint resp;

    if (!dialog) {
        GtkWidget *label, *vbox;

        dialog = adjust_dialog_new(_("Samples' volume adjusting"));
        vbox = ADJUST_DIALOG(dialog)->vbox;
        label = gtk_label_new(_("Volume"));
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
        gtk_box_reorder_child(GTK_BOX(vbox), label, 0);

        label = gtk_label_new(_("Panning"));
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
        ad = ADJUST_DIALOG(dialog);
        pan_adj_spin = adjust_dialog_line(ad, N_("Shift"),
            0.0, -128.0, 127.0, 1.0, 10.0, 0);
        pan_setto_spin = adjust_dialog_line(ad, N_("Set to"),
            0.0, -128.0, 127.0, 1.0, 10.0, 0);

        gtk_widget_show_all(dialog);
    } else
        ad = ADJUST_DIALOG(dialog);

    amp_spin = ad->amp_spin;
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_widget_grab_focus(amp_spin);
    resp = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(dialog);

    if (resp == GTK_RESPONSE_OK) {
        gboolean set_notes_vol = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ad->setnotesvol_check));

        switch (ad->current_radio) {
        case 0:
            amp_adjust(gtk_spin_button_get_value(GTK_SPIN_BUTTON(amp_spin)), FALSE, FALSE, FALSE, set_notes_vol);
            break;
        case 1:
            amp_adjust(gtk_spin_button_get_value(GTK_SPIN_BUTTON(ad->setto_spin)), FALSE, FALSE, TRUE, set_notes_vol);
            break;
        case 2:
            pan_adjust(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(pan_adj_spin)), FALSE);
            break;
        case 3:
            pan_adjust(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(pan_setto_spin)), TRUE);
            break;
        default:
            break;
        }
    }
}
