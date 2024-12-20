
/*
 * The Real SoundTracker - instrument editor
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

#include <config.h>

#include <glib/gi18n.h>
#include <stdlib.h>
#include <math.h>

#include "clavier.h"
#include "envelope-box.h"
#include "errors.h"
#include "extspinbutton.h"
#include "file-operations.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "gui.h"
#include "history.h"
#include "instrument-editor.h"
#include "keys.h"
#include "module-info.h"
#include "sample-editor.h"
#include "st-subs.h"
#include "track-editor.h"
#include "tracker.h"
#include "xm.h"

file_op *saveinstr, *loadinstr;

struct InstrumentEditor {
    gboolean full;
    guint idle_handler;
};

static GtkWidget *volenv, *panenv, *disableboxes[5];
static GtkWidget* instrument_editor_vibtype_w;
static GtkWidget* clavier;
static GtkWidget* curnote_label;
static GtkAdjustment* instrument_page_adj[4];
static GObject* setting_objs[5];
static gint tags[5];

static gchar channels[XM_NUM_CHAN] = {0};
static gint keys[NUM_NOTES] = {0};

static STInstrument *current_instrument, *tmp_instrument = NULL, *tmp_instr2 = NULL;
static gint current_instrument_number = 0;
static STEnvelope env_buffer = {0};

static void
adjustment_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    gint tmp_value;
    gint* value = arg;

    g_assert(GTK_IS_ADJUSTMENT(data));

    tmp_value = rint(gtk_adjustment_get_value(GTK_ADJUSTMENT(data)));
    gtk_adjustment_set_value(GTK_ADJUSTMENT(data), *value);
    *value = tmp_value;
}

static inline void
log_adjustment(GtkAdjustment* adj,
    const gchar* title,
    const gint prev_value)
{
    history_log_action(HISTORY_ACTION_INT, title,
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | HISTORY_FLAG_COLLATABLE,
        adjustment_undo, adj, 0, prev_value);
}

static void
instrument_page_volfade_changed(GtkAdjustment *adj)
{
    log_adjustment(adj, _("Volume Fadeout changing"), current_instrument->volfade);
    current_instrument->volfade = rint(gtk_adjustment_get_value(adj));
}

static void
instrument_page_vibspeed_changed(GtkAdjustment *adj)
{
    log_adjustment(adj, _("Vibrato Speed changing"), current_instrument->vibrate);
    current_instrument->vibrate = rint(gtk_adjustment_get_value(adj));
}

static void
instrument_page_vibdepth_changed(GtkAdjustment *adj)
{
    log_adjustment(adj, _("Vibrato Depth changing"), current_instrument->vibdepth);
    current_instrument->vibdepth = rint(gtk_adjustment_get_value(adj));
}

static void
instrument_page_vibsweep_changed(GtkAdjustment *adj)
{
    log_adjustment(adj, _("Vibrato Sweep changing"), current_instrument->vibsweep);
    current_instrument->vibsweep = rint(gtk_adjustment_get_value(adj));
}

static void
combo_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    gint tmp_value;
    gint* value = arg;

    g_assert(GTK_IS_COMBO_BOX(data));

    tmp_value = gtk_combo_box_get_active(GTK_COMBO_BOX(data));
    gtk_combo_box_set_active(GTK_COMBO_BOX(data), *value);
    *value = tmp_value;
}

static void
instrument_editor_vibtype_changed(GtkComboBox* combo)
{
    gint type = gtk_combo_box_get_active(combo);

    if (current_instrument->vibtype != type) {
        history_log_action(HISTORY_ACTION_INT, _("Vibrato Type changing"),
            HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | HISTORY_FLAG_COLLATABLE,
            combo_undo, combo, 0, current_instrument->vibtype);
        current_instrument->vibtype = type;
    }
}

static void
sample_map_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    const gsize smap_size = sizeof(current_instrument->samplemap);
    gint8* tmp_mem = alloca(smap_size);

    memcpy(tmp_mem, data, smap_size);
    memcpy(data, arg, smap_size);
    memcpy(arg, tmp_mem, smap_size);

    /* Current instrument should already be set by operations history services */
    gtk_widget_queue_draw(clavier);
}

