
/*
 * The Real SoundTracker - module info page
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

#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extspinbutton.h"
#include "gui-subs.h"
#include "gui.h"
#include "history.h"
#include "instrument-editor.h"
#include "keys.h"
#include "main.h"
#include "module-info.h"
#include "sample-editor.h"
#include "st-subs.h"
#include "track-editor.h"
#include "xm.h"

static GtkWidget *ilist, *slist, *ilist2, *slist2, *songname, *sw1, *sw2;
static GtkWidget *freqmode_w[2], *ptmode_toggle;
static GtkWidget *ins1_label, *smp1_label, *ins2_label, *smp2_label, *mix_label1, *mix_label2, *norm_check;
static guint curi = 0, curs = 0, desti = 0, dests = 0, mix_prev_note;
static gfloat mixlevel = 50.0;
static gboolean is_tuning = FALSE;

struct tuner {
    GtkWidget *frame, *note_label, *note2_label, *note_spin;
    GtkWidget *period_label, *period_label2, *period_spin, *play, *stop;
    guint note, mode, period;
    gboolean is_ref;
    gint timer;
    const gchar *label_text;
};

static struct tuner *tuning, *ref;

static STSample prev_sample = {{0}};

static const gchar* mode1[] = {N_("Single"), N_("Cyclic"), N_("Keyboard")};
static const gchar* mode2[] = {N_("Single"), N_("Cyclic"), N_("Coupled")};
enum {
    MODE_SINGLE = 0,
    MODE_CYCLIC,
    MODE_KEYBOARD,
    MODE_COUPLED = MODE_KEYBOARD,
    MODE_LAST
};

static void
ptmode_changed(GtkToggleButton* widget)
{
    if (xm) {
        history_log_toggle_button(widget, _("Protracker mode changing"),
            HISTORY_FLAG_LOG_PAGE, xm->flags & XM_FLAGS_IS_MOD);
        if (gtk_toggle_button_get_active(widget))
            xm->flags |= XM_FLAGS_IS_MOD;
        else
            xm->flags &= ~XM_FLAGS_IS_MOD;
    }
}

static void
freqmode_changed(GtkToggleButton* widget)
{
    if (xm && gtk_toggle_button_get_active(widget)) {
        gint m = find_current_toggle(freqmode_w, 2);

        history_log_radio_group(freqmode_w, _("Frequency mode changing"),
            HISTORY_FLAG_LOG_PAGE, (xm->flags & XM_FLAGS_AMIGA_FREQ) != 0 ? 1 : 0, 2);
        if (m)
            xm->flags |= XM_FLAGS_AMIGA_FREQ;
        else
            xm->flags &= ~XM_FLAGS_AMIGA_FREQ;
    }
}

static void
songname_changed(GtkEntry* entry)
{
    gchar* term;

    history_log_entry(entry, _("Song name changing"), 20 * 4,
        HISTORY_FLAG_LOG_PAGE, xm->utf_name);
    g_utf8_strncpy(xm->utf_name, gtk_entry_get_text(entry), 20);
    term = g_utf8_offset_to_pointer(xm->utf_name, 21);
    term[0] = 0;
    xm->needs_conversion = TRUE;
}

static void
modinfo_update_all_samples(const gint n, const gboolean second)
{
    guint i;
    GtkTreeModel* model;
    GtkWidget* list = second ? slist2 : slist;
    GtkListStore* slist_store = GUI_GET_LIST_STORE(list);

    model = gui_list_freeze(list);
    for (i = 0; i < XM_NUM_SAMPLES; i++) {
        GtkTreeIter iter;

        if (!gui_list_get_iter(i, model, &iter))
            return; /* Some bullshit happens :-/ */
        gtk_list_store_set(slist_store, &iter, 1,
            xm->instruments[n].samples[i].utf_name, -1);
    }
    gui_list_thaw(list, model);
}

static void
modinfo_update_instrument_full(const gint n, const gboolean second)
{
    GtkTreeIter iter;
    GtkTreeModel* tree_model = GUI_GET_TREE_MODEL(second ? ilist2 : ilist);

    if (!gui_list_get_iter(n, tree_model, &iter))
        return; /* Some bullshit happens :-/ */
    gtk_list_store_set(GTK_LIST_STORE(tree_model), &iter, 1, xm->instruments[n].utf_name,
        2, st_instrument_num_samples(&xm->instruments[n]), -1);

    if (n == (second ? desti : curi))
        modinfo_update_all_samples(n, second);
}

static void
update_labels(struct tuner* t,
    GtkWidget *label,
    GtkWidget *mix_label,
    const gint n,
    const gint ins,
    const gint smp)
{
    gchar *buf;

    buf = g_strdup_printf("%i", n);
    gtk_label_set_text(GTK_LABEL(label), buf);
    g_free(buf);
    buf = g_strdup_printf(_("%sIns. %i, Smp. %i"), t->label_text, ins + 1, smp);
    gtk_frame_set_label(GTK_FRAME(t->frame), buf);
    g_free(buf);
    buf = g_strdup_printf(_("I%i, S%i"), ins + 1, smp);
    gtk_label_set_text(GTK_LABEL(mix_label), buf);
    g_free(buf);
}

static void
ilist_select(GtkTreeSelection* sel)
{
    gint row = gui_list_get_selection_index(sel);

    if (row != -1 && row != curi) {
        curi = row;
        gui_set_current_instrument(row + 1);
        update_labels(tuning, ins1_label, mix_label1, row + 1, curi, curs);
    }
}