static void
check_and_log_samplemap(void)
{
    if (!history_test_collate(HISTORY_ACTION_POINTER,
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS, current_instrument->samplemap)) {
        const gsize smap_size = sizeof(current_instrument->samplemap);
        gint8* smap = g_malloc(smap_size);

        memcpy(smap, current_instrument->samplemap, smap_size);
        history_log_action(HISTORY_ACTION_POINTER, _("Sample map modification"),
            HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | HISTORY_FLAG_COLLATABLE,
            sample_map_undo, current_instrument->samplemap, smap_size, smap);
    }
}

static void
instrument_editor_clavierkey_press_event(GtkWidget* widget,
    gint key,
    gint button)
{
    if (button == 3) {
        check_and_log_samplemap();
        current_instrument->samplemap[key] = gui_get_current_sample();
        gtk_widget_queue_draw(clavier);
    } else {
        track_editor_do_the_note_key(key - 12 * gui_get_current_octave_value(), TRUE, 0, 0, TRUE);
        clavier_press(CLAVIER(clavier), key);
    }
}

static void
instrument_editor_init_samplemap(void)
{
    int key;
    int sample = gui_get_current_sample();

    check_and_log_samplemap();
    for (key = 0; key < ARRAY_SIZE(current_instrument->samplemap); key++) {
        current_instrument->samplemap[key] = sample;
    }

    clavier_set_key_labels(CLAVIER(clavier), current_instrument->samplemap);
}

static void
instrument_editor_clavierkey_release_event(GtkWidget* widget,
    gint key,
    gint button)
{
    if (button != 3) {
        track_editor_do_the_note_key(key - 12 * gui_get_current_octave_value(), FALSE, 0, 0, TRUE);
        clavier_release(CLAVIER(clavier), key);
    }
}

static gint
instrument_editor_clavierkey_enter_event(GtkWidget* widget,
    gint key,
    gpointer data)
{
    gtk_label_set_text(GTK_LABEL(curnote_label), tracker_get_note_name(key));
    return FALSE;
}

static gint
instrument_editor_clavierkey_leave_event(GtkWidget* widget,
    gint key,
    gpointer data)
{
    gtk_label_set_text(GTK_LABEL(curnote_label), NULL);
    return FALSE;
}

static gboolean
alloc_instr(STInstrument** instr, const gboolean clean)
{
    if (!*instr)
        *instr = g_new0(STInstrument, 1);

    if (!*instr) {
        gui_oom_error();
        return FALSE;
    }

    if (clean)
        st_clean_instrument(*instr, NULL);
    return TRUE;
}

static void
instrument_total_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer saved, gpointer current)
{
    if (!alloc_instr(&tmp_instr2, FALSE))
        return;

    gui_play_stop();
    st_copy_instrument(saved, tmp_instr2);
    st_copy_instrument(current, saved);
    st_copy_instrument(tmp_instr2, current);
    /* With some possible overhead, but in some cases
       we need to update not only the current instrument */
    modinfo_update_all();
    instrument_editor_update(FALSE);
    sample_editor_set_sample(&((STInstrument *)current)->samples[gui_get_current_sample()]);
}

static void
instrument_deallocate(gpointer data)
{
    STInstrument* arg = data;
    gint i;

    for (i = 0; i < XM_NUM_SAMPLES; i++)
        if (arg->samples[i].sample.data)
            g_free(arg->samples[i].sample.data);
}

gboolean
instrument_editor_check_and_log_instrument(STInstrument* instr,
    const gchar* title,
    const gint flags,
    gsize i_size)
{
    if (!i_size)
        i_size = st_get_instrument_size(instr);
    if (history_check_size(i_size)) {
        /* Undo is possible */
        STInstrument* arg = NULL;

        if (!alloc_instr(&arg, FALSE))
            return FALSE;
        /* Storing old instrument somewhere and take it for logging */
        st_copy_instrument(instr, arg);
        history_log_action_full(HISTORY_ACTION_POINTER, _(title), flags,
            instrument_total_undo, instrument_deallocate, instr, i_size, arg);
    } else if (!history_query_oversized(mainwindow))
        /* User rejected to continue without undo */
        return FALSE;

    return TRUE;
}

static void
instrument_editor_load_instrument(const gchar* fn, const gchar* localname)
{
    STInstrument* instr = current_instrument;
    FILE* f;
    gboolean modified = FALSE;

    g_assert(instr != NULL);

    /* Some overhead (load to temporary than copy) but we're now at the safe
       side that the existing instrument would not be overwritten if the
       instrument being loaded is bad. */

    if (!alloc_instr(&tmp_instr2, TRUE))
        return;

    f = gui_fopen(localname, fn, "rb");
    if (f) {
        gui_statusbar_update(STATUS_LOADING_INSTRUMENT, TRUE);
        if (xm_load_xi(tmp_instr2, f))
            modified = TRUE;
        else
            gui_statusbar_update(STATUS_IDLE, FALSE);
        fclose(f);
    }

    if (modified) {
        /* We take care about both the new and old instruments */
        if (instrument_editor_check_and_log_instrument(instr, N_("Instrument loading"),
            HISTORY_SET_PAGE(NOTEBOOK_PAGE_INSTRUMENT_EDITOR) | HISTORY_FLAG_LOG_INS | HISTORY_EXTRA_FLAGS(0),
            MAX(st_get_instrument_size(instr), st_get_instrument_size(tmp_instr2)))) {
            /* Instead of locking the instrument and samples, we simply stop playing. */
            gui_play_stop();

            st_copy_instrument_full(tmp_instr2, instr,
                gui_settings.ins_name_overwrite, gui_settings.smp_name_overwrite);
            instrument_editor_update(TRUE);
            sample_editor_set_sample(&instr->samples[gui_get_current_sample()]);
            gui_statusbar_update(STATUS_INSTRUMENT_LOADED, FALSE);
        }
    }
}

static void
instrument_editor_save_instrument(const gchar* fn, gchar* localname)
{
    STInstrument* instr = current_instrument;
    FILE* f;

    g_assert(instr != NULL);

    f = gui_fopen(localname, fn, "wb");

    if (f) {
        gui_statusbar_update(STATUS_SAVING_INSTRUMENT, TRUE);
        if (xm_save_xi(instr, f)) {
            static GtkWidget* dialog = NULL;

            gui_error_dialog(&dialog, _("Saving instrument failed."), FALSE);
            gui_statusbar_update(STATUS_IDLE, FALSE);
        } else
            gui_statusbar_update(STATUS_INSTRUMENT_SAVED, FALSE);
        fclose(f);
    }
}

gboolean instrument_editor_clear_current_instrument(void)
{
    /* Deleting the same instrument twice makes no sence, but we should be
       at the safe side and skip senseless action */
    if (history_test_collate(HISTORY_ACTION_POINTER,
        HISTORY_FLAG_LOG_INS | HISTORY_EXTRA_FLAGS(1), current_instrument))
        return FALSE;

    if (gui_ok_cancel_modal(mainwindow,
        _("Clear current instrument?"))) {
        if (instrument_editor_check_and_log_instrument(current_instrument, N_("Instrument clearing"),
            HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS |
            HISTORY_FLAG_COLLATABLE | HISTORY_EXTRA_FLAGS(1), 0)) {
            gui_play_stop();

            st_clean_instrument_full2(current_instrument, NULL,
                gui_settings.ins_name_overwrite, gui_settings.smp_name_overwrite);

            instrument_editor_update(TRUE);
            sample_editor_update();
        }
    }
    return TRUE;
}

static void copy_envelope(EnvelopeBox* e, STEnvelope* se)
{
    memcpy(&env_buffer, se, sizeof(env_buffer));
}

static void paste_envelope(EnvelopeBox* e, STEnvelope* se)
{
    if (env_buffer.num_points) { /* There is something in the buffer */
        envelope_box_log_action(e, N_("Envelope pasting"), 0);
        memcpy(se, &env_buffer, sizeof(env_buffer));
        envelope_box_set_envelope(e, se);
    }
}