static void
slist_select(GtkTreeSelection* sel)
{
    gint row = gui_list_get_selection_index(sel);

    if (row != -1 && row != curs) {
        curs = row;
        gui_set_current_sample(row);
        update_labels(tuning, smp1_label, mix_label1, row, curi, curs);
    }
}

static void
ilist2_select(GtkTreeSelection* sel)
{
    gint row = gui_list_get_selection_index(sel);

    if (row != -1 && row != desti) {
        desti = row;
        modinfo_update_all_samples(row, TRUE);
        update_labels(ref, ins2_label, mix_label2, row + 1, desti, dests);
    }
}

static void
slist2_select(GtkTreeSelection* sel)
{
    gint row = gui_list_get_selection_index(sel);

    if (row != -1 && row != dests) {
        dests = row;
        update_labels(ref, smp2_label, mix_label2, row, desti, dests);
    }
}

static GtkWidget*
add_label(const gchar* title,
    const gchar* value,
    const GtkWidget* box)
{
    GtkWidget *box2, *thing, *frame;

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), box2, FALSE, FALSE, 0);

    thing = gtk_label_new(_(title));
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_box_pack_end(GTK_BOX(box2), frame, FALSE, FALSE, 0);
    gtk_widget_show(frame);

    thing = gtk_label_new(value);
    gtk_misc_set_alignment(GTK_MISC(thing), 1.0, 0.5);
    gtk_container_add(GTK_CONTAINER(frame), thing);

    return thing;
}

static void
exp_change(GtkExpander *expndr)
{
    is_tuning = gtk_expander_get_expanded(expndr);

    (is_tuning ? gtk_widget_show : gtk_widget_hide)(sw1);
    (is_tuning ? gtk_widget_show : gtk_widget_hide)(sw2);
    if (!is_tuning) {
        modinfo_stop_cycling();
        if (gui_playing_mode == PLAYING_NOTE)
            gui_play_stop();
        else {
            gui_stop_note(0);
            gui_stop_note(1);
        }
    }
}

static void
modinfo_xcopy_ins(const gpointer data)
{
    if (curi != desti) {
        gint action = GPOINTER_TO_INT(data);
        STInstrument* src_ins = &xm->instruments[curi];
        STInstrument* dst_ins = &xm->instruments[desti];

        /* It's simplier just to stop playing instead of performing some doubtful locks */
        gui_play_stop();

        instrument_editor_xcopy_instruments(src_ins, dst_ins, action);
        if (action) {
            instrument_editor_update(FALSE);
            sample_editor_update();
            modinfo_update_instrument(curi);
        }
        modinfo_update_instrument(desti);
    }
}

static
void modinfo_update_sample_full(const gint ins, const gint smp, const gboolean second)
{
    GtkTreeIter iter;
    GtkTreeModel* tree_model = GUI_GET_TREE_MODEL(second ? slist2 : slist);

    if (!gui_list_get_iter(smp, tree_model, &iter))
        return; /* Some bullshit happens :-/ */
    gtk_list_store_set(GTK_LIST_STORE(tree_model), &iter, 1,
        xm->instruments[ins].samples[smp].utf_name, -1);
}

/* Not a simple case. We have to back up both a sample and the
   sample map of an instrument using one argument */
struct Smp_n_MapBackup {
    gint ins, smp; /* Destination instrument and sample */
    gint8 samplemap[96];
    STSample sample;
    gint16 data[1];
};

static struct Smp_n_MapBackup*
make_sample_copy_arg(const gint ins, const gint smp, const guint32 backup_size)
{
    struct Smp_n_MapBackup* arg;
    STInstrument* src_ins = &xm->instruments[ins];
    STSample* src_smp = &src_ins->samples[smp];

    arg = g_malloc(backup_size);
    if (!arg) {
        gui_oom_error();
        return NULL;
    }

    arg->ins = ins;
    arg->smp = smp;
    memcpy(arg->samplemap, src_ins->samplemap, sizeof(arg->samplemap));
    arg->sample = *src_smp;
    if (src_smp->sample.length)
        memcpy(arg->data, src_smp->sample.data, src_smp->sample.length * sizeof(arg->data[0]));

    return arg;
}

static void sample_copy_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    struct Smp_n_MapBackup* bup = arg;
    STInstrument* dst_ins = &xm->instruments[bup->ins];
    STSample* dst_smp = &dst_ins->samples[bup->smp];
    gint8* tmp_smap = alloca(sizeof(bup->samplemap));
    const gsize data_length = dst_smp->sample.length * sizeof(dst_smp->sample.data[0]);
    gint16* tmp_data = NULL;
    STSample tmp_smp;
    GMutex tmp_lock;

    if (data_length) {
        tmp_data = g_malloc(data_length);
        if (!tmp_data) {
            gui_oom_error();
            return;
        }
        memcpy(tmp_data, dst_smp->sample.data, data_length);
    }
    tmp_smp = *dst_smp;
    memcpy(tmp_smap, dst_ins->samplemap, sizeof(dst_ins->samplemap));

    g_mutex_lock(&dst_smp->sample.lock);
    tmp_lock = dst_smp->sample.lock;
    if (dst_smp->sample.length && dst_smp->sample.data)
        g_free(dst_smp->sample.data);
    memcpy(dst_ins->samplemap, bup->samplemap, sizeof(dst_ins->samplemap));
    *dst_smp = bup->sample;
    if (dst_smp->sample.length) {
        dst_smp->sample.data = g_new(gint16, dst_smp->sample.length);
        memcpy(dst_smp->sample.data, bup->data, dst_smp->sample.length * sizeof(bup->data[0]));
    } else
        dst_smp->sample.data = NULL;
    dst_smp->sample.lock = tmp_lock;
    g_mutex_unlock(&dst_smp->sample.lock);

    bup->sample = tmp_smp;
    if (tmp_data) {
        memcpy(bup->data, tmp_data, bup->sample.sample.length * sizeof(tmp_data[0]));
        g_free(tmp_data);
    }
    memcpy(bup->samplemap, tmp_smap, sizeof(bup->samplemap));

    modinfo_update_instrument(bup->ins);
}