void instrument_page_create(GtkNotebook* nb)
{
    GtkWidget *mainbox, *vbox, *thing, *box, *box2, *box3, *box4, *frame, *table;
    GtkListStore* list_store;
    GtkTreeIter iter;
    guint i;
    static const char* vibtypelabels[] = { N_("Sine"), N_("Square"), N_("Saw Down"), N_("Saw Up"), NULL };

    static const gchar* xi_f[] = { N_("FastTracker instruments (*.xi)"), "*.[xX][iI]", NULL };
    static const gchar** formats[] = { xi_f, NULL };

    mainbox = gtk_vbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(mainbox), 10);
    gtk_notebook_append_page(nb, mainbox, gtk_label_new(_("Instrument Editor")));
    gtk_widget_show(mainbox);

    add_empty_vbox(mainbox);

    disableboxes[0] = vbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), vbox, FALSE, TRUE, 0);
    gtk_widget_show(vbox);

    volenv = envelope_box_new(_("Volume envelope"));
    gtk_box_pack_start(GTK_BOX(vbox), volenv, TRUE, TRUE, 0);
    gtk_widget_show(volenv);
    g_signal_connect(volenv, "copy_env",
        G_CALLBACK(copy_envelope), NULL);
    g_signal_connect(volenv, "paste_env",
        G_CALLBACK(paste_envelope), NULL);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    panenv = envelope_box_new(_("Panning envelope"));
    gtk_box_pack_start(GTK_BOX(vbox), panenv, TRUE, TRUE, 0);
    gtk_widget_show(panenv);
    g_signal_connect(panenv, "copy_env",
        G_CALLBACK(copy_envelope), NULL);
    g_signal_connect(panenv, "paste_env",
        G_CALLBACK(paste_envelope), NULL);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    box = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), box, FALSE, TRUE, 0);
    gtk_widget_show(box);

    box2 = gtk_vbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(box), box2, TRUE, TRUE, 0);
    gtk_widget_show(box2);

    loadinstr = fileops_dialog_create(DIALOG_LOAD_INSTRUMENT, _("Load Instrument"),
        &gui_settings.loadinstr_path, instrument_editor_load_instrument, TRUE, FALSE, formats,
        N_("Load instrument in the current instrument slot"), NULL);
    saveinstr = fileops_dialog_create(DIALOG_SAVE_INSTRUMENT, _("Save Instrument"),
        &gui_settings.saveinstr_path, instrument_editor_save_instrument, TRUE, TRUE, formats,
        N_("Save the current instrument"), "xi");

    thing = gtk_button_new_with_label(_("Load XI"));
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(fileops_open_dialog), loadinstr);

    disableboxes[3] = thing = gtk_button_new_with_label(_("Save XI"));
    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(fileops_open_dialog), saveinstr);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    disableboxes[1] = box2 = gtk_vbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(box), box2, TRUE, TRUE, 0);

    box3 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    thing = gtk_label_new(_("Volume Fadeout"));
    gtk_box_pack_start(GTK_BOX(box3),thing, FALSE, FALSE, 0);
    setting_objs[0] = G_OBJECT(gtk_adjustment_new(0, 0, 0xfff, 1.0, 400.0, 0.0));
    instrument_page_adj[0] = GTK_ADJUSTMENT(setting_objs[0]);
    thing = extspinbutton_new(instrument_page_adj[0], 0.0, 0, TRUE);
    gtk_box_pack_end(GTK_BOX(box3),thing, FALSE, FALSE, 0);
    thing = gtk_hscale_new(instrument_page_adj[0]);
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show_all(box2);
    tags[0] = g_signal_connect(instrument_page_adj[0], "value_changed",
        G_CALLBACK(instrument_page_volfade_changed), NULL);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    disableboxes[4] = table = gtk_table_new(2, 2, TRUE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 2);
    gtk_table_set_col_spacings(GTK_TABLE(table), 8);
    gtk_box_pack_start(GTK_BOX(box), table, TRUE, TRUE, 0);

    box3 = gtk_hbox_new(FALSE, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), box3, 0, 1, 0, 1);

    thing = gtk_label_new(_("Vibrato:"));
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, FALSE, 0);
    list_store = gtk_list_store_new(1, G_TYPE_STRING);
    for (i = 0; vibtypelabels[i] != NULL; i++) {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, _(vibtypelabels[i]), -1);
    }
    instrument_editor_vibtype_w = thing = gui_combo_new(list_store);
    setting_objs[4] = G_OBJECT(thing);
    tags[4] = g_signal_connect(thing, "changed", G_CALLBACK(instrument_editor_vibtype_changed), NULL);
    gtk_box_pack_end(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    thing = gtk_label_new(_("Type"));
    gtk_box_pack_end(GTK_BOX(box3), thing, FALSE, FALSE, 0);
    thing = gui_subs_create_slider_full(_("Speed"), 0, 0x3f,
        instrument_page_vibspeed_changed, &instrument_page_adj[1], TRUE, &tags[1]);
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 1, 2, 0, 1);
    setting_objs[1] = G_OBJECT(instrument_page_adj[1]);

    thing = gui_subs_create_slider_full(_("Depth"), 0, 0xf,
        instrument_page_vibdepth_changed, &instrument_page_adj[2], TRUE, &tags[2]);
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 0, 1, 1, 2);
    setting_objs[2] = G_OBJECT(instrument_page_adj[2]);
    thing = gui_subs_create_slider_full(_("Sweep"), 0, 0xff,
        instrument_page_vibsweep_changed, &instrument_page_adj[3], TRUE, &tags[3]);
    gtk_table_attach_defaults(GTK_TABLE(table), thing, 1, 2, 1, 2);
    gtk_widget_show_all(table);
    setting_objs[3] = G_OBJECT(instrument_page_adj[3]);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    // Sample map editor coming up
    disableboxes[2] = box2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, TRUE, TRUE, 0);
    gtk_widget_show(box3);

    box = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(box), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_show(box);
    gtk_box_pack_start(GTK_BOX(box3), box, TRUE, TRUE, 0);

    clavier = clavier_new(gui_settings.clavier_font);
    gtk_widget_set_tooltip_text(clavier, _("Left click to play, right click to assign current sample to a note"));
    gtk_widget_show(clavier);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(box), clavier);

    gtk_widget_set_size_request(clavier, NUM_NOTES * 7, 50);

    clavier_set_range(CLAVIER(clavier), 0, 95);

    g_signal_connect(clavier, "clavierkey_press",
        G_CALLBACK(instrument_editor_clavierkey_press_event),
        NULL);
    g_signal_connect(clavier, "clavierkey_release",
        G_CALLBACK(instrument_editor_clavierkey_release_event),
        NULL);
    g_signal_connect(clavier, "clavierkey_enter",
        G_CALLBACK(instrument_editor_clavierkey_enter_event),
        NULL);
    g_signal_connect(clavier, "clavierkey_leave",
        G_CALLBACK(instrument_editor_clavierkey_leave_event),
        NULL);

    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);
    gtk_widget_show(box3);

    add_empty_vbox(box3);

    thing = gtk_label_new(_("Note:"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(box3), frame, FALSE, TRUE, 0);
    gtk_widget_show(frame);

    box4 = gtk_vbox_new(FALSE, 2);
    gtk_widget_show(box4);
    gtk_container_add(GTK_CONTAINER(frame), box4);
    gtk_container_set_border_width(GTK_CONTAINER(box4), 4);

    curnote_label = thing = gtk_label_new("");
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box4), thing, FALSE, TRUE, 0);

    thing = gtk_button_new_with_label(_("Init"));
    gtk_widget_set_tooltip_text(thing, _("Map all notes to the current sample"));
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(instrument_editor_init_samplemap), NULL);

    add_empty_vbox(box3);

    add_empty_vbox(mainbox);
}

void instrument_editor_copy_instrument(STInstrument* instr)
{
    if (!alloc_instr(&tmp_instrument, FALSE))
        return;
    st_copy_instrument(instr, tmp_instrument);
}

void instrument_editor_cut_instrument(STInstrument* instr)
{
    if (instrument_editor_check_and_log_instrument(instr, N_("Instrument cutting"),
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | HISTORY_EXTRA_FLAGS(2), 0)) {
        instrument_editor_copy_instrument(instr);
        st_clean_instrument(instr, NULL);
    }
}

void instrument_editor_paste_instrument(STInstrument* instr)
{
    if (tmp_instrument == NULL)
        return;

    if (instrument_editor_check_and_log_instrument(instr, N_("Instrument pasting"),
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | HISTORY_EXTRA_FLAGS(3),
        MAX(st_get_instrument_size(instr), st_get_instrument_size(tmp_instrument))))
        st_copy_instrument(tmp_instrument, instr);
}

void instrument_editor_set_instrument(STInstrument* i, const gint ins)
{
    current_instrument = i;
    current_instrument_number = ins;

    instrument_editor_update(FALSE);
}

STInstrument*
instrument_editor_get_instrument(void)
{
    return current_instrument;
}

void instrument_editor_xcopy_instruments(STInstrument* src_instr,
    STInstrument* dst_instr,
    gboolean xchg)
{
    if (xchg) {
        if (!alloc_instr(&tmp_instr2, FALSE))
            return;
        /* Both instruments are in the module -- no need to arrange special
           place to storing some of them */
        history_log_action(HISTORY_ACTION_POINTER_NOFREE, _("Instruments exchanging"),
            HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | HISTORY_EXTRA_FLAGS(5),
            instrument_total_undo, dst_instr, 0, src_instr);
        st_copy_instrument(dst_instr, tmp_instr2);
    } else {
        if (!instrument_editor_check_and_log_instrument(dst_instr,
            N_("Instrument copying"), HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | HISTORY_EXTRA_FLAGS(4),
            MAX(st_get_instrument_size(dst_instr), st_get_instrument_size(src_instr))))
        return;
    }

    st_copy_instrument(src_instr, dst_instr);
    if (xchg)
        st_copy_instrument(tmp_instr2, src_instr);
}