static void
sample_xchg_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    sample_editor_xcopy_samples(arg, data, TRUE);
    modinfo_update_instrument_full(ins - 1, FALSE);
    modinfo_update_instrument_full(desti, TRUE);
}

static void
modinfo_xcopy_smp(const gpointer data)
{
    if (curi != desti || curs != dests) {
        gint action = GPOINTER_TO_INT(data);
        STInstrument* dest_ins = &xm->instruments[desti];
        STInstrument* src_ins = &xm->instruments[curi];
        STSample* dest_smp = &dest_ins->samples[dests];
        STSample* src_smp = &src_ins->samples[curs];

        if (action) {
            history_log_action(HISTORY_ACTION_POINTER_NOFREE, _("Sample exchanging"),
                HISTORY_FLAG_LOG_ALL, sample_xchg_undo, src_smp, 0, dest_smp);
        } else {
            struct Smp_n_MapBackup* arg;
            const guint32 data_length = MAX(src_smp->sample.length, dest_smp->sample.length);
            const guint32 backup_size = sizeof(struct Smp_n_MapBackup) +
                sizeof(arg->data[0]) * (data_length - 1);

            if (history_check_size(backup_size)) {
                arg = make_sample_copy_arg(desti, dests, backup_size);
                history_log_action(HISTORY_ACTION_POINTER, _("Sample overwriting"),
                    HISTORY_FLAG_LOG_ALL, sample_copy_undo, NULL, backup_size, arg);
            } else if (!history_query_oversized(mainwindow))
                return;
        }

        if (st_instrument_num_samples(dest_ins) == 0) {
            st_clean_instrument_full(dest_ins, NULL, FALSE);
            memset(dest_ins->samplemap, dests, sizeof(dest_ins->samplemap));
        }
        if (action && st_instrument_num_samples(src_ins) == 0) {
            st_clean_instrument_full(src_ins, NULL, FALSE);
            memset(src_ins->samplemap, curs, sizeof(src_ins->samplemap));
        }
        sample_editor_xcopy_samples(src_smp, dest_smp, action);

        if (action) {
            sample_editor_update();
            instrument_editor_update(TRUE);
        }
        modinfo_update_instrument(desti);
        if (curi == desti)
            modinfo_update_sample_full(desti, dests, FALSE);
    }
}

static void
note_changed(GtkSpinButton* spin, struct tuner* t)
{
    t->note = gtk_spin_button_get_value_as_int(spin);
    gtk_label_set_text(GTK_LABEL(t->note_label), tracker_get_note_name(t->note));
}

static void
mix_prev_note_changed(GtkSpinButton* spin, GtkLabel* label)
{
    mix_prev_note = gtk_spin_button_get_value_as_int(spin);
    gtk_label_set_text(label, tracker_get_note_name(mix_prev_note));
}

static void
period_changed(GtkSpinButton* spin, struct tuner* t)
{
    t->period = lrint(gtk_spin_button_get_value(spin) * 1000.0);
}

static void
mode_changed(GtkComboBox* combo, struct tuner* t)
{
    gboolean active;

    t->mode = gtk_combo_box_get_active(combo);

    active = (t->mode == MODE_CYCLIC);
    gtk_widget_set_sensitive(t->period_label, active);
    gtk_widget_set_sensitive(t->period_spin, active);
    gtk_widget_set_sensitive(t->period_label2, active);

    active = !(t->mode == MODE_KEYBOARD); /* == MODE_COUPLED also */
    gtk_widget_set_sensitive(t->play, active);
    gtk_widget_set_sensitive(t->stop, active);
    if (!t->is_ref) {
        gtk_widget_set_sensitive(t->note2_label, active);
        gtk_widget_set_sensitive(t->note_spin, active);
    }
}

static gboolean
play_note(gpointer data)
{
    struct tuner* t = (struct tuner*)data;

    gint ins_num = t->is_ref ? desti : curi;
    gint smp_num = t->is_ref ? dests : curs;
    STSample* sample = &xm->instruments[ins_num].samples[smp_num];

    if (!sample) {
        t->timer = -1;
        return FALSE;
    }

    gui_play_note_full(t->is_ref ? 0 : 1, t->note + 1, sample, 0, sample->sample.length, TRUE, ins_num + 1, smp_num);
    if (!t->is_ref && ref->mode == MODE_COUPLED) {
        sample = &xm->instruments[desti].samples[dests];
        if (sample)
            gui_play_note_full(0, ref->note + 1, sample, 0, sample->sample.length, FALSE, desti + 1, dests);
    }

    return TRUE;
}

static void
play_clicked(struct tuner* t)
{
    switch (t->mode) {
    case MODE_SINGLE:
        play_note(t);
        break;
    case MODE_CYCLIC:
        if (t->timer == -1) {
            play_note(t);
            t->timer = g_timeout_add(t->period, play_note, t);
        }
        break;
    default:
        break;
    }
}