gboolean
instrument_editor_handle_keys(int shift,
    int ctrl,
    int alt,
    guint32 keyval,
    gboolean pressed)
{
    int i;

    i = keys_get_key_meaning(keyval, ENCODE_MODIFIERS(shift, ctrl, alt), -1);
    if (i != -1 && KEYS_MEANING_TYPE(i) == KEYS_MEANING_NOTE) {
        gboolean real_press = track_editor_do_the_note_key(i, pressed, keyval,
            ENCODE_MODIFIERS(shift, ctrl, alt), TRUE);
        if (real_press &&
            gui_playing_mode != PLAYING_PATTERN &&
            gui_playing_mode != PLAYING_SONG) { /* Manual release still remains for user pressed keys */
            gint key = i + 12 * gui_get_current_octave_value();
            (pressed ? clavier_press : clavier_release)(CLAVIER(clavier), key);
        }
        return TRUE;
    }

    return FALSE;
}

void
instrument_editor_indicate_key(const gint channel,
    const gint key)
{
    if (channel == -1) {
        clavier_release(CLAVIER(clavier), -1);
        memset(channels, 0, sizeof(channels));
        memset(keys, 0, sizeof(keys));
        return;
    }

    if (key >= 0) { /* Press */
        if (!keys[key]) /* First time press */
            clavier_press(CLAVIER(clavier), key);
        keys[key]++;
        channels[channel] = key;
    } else { /* Release */
        gint rkey = channels[channel];

        if (keys[rkey] > 0) {
            keys[rkey]--;
            if (!keys[rkey])
                clavier_release(CLAVIER(clavier), rkey);
        }
    }
}

static gboolean
instrument_editor_update_idle_function(struct InstrumentEditor *ie)
{
    gint n;
    gboolean o = current_instrument != NULL && st_instrument_num_samples(current_instrument) > 0;

    for (n = 0; n < ARRAY_SIZE(disableboxes); n++)
        gtk_widget_set_sensitive(disableboxes[n], o);

    if (!o) {
        envelope_box_set_envelope(ENVELOPE_BOX(volenv), NULL);
        envelope_box_set_envelope(ENVELOPE_BOX(panenv), NULL);
    } else {
        envelope_box_set_envelope(ENVELOPE_BOX(volenv), &current_instrument->vol_env);
        envelope_box_set_envelope(ENVELOPE_BOX(panenv), &current_instrument->pan_env);

        gui_block_signals(setting_objs, tags, TRUE);
        gtk_adjustment_set_value(instrument_page_adj[0], current_instrument->volfade);
        gtk_adjustment_set_value(instrument_page_adj[1], current_instrument->vibrate);
        gtk_adjustment_set_value(instrument_page_adj[2], current_instrument->vibdepth);
        gtk_adjustment_set_value(instrument_page_adj[3], current_instrument->vibsweep);
        gtk_combo_box_set_active(GTK_COMBO_BOX(instrument_editor_vibtype_w), current_instrument->vibtype);
        gui_block_signals(setting_objs, tags, FALSE);
    }

    if (ie->full)
        modinfo_update_instrument(current_instrument_number);

    ie->idle_handler = 0;
    return FALSE;
}

void
instrument_editor_update(gboolean full)
{
    static struct InstrumentEditor ie = {0};

    ie.full = full;

    if (current_instrument){
        gui_block_instrname_entry(TRUE); /* Preventing callback when changing instrument name entry */
        gtk_entry_set_text(GTK_ENTRY(gui_curins_name), current_instrument->utf_name);
        gui_block_instrname_entry(FALSE);
        clavier_set_key_labels(CLAVIER(clavier), current_instrument->samplemap);
    } else
        clavier_set_key_labels(CLAVIER(clavier), NULL);

    if (!ie.idle_handler) {
        ie.idle_handler = g_idle_add((GSourceFunc)instrument_editor_update_idle_function,
                                      &ie);
        g_assert(ie.idle_handler != 0);
    }
}

void
instrument_editor_update_clavier(const gchar* font)
{
    clavier_set_font(CLAVIER(clavier), font);
}