static void
stop_clicked(struct tuner* t)
{
    if (t->timer != -1) {
        g_source_remove(t->timer);
        t->timer = -1;
    }
    gui_stop_note(t->is_ref ? 0 : 1);
    if (!t->is_ref && ref->mode == MODE_COUPLED)
        gui_stop_note(0);
}

static struct tuner*
tuner_new(const gboolean is_ref, const gchar* label_text)
{
    struct tuner* t = g_new(struct tuner, 1);

    t->note = 48;
    t->mode = 0;
    t->period = 1000;
    t->is_ref = is_ref;
    t->timer = -1;
    t->label_text = label_text;

    t->frame = gtk_frame_new("");
    gtk_frame_set_shadow_type(GTK_FRAME(t->frame), GTK_SHADOW_IN);

    return t;
}

static GtkWidget*
tuner_populate(struct tuner* t)
{
    GtkWidget *vbox, *hbox, *thing;
    GtkListStore* list_store;
    GtkTreeIter iter;
    gint i, width, height;

    vbox = gtk_vbox_new(TRUE, 2);
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    t->play = thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), gui_get_pixmap(GUI_PIXMAP_PLAY));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    g_signal_connect_swapped(thing, "clicked", G_CALLBACK(play_clicked), t);

    t->stop = thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), gui_get_pixmap(GUI_PIXMAP_STOP));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    g_signal_connect_swapped(thing, "clicked", G_CALLBACK(stop_clicked), t);

    t->period_label = thing = gtk_label_new(_("Period"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(thing, FALSE);
    t->period_label2 = thing = gtk_label_new(_("s"));
    gtk_box_pack_end(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(thing, FALSE);
    t->period_spin = thing = extspinbutton_new(
        GTK_ADJUSTMENT(gtk_adjustment_new(1.0, 0.0, 10.0, 0.1, 1.0, 0.0)),
        0.0, 1, TRUE);
    gtk_box_pack_end(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(thing, FALSE);
    g_signal_connect(thing, "value-changed", G_CALLBACK(period_changed), t);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    thing = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

    thing = gtk_label_new(_("Mode"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

    list_store = gtk_list_store_new(1, G_TYPE_STRING);
    for (i = 0; i < MODE_LAST; i++) {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, _((t->is_ref ? mode2 : mode1)[i]), -1);
    }
    thing = gui_combo_new(list_store);
    gtk_combo_box_set_active(GTK_COMBO_BOX(thing), 0);
    gtk_box_pack_end(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "changed", G_CALLBACK(mode_changed), t);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    thing = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

    t->note2_label = thing = gtk_label_new(_("Note"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

    thing = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(thing), GTK_SHADOW_IN);
    gtk_box_pack_end(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    t->note_label = gtk_label_new("");
    gui_get_pixel_size(t->note_label, " C#4 ", &width, &height);
    gtk_widget_set_size_request(t->note_label, width, -1);
    gtk_container_add(GTK_CONTAINER(thing), t->note_label);

    t->note_spin = thing = extspinbutton_new(
        GTK_ADJUSTMENT(gtk_adjustment_new(48.0, 0.0, NUM_NOTES - 1, 1.0, 5.0, 0.0)),
        0.0, 0, TRUE);
    gtk_box_pack_end(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    g_signal_connect(thing, "value-changed", G_CALLBACK(note_changed), t);
    note_changed(GTK_SPIN_BUTTON(thing), t);

    return vbox;
}

static void
mixlevel_changed(GtkAdjustment* adj)
{
    mixlevel = gtk_adjustment_get_value(adj);
}

static void
same_sample_warning(void)
{
    static GtkWidget* dialog = NULL;

    gui_warning_dialog(&dialog, _("Mixing a sample with itself is not a useful idea..."), FALSE);
}

static void
stereo_mono_warning(void)
{
    static GtkWidget* dialog = NULL;

    gui_warning_dialog(&dialog, _("Mixing stereo and mono samples is not implemented. "
        "Convert both samples to either mono or stereo and then mix them."), FALSE);
}

static void
mix_samples(const STSample* dst,
    STSample* sample,
    STSample* sample1,
    const guint32 length1,
    const guint32 length2,
    const gfloat mlevel)
{
    guint32 i, tmp, itmp;
    gint max_value = 0;
    gfloat mlevel1, norm_level = 100.0;
    STSample *rest;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(norm_check))) {
        /* Here we check the actual maximum value to avoid intermediate operations
           which may lead to accuracy lost */
        for (i = 0; i < MIN(length1, length2); i++) {
            gint tmp = abs(lrintf((float)sample->sample.data[i] * mlevel / 100.0 +
                (float)sample1->sample.data[i] * (100.0 - mlevel) / 100.0));
            if (tmp > max_value)
                max_value = tmp;
        }
        if (sample->sample.flags & ST_SAMPLE_STEREO)
            for (i = 0; i < MIN(length1, length2); i++) {
                gint tmp = abs(lrintf((float)sample->sample.data[i + sample->sample.length] * mlevel / 100.0 +
                    (float)sample1->sample.data[i + sample1->sample.length] * (100.0 - mlevel) / 100.0));
                if (tmp > max_value)
                    max_value = tmp;
            }
        if (length1 < length2) {
            mlevel1 = 100.0 - mlevel;
            rest = sample1;
        } else {
            rest = sample;
            mlevel1 = mlevel;
        }
        itmp = i;
        for (; i < MAX(length1, length2); i++) {
            tmp = lrintf((float)rest->sample.data[i] * mlevel1 / 100.0);
            if (tmp > max_value)
                max_value = tmp;
        }
        if (sample->sample.flags & ST_SAMPLE_STEREO)
            for (i = itmp; i < MAX(length1, length2); i++) {
                tmp = lrintf((float)rest->sample.data[i + rest->sample.length] * mlevel1 / 100.0);
                if (tmp > max_value)
                    max_value = tmp;
            }

        if (max_value)
            norm_level = max_value / 327.67; /* 100.0 for full-scale */
    }

    for (i = 0; i < MIN(length1, length2); i++)
        dst->sample.data[i] = lrintf((float)sample->sample.data[i] * mlevel / norm_level +
            (float)sample1->sample.data[i] * (100.0 - mlevel) / norm_level);
    if (sample->sample.flags & ST_SAMPLE_STEREO)
        for (i = 0; i < MIN(length1, length2); i++)
            dst->sample.data[i + dst->sample.length] =
                lrintf((float)sample->sample.data[i + sample->sample.length] * mlevel / norm_level +
                (float)sample1->sample.data[i + sample1->sample.length] * (100.0 - mlevel) / norm_level);

    if (length1 < length2) {
        mlevel1 = 100.0 - mlevel;
        rest = sample1;
    } else {
        rest = sample;
        mlevel1 = mlevel;
    }

    itmp = i;
    for (; i < MAX(length1, length2); i++)
        dst->sample.data[i] = lrintf((float)rest->sample.data[i] * mlevel1 / norm_level);
    if (sample->sample.flags & ST_SAMPLE_STEREO)
        for (i = itmp; i < MAX(length1, length2); i++)
            dst->sample.data[i + dst->sample.length] =
                lrintf((float)rest->sample.data[i + rest->sample.length] * mlevel1 / norm_level);
}

static void
mix_clicked(void)
{
    STSample* sample = &xm->instruments[curi].samples[curs];
    STSample* sample1 = &xm->instruments[desti].samples[dests];
    gsize length1 = sample->sample.length, length2 = sample1->sample.length, loglen;

    /* Silently ignore empty samples */
    if ((!sample->sample.data) || (!sample1->sample.data))
        return;
    if ((!length1) || (!length2))
        return;

    if ((sample->sample.flags & ST_SAMPLE_STEREO) !=
        (sample1->sample.flags & ST_SAMPLE_STEREO)) {
        stereo_mono_warning();
        return;
    }
    if (curi == desti && curs == dests) {
        same_sample_warning();
        return;
    }

    loglen = MAX(length1, length2);
    if (sample->sample.flags & ST_SAMPLE_STEREO)
        loglen <<= 1;

    if (!sample_editor_check_and_log_sample(sample, N_("Mixing samples"),
            HISTORY_FLAG_LOG_INS | HISTORY_FLAG_LOG_SMP |
                HISTORY_SET_PAGE(NOTEBOOK_PAGE_SAMPLE_EDITOR), loglen))
        return;

    g_mutex_lock(&sample->sample.lock);
    if (length1 < length2) {
        gsize alloc_len = length2 * sizeof(sample->sample.data[0]);

        if (sample->sample.flags & ST_SAMPLE_STEREO)
            alloc_len <<= 1;
        sample->sample.data = realloc(sample->sample.data, alloc_len);
        if (sample->sample.flags & ST_SAMPLE_STEREO)
            memmove(&sample->sample.data[length2], &sample->sample.data[sample->sample.length],
                sample->sample.length * sizeof(sample->sample.data[0]));
        sample->sample.length = length2;
    }
    mix_samples(sample, sample, sample1, length1, length2, mixlevel);

    g_mutex_unlock(&sample->sample.lock);
    sample_editor_update();
}

static void
mixer_preview(void)
{
    STSample* sample = &xm->instruments[curi].samples[curs];
    STSample* sample1 = &xm->instruments[desti].samples[dests];
    gsize length1 = sample->sample.length, length2 = sample1->sample.length, length, alloc_length;
    gpointer sampledata;
    static gsize prev_alloc_length = 0;

    /* Silently ignore empty samples */
    if ((!sample->sample.data) || (!sample1->sample.data))
        return;
    if ((!length1) || (!length2))
        return;

    if ((sample->sample.flags & ST_SAMPLE_STEREO) !=
        (sample1->sample.flags & ST_SAMPLE_STEREO)) {
        stereo_mono_warning();
        return;
    }
    if (curi == desti && curs == dests) {
        same_sample_warning();
        return;
    }

    length = MAX(length1, length2);
    alloc_length = length;
    if (sample->sample.flags & ST_SAMPLE_STEREO)
        alloc_length <<= 1;
    /* Indeed, we need to update the sample data only if either a mixlevel or one of the samples
       is changed (instrument/sample number or the sample data itself. But tracking the sample
       data modifications (by editing, loading new samples / instrumens / module) is rather
       complicated. So we update the sample data all the time though... */
    if (!prev_sample.sample.data) {
        prev_sample.sample.data = malloc(alloc_length * sizeof(sample->sample.data[0]));
        prev_alloc_length = alloc_length;
    } else if (prev_alloc_length < alloc_length) {
        g_free(prev_sample.sample.data);
        prev_sample.sample.data = malloc(alloc_length * sizeof(sample->sample.data[0]));
        prev_alloc_length = alloc_length;
    }

    sampledata = prev_sample.sample.data;
    prev_sample = *sample;
    prev_sample.sample.data = sampledata;
    prev_sample.sample.length = length;
    mix_samples(&prev_sample, sample, sample1, length1, length2, mixlevel);

    gui_play_stop();
    gui_play_note_full(0, mix_prev_note + 1, &prev_sample, 0, prev_sample.sample.length, FALSE, -1, -1);
}

void modinfo_page_create(GtkNotebook* nb)
{
    GtkWidget *hbox, *thing, *vbox, *expndr, *hbox2, *vbox2, *hbox3, *alignment, *frame, *prev_note_spin;
    GtkAdjustment *adj;
    GtkListStore* list_store;
    GtkTreeIter iter;
    GtkTreeModel* model;
    const gchar* ititles[3] = { "n", N_("Instrument Name"), N_("#smpl") };
    const gchar* stitles[2] = { "n", N_("Sample Name") };
    GType itypes[3] = { G_TYPE_INT, G_TYPE_STRING, G_TYPE_INT };
    GType stypes[2] = { G_TYPE_INT, G_TYPE_STRING };
    const gfloat ialignments[3] = { 0.5, 0.0, 0.5 };
    const gfloat salignments[2] = { 0.5, 0.0 };
    const gboolean iexpands[3] = { FALSE, TRUE, FALSE };
    const gboolean sexpands[3] = { FALSE, TRUE };
    const gchar* freqlabels[] = { N_("Linear"), N_("Amiga") };
    gint i, width, height;

    vbox = gtk_vbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_notebook_append_page(nb, vbox, gtk_label_new(_("Module Info")));
    gtk_widget_show(vbox);

    hbox = gtk_hbox_new(TRUE, 10);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    /* Main instruments and samples lists */
    ilist = gui_list_in_scrolled_window_full(3, ititles, hbox, itypes, ialignments,
        iexpands, GTK_SELECTION_BROWSE, TRUE, TRUE, &thing,
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_show_all(thing);
    list_store = GUI_GET_LIST_STORE(ilist);
    model = gui_list_freeze(ilist);
    for (i = 1; i <= XM_NUM_INSTRUMENTS; i++) {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, i, 1, "", 2, 0, -1);
    }
    gui_list_thaw(ilist, model);
    gui_list_handle_selection(ilist, G_CALLBACK(ilist_select), NULL);

    slist = gui_list_in_scrolled_window_full(2, stitles, hbox, stypes, salignments,
        sexpands, GTK_SELECTION_BROWSE, TRUE, TRUE, &thing,
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_show_all(thing);
    list_store = GUI_GET_LIST_STORE(slist);
    model = gui_list_freeze(slist);
    for (i = 0; i < XM_NUM_SAMPLES; i++) {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, i, 1, "", -1);
    }
    gui_list_thaw(slist, model);
    gui_list_handle_selection(slist, G_CALLBACK(slist_select), NULL);

    /* Secondary instruments and samples lists for extended functions */
    ilist2 = gui_list_in_scrolled_window_full(3, ititles, hbox, itypes, ialignments,
        iexpands, GTK_SELECTION_BROWSE, TRUE, TRUE, &sw1,
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_show(ilist2);
    list_store = GUI_GET_LIST_STORE(ilist2);
    model = gui_list_freeze(ilist2);
    for (i = 1; i <= XM_NUM_INSTRUMENTS; i++) {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, i, 1, "", 2, 0, -1);
    }
    gui_list_thaw(ilist2, model);
    gui_list_handle_selection(ilist2, G_CALLBACK(ilist2_select), NULL);

    slist2 = gui_list_in_scrolled_window_full(2, stitles, hbox, stypes, salignments,
        sexpands, GTK_SELECTION_BROWSE, TRUE, TRUE, &sw2,
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_show(slist2);
    list_store = GUI_GET_LIST_STORE(slist2);
    model = gui_list_freeze(slist2);
    for (i = 0; i < XM_NUM_SAMPLES; i++) {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, i, 1, "", -1);
    }
    gui_list_thaw(slist2, model);
    gui_list_handle_selection(slist2, G_CALLBACK(slist2_select), NULL);

    /* Module metadata */
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    thing = gtk_label_new(_("Songname:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

    gui_get_text_entry(20, songname_changed, &songname);
    gtk_box_pack_start(GTK_BOX(hbox), songname, TRUE, TRUE, 0);

    add_empty_hbox(hbox);
    thing = make_labelled_radio_group_box(_("Frequencies:"), freqlabels, freqmode_w, freqmode_changed);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);
    add_empty_hbox(hbox);

    ptmode_toggle = gtk_check_button_new_with_label(_("ProTracker Mode"));
    gtk_box_pack_start(GTK_BOX(hbox), ptmode_toggle, FALSE, TRUE, 0);
    gtk_widget_show(ptmode_toggle);
    g_signal_connect(ptmode_toggle, "toggled",
        G_CALLBACK(ptmode_changed), NULL);

    add_empty_hbox(hbox);
    gtk_widget_show_all(hbox);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    /* Extended editor */
    expndr = gtk_expander_new(_("Extended Instrument/Sample Editor"));
    gtk_expander_set_expanded(GTK_EXPANDER(expndr), FALSE);
    gtk_box_pack_end(GTK_BOX(vbox), expndr, FALSE, FALSE, 0);
    g_signal_connect(expndr, "notify::expanded", G_CALLBACK(exp_change), NULL);
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_container_add(GTK_CONTAINER(expndr), hbox);

    vbox = gtk_vbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    ins1_label = add_label(N_("Ins. 1"), "1", vbox);
    gui_get_pixel_size(ins1_label, "000", &width, &height);
    gtk_widget_set_size_request(ins1_label, width, -1);
    smp1_label = add_label(N_("Smp. 1"), "0", vbox);
    gtk_widget_set_size_request(smp1_label, width, -1);
    ins2_label = add_label(N_("Ins. 2"), "1", vbox);
    gtk_widget_set_size_request(ins2_label, width, -1);
    smp2_label = add_label(N_("Smp. 2"), "0", vbox);
    gtk_widget_set_size_request(smp2_label, width, -1);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

    vbox = gtk_vbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    thing = gtk_button_new_with_label(_("I1 => I2"));
    gtk_widget_set_tooltip_text(thing, _("Copy Instrument 1 to Instrument 2"));
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
    g_signal_connect_swapped(thing, "clicked", G_CALLBACK(modinfo_xcopy_ins), GINT_TO_POINTER(0));

    thing = gtk_button_new_with_label(_("I1 <=> I2"));
    gtk_widget_set_tooltip_text(thing, _("Exchange Instruments 1 and 2"));
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
    g_signal_connect_swapped(thing, "clicked", G_CALLBACK(modinfo_xcopy_ins), GINT_TO_POINTER(1));

    thing = gtk_button_new_with_label(_("S1 => S2"));
    gtk_widget_set_tooltip_text(thing, _("Copy Sample 1 to Sample 2"));
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
    g_signal_connect_swapped(thing, "clicked", G_CALLBACK(modinfo_xcopy_smp), GINT_TO_POINTER(0));

    thing = gtk_button_new_with_label(_("S1 <=> S2"));
    gtk_widget_set_tooltip_text(thing, _("Exchange Samples 1 and 2"));
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
    g_signal_connect_swapped(thing, "clicked", G_CALLBACK(modinfo_xcopy_smp), GINT_TO_POINTER(1));

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

    /* Mixer */

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
    thing = gtk_label_new(_("Mixing balance"));
    gtk_misc_set_alignment(GTK_MISC(thing), 0.5, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, TRUE, 0);

    hbox2 = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, TRUE, 0);
    vbox2 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox2), vbox2, TRUE, TRUE, 0);
    hbox3 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox3, FALSE, TRUE, 0);

    mix_label1 = gtk_label_new(_("I1, S0"));
    gtk_box_pack_start(GTK_BOX(hbox3), mix_label1, FALSE, TRUE, 0);

    mix_label2 = gtk_label_new(_("I1, S0"));
    gtk_box_pack_end(GTK_BOX(hbox3), mix_label2, FALSE, TRUE, 0);

    adj = GTK_ADJUSTMENT(gtk_adjustment_new(50.0, 0.0, 100.0, 1.0, 10.0, 0.0));
    thing = gtk_hscale_new(adj);
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox2), thing, FALSE, TRUE, 0);
    g_signal_connect(adj, "value-changed", G_CALLBACK(mixlevel_changed), NULL);

    alignment = gtk_alignment_new(0.5, 1.0, 0.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox2), alignment, FALSE, FALSE, 0);
    hbox3 = gtk_hbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(alignment), hbox3);

    thing = extspinbutton_new(adj, 0.0, 0, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox3), thing, FALSE, FALSE, 0);
    thing = gtk_label_new("%");
    gtk_box_pack_start(GTK_BOX(hbox3), thing, FALSE, FALSE, 0);

    norm_check = gtk_check_button_new();
    gtk_box_pack_start(GTK_BOX(hbox3), norm_check, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(norm_check, _("Normalize mixing result"));
    thing = gtk_label_new(_("N"));
    gtk_box_pack_start(GTK_BOX(hbox3), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Normalize mixing result"));

    hbox2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_end(GTK_BOX(vbox), hbox2, FALSE, TRUE, 0);

    thing = gtk_label_new(_("Note"));
    gtk_widget_set_tooltip_text(thing, _("Note to demonstrate result"));
    gtk_box_pack_start(GTK_BOX(hbox2), thing, FALSE, FALSE, 0);

    prev_note_spin = extspinbutton_new(
        GTK_ADJUSTMENT(gtk_adjustment_new(48.0, 0.0, NUM_NOTES - 1, 1.0, 5.0, 0.0)),
        0.0, 0, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox2), prev_note_spin, FALSE, FALSE, 0);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(hbox2), frame, FALSE, FALSE, 0);
    thing = gtk_label_new("");
    gui_get_pixel_size(thing, " C#4 ", &width, &height);
    gtk_widget_set_size_request(thing, width, -1);
    gtk_container_add(GTK_CONTAINER(frame), thing);
    g_signal_connect(prev_note_spin, "value-changed", G_CALLBACK(mix_prev_note_changed), thing);
    mix_prev_note_changed(GTK_SPIN_BUTTON(prev_note_spin), GTK_LABEL(thing));

    thing = gtk_button_new_with_label(_("Demo"));
    gtk_widget_set_tooltip_text(thing, _("Play the mixed sample without module modification"));
    gtk_box_pack_start(GTK_BOX(hbox2), thing, TRUE, TRUE, 0);
    g_signal_connect(thing, "clicked", G_CALLBACK(mixer_preview), NULL);

    thing = gtk_button_new_with_label(_("Mix!"));
    gtk_widget_set_tooltip_text(thing,
        _("Mix sample 1 of instrument 1 with sample 2 of instrument 2 with the given balance ratio"));
    gtk_box_pack_start(GTK_BOX(hbox2), thing, TRUE, TRUE, 0);
    g_signal_connect(thing, "clicked", G_CALLBACK(mix_clicked), NULL);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);

    /* Tuning facility, tuning sample */
    tuning = tuner_new(FALSE, _("Tuning: "));
    gtk_box_pack_start(GTK_BOX(hbox), tuning->frame, FALSE, FALSE, 0);
    update_labels(tuning, ins1_label, mix_label1, 1, 0, 0);

    hbox2 = gtk_hbox_new(FALSE, 4);
    gtk_container_set_border_width(GTK_CONTAINER(hbox2), 4);
    gtk_container_add(GTK_CONTAINER(tuning->frame), hbox2);
    vbox = gtk_vbox_new(TRUE, 2);
    gtk_box_pack_start(GTK_BOX(hbox2), vbox, FALSE, FALSE, 0);

    thing = gtk_hscale_new(sample_editor_get_adjustment(SAMPLE_EDITOR_FINETUNE));
    gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
    gui_put_labelled_spin_button_with_adj(N_("Finetune"), sample_editor_get_adjustment(SAMPLE_EDITOR_FINETUNE), vbox, TRUE);
    gui_put_labelled_spin_button_with_adj(N_("RelNote"), sample_editor_get_adjustment(SAMPLE_EDITOR_RELNOTE), vbox, TRUE);

    vbox = tuner_populate(tuning);
    gtk_box_pack_start(GTK_BOX(hbox2), vbox, FALSE, FALSE, 0);

    /* Reference sample */
    ref = tuner_new(TRUE, _("Reference: "));
    gtk_box_pack_start(GTK_BOX(hbox), ref->frame, FALSE, FALSE, 0);

    vbox = tuner_populate(ref);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);
    gtk_container_add(GTK_CONTAINER(ref->frame), vbox);
    update_labels(ref, ins2_label, mix_label2, 1, 0, 0);

    gtk_widget_show_all(expndr);
}

gboolean
modinfo_page_handle_keys(int shift,
    int ctrl,
    int alt,
    guint32 keyval,
    gboolean pressed)
{
    gint modifiers = ENCODE_MODIFIERS(shift, ctrl, alt);
    gint i = keys_get_key_meaning(keyval, modifiers, -1);
    gboolean handled = FALSE, samples_focused;

    if (i != -1 && KEYS_MEANING_TYPE(i) == KEYS_MEANING_NOTE) {
        if (tuning->mode == MODE_KEYBOARD && is_tuning) {
            STSample* sample = &xm->instruments[curi].samples[curs];

            handled = gui_play_note_no_repeat(keyval, modifiers, pressed,
                sample, 0, sample->sample.length, 1, FALSE, NULL, TRUE, curi + 1, curs);

            /* handled means that it's a real, not fake, keypress and the note is correct */
            if (ref->mode == MODE_COUPLED && handled) {
                if (pressed) {
                    sample = &xm->instruments[desti].samples[dests];
                    if (sample)
                        gui_play_note_full(0, ref->note + 1, sample, 0, sample->sample.length, FALSE, curi + 1, curs);
                } else
                    gui_stop_note(0);
            }
        } else
            track_editor_do_the_note_key(i, pressed, keyval, ENCODE_MODIFIERS(shift, ctrl, alt), TRUE);
        return TRUE;
    }

    if (!pressed)
        return FALSE;

    samples_focused = (GTK_WINDOW(mainwindow)->focus_widget == slist);
    switch (keyval) {
    case GDK_Tab:
    case GDK_ISO_Left_Tab:
        gtk_window_set_focus(GTK_WINDOW(mainwindow), samples_focused ? ilist : slist);
        handled = TRUE;
        break;
    case GDK_Up:
        if (samples_focused)
            gui_offset_current_sample(shift ? -4 : -1);
        else
            gui_offset_current_instrument(shift ? -5 : -1);
        handled = TRUE;
        break;
    case GDK_Down:
        if (samples_focused)
            gui_offset_current_sample(shift ? 4 : 1);
        else
            gui_offset_current_instrument(shift ? 5 : 1);
        handled = TRUE;
        break;
    case GDK_Delete:
        handled = instrument_editor_clear_current_instrument();
        break;
    default:
        break;
    }
    return handled;
}

void modinfo_stop_cycling(void)
{
    if (tuning->timer != -1) {
        g_source_remove(tuning->timer);
        tuning->timer = -1;
    }
    if (ref->timer != -1) {
        g_source_remove(ref->timer);
        ref->timer = -1;
    }
}

void
modinfo_update_instrument(int n)
{
    modinfo_update_instrument_full(n, FALSE);
    modinfo_update_instrument_full(n, TRUE);
}

void modinfo_update_sample(int n)
{
    modinfo_update_sample_full(curi, n, FALSE);
    modinfo_update_sample_full(curi, n, TRUE);
}

void modinfo_update_all(void)
{
    gint i;
    const gint freq_index = (xm->flags & XM_FLAGS_AMIGA_FREQ) != 0 ? 1 : 0;

    for (i = 0; i < XM_NUM_INSTRUMENTS; i++)
        modinfo_update_instrument(i);

    gtk_entry_set_text(GTK_ENTRY(songname), xm->utf_name);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ptmode_toggle), xm->flags & XM_FLAGS_IS_MOD);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(freqmode_w[freq_index]), TRUE);
}

void modinfo_set_current_instrument(int n)
{
    g_return_if_fail(n >= 0 && n < XM_NUM_INSTRUMENTS);

    gui_list_select(ilist, n, FALSE, 0.0);
    modinfo_update_all_samples(n, FALSE);
}

void modinfo_set_current_sample(int n)
{
    g_return_if_fail(n >= 0 && n < XM_NUM_SAMPLES);

    gui_list_select(slist, n, FALSE, 0.0);
}

gint modinfo_get_current_sample(void)
{
    return curs;
}
