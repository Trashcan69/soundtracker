
/*
 * The Real SoundTracker - Loop Factory
 *
 * Copyright (C) 2022-2023 Yury Aliaev
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

#include <math.h>
#include <stdlib.h>

#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "extspinbutton.h"
#include "gui.h"
#include "gui-settings.h"
#include "history.h"
#include "sample-editor.h"

enum {
    WINDOW_TRIANGLE = 0,
    WINDOW_RECTANGLE,
    WINDOW_GAUSS,
    WINDOW_LAST
};

enum {
    TRANSIENT_LIN = 0,
    TRANSIENT_EXP,
    TRANSIENT_REV_EXP,
    TRANSIENT_LOG,
    TRANSIENT_LAST
};

typedef struct {
    GtkWidget *display, *loop_spin, *ratio_box, *ratio_spin, *sigma_box, *sigma_spin, *sharp_spin;
    GtkWidget *avalue, *bvalue, *window_combo, *transient_combo, *transient_box, *xfade_button, *sharp_box;
    GtkAdjustment *range_sadj, *range_eadj, *area_start_adj, *area_end_adj;
    gint window_shape, transient_shape;
    gboolean is_end;
} LoopFactoryPanel;

static GtkWidget *dialog = NULL;
static LoopFactoryPanel panel0, panel1;

static void
loop_factory_reset(LoopFactoryPanel* panel)
{
    SampleEditorDisplay* s;
    SampleDisplay* sd;
    gint w, x_start, x_end;

    s = SAMPLE_EDITOR_DISPLAY(panel->display);
    sd = SAMPLE_DISPLAY(panel->display);
    w = MIN(sd->width, s->sample->sample.length);

    x_start = (panel->is_end ? s->sample->sample.loopend : s->sample->sample.loopstart) - (w >> 1);
    if (x_start < 0)
        x_start = 0;
    x_end = x_start + w;
    if (x_end >= s->sample->sample.length) {
        x_end = s->sample->sample.length;
        x_start = x_end - w;
    }

    sample_display_set_window(sd, x_start, x_end);
}

static void
display_loop_changed(SampleEditorDisplay* sd,
    gint start,
    gint end)
{
    g_return_if_fail(sd->sample != NULL);
    g_return_if_fail(sd->sample->sample.data != NULL);
    g_return_if_fail(IS_SAMPLE_LOOPED(sd->sample->sample));
    g_return_if_fail(start < end);

    if (start != sd->sample->sample.loopstart)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(panel0.loop_spin), start);
    if (end != sd->sample->sample.loopend)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(panel1.loop_spin), end);
}

static void
update_loop_value(LoopFactoryPanel* panel)
{
    gchar* label;
    st_mixer_sample_info* smp = &SAMPLE_EDITOR_DISPLAY(panel->display)->sample->sample;
    guint32 i = panel->is_end ? smp->loopend : smp->loopstart;

    /* The loop point can be the first discrete of a sample */
    label = (i == 0 ? g_strdup_printf("\342\200\223") : g_strdup_printf("%i", smp->data[i - 1]));
    gtk_label_set_text(GTK_LABEL(panel->bvalue), label);
    g_free(label);

    /* Next to the loop end value can be outside sample data */
    label = (i == smp->length ? g_strdup_printf("\342\200\223") : g_strdup_printf("%i", smp->data[i]));
    gtk_label_set_text(GTK_LABEL(panel->avalue), label);
    g_free(label);
}

static void
loop_spin_changed(LoopFactoryPanel* panel)
{
    /* A crutch because these spin buttons share adjustments with the Sample Editor
       ones, but the sample is updated only when the Loop Factory window is visible */
    if (dialog && gtk_widget_get_visible(dialog)) {
        gint start = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(panel0.loop_spin));
        gint end = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(panel1.loop_spin));

        if (end <= SAMPLE_EDITOR_DISPLAY(panel->display)->sample->sample.length) {
            sample_display_set_loop(SAMPLE_DISPLAY(panel0.display), start, end);
            sample_display_set_loop(SAMPLE_DISPLAY(panel1.display), start, end);
            update_loop_value(panel);
        }
    }
}

static void
loop_factory_update_status(SampleEditorDisplay* s,
    const gboolean cursor_in,
    const guint32 pos,
    gpointer l)
{
    static gchar status[256];
    gint n = sample_editor_display_print_status(s, cursor_in, pos, status, sizeof(status));

    gtk_label_set_text(GTK_LABEL(l), n ? status : "");
}

static void
loop_factory_panel_set_sample(LoopFactoryPanel* panel,
    STSample* sample,
    const gboolean xfade_active)
{
    SampleDisplay* sd = SAMPLE_DISPLAY(panel->display);
    gboolean new_sample = (SAMPLE_EDITOR_DISPLAY(panel->display)->sample != sample);
    gint ss = sd->sel_start, se = sd->sel_end;

    sample_editor_display_set_sample(SAMPLE_EDITOR_DISPLAY(panel->display), sample);
    if (new_sample) {
        gtk_adjustment_configure(panel->area_start_adj, 0.0, 0.0,
            sample->sample.length - 1, 1.0, 5.0, 0.0);
        gtk_adjustment_configure(panel->area_end_adj, sample->sample.length, 1.0, 
            sample->sample.length, 1.0, 5.0, 0.0);
    } else if (ss != -1)
        sample_display_set_selection(sd, ss, se);

    loop_factory_reset(panel);
    update_loop_value(panel);
    gtk_widget_set_sensitive(panel->xfade_button, xfade_active);
    gtk_widget_set_sensitive(panel->transient_box, xfade_active);
}

static void
loop_factory_range_from_sel(LoopFactoryPanel* panel)
{
    SampleDisplay* sd = SAMPLE_DISPLAY(panel->display);
    SampleEditorDisplay* sed = SAMPLE_EDITOR_DISPLAY(panel->display);
    gint ss = sd->sel_start, se = sd->sel_end;
    gint loop_point = panel->is_end ? sed->sample->sample.loopend : sed->sample->sample.loopstart;

    if (ss == -1)
        return;

    if (loop_point < ss || loop_point > se) {
        static GtkWidget* dialog = NULL;

        gui_warning_dialog(&dialog, _("Loop point must be within the range."), FALSE);

        return;
    }
    gtk_adjustment_set_value(panel->range_sadj, ss - loop_point);
    gtk_adjustment_set_value(panel->range_eadj, se - loop_point);
}

static void
loop_factory_range_from_opp(LoopFactoryPanel* panel)
{
    LoopFactoryPanel* p = (panel == &panel0 ? &panel1 : &panel0);

    gtk_adjustment_set_value(panel->range_sadj, gtk_adjustment_get_value(p->range_sadj));
    gtk_adjustment_set_value(panel->range_eadj, gtk_adjustment_get_value(p->range_eadj));
}

static void
loop_factory_area_start_changed(GtkAdjustment* adj,
    LoopFactoryPanel* panel)
{
    gtk_adjustment_set_lower(panel->area_end_adj,
        lrint(gtk_adjustment_get_value(adj)) + 1.0);
}

static void
loop_factory_area_end_changed(GtkAdjustment* adj,
    LoopFactoryPanel* panel)
{
    gtk_adjustment_set_upper(panel->area_start_adj,
        lrint(gtk_adjustment_get_value(adj)) - 1.0);
}

static void
loop_factory_area_from_sel(LoopFactoryPanel* panel)
{
    gint ss = SAMPLE_DISPLAY(panel->display)->sel_start;
    gint se = SAMPLE_DISPLAY(panel->display)->sel_end;

    if (ss != -1) {
        if (se > lrint(gtk_adjustment_get_value(panel->area_end_adj))) {
            gtk_adjustment_set_value(panel->area_end_adj, se);
            gtk_adjustment_set_value(panel->area_start_adj, ss);
        } else {
            gtk_adjustment_set_value(panel->area_start_adj, ss);
            gtk_adjustment_set_value(panel->area_end_adj, se);
        }
    }
}

static void
window_shape_changed(GtkComboBox* combo,
    LoopFactoryPanel* panel)
{
    gint new_shape = gtk_combo_box_get_active(combo);

    g_return_if_fail(new_shape >= 0 && new_shape < WINDOW_LAST);
    if (new_shape == panel->window_shape)
        return;

    switch (panel->window_shape) {
    case WINDOW_RECTANGLE:
        gtk_widget_hide(panel->ratio_box);
        break;
    case WINDOW_GAUSS:
        gtk_widget_hide(panel->sigma_box);
        break;
    default:
        break;
    }
    switch (new_shape) {
    case WINDOW_RECTANGLE:
        gtk_widget_show(panel->ratio_box);
        break;
    case WINDOW_GAUSS:
        gtk_widget_show(panel->sigma_box);
        break;
    default:
        break;
    }

    panel->window_shape = new_shape;
}

static void
trans_shape_changed(GtkComboBox* combo,
    LoopFactoryPanel* panel)
{
    gint new_shape = gtk_combo_box_get_active(combo);

    g_return_if_fail(new_shape >= 0 && new_shape < TRANSIENT_LAST);
    if (new_shape == panel->transient_shape)
        return;

    (new_shape == TRANSIENT_LIN ? gtk_widget_hide : gtk_widget_show)(panel->sharp_box);
    panel->transient_shape = new_shape;
}

static void
loop_factory_update_sampledata(LoopFactoryPanel* panel,
    STSample* newsample)
{
    SampleDisplay* sd = SAMPLE_DISPLAY(panel->display);
    STSample* oldsample = SAMPLE_EDITOR_DISPLAY(panel->display)->sample;

    if (oldsample == newsample) {
        /* Sample is not changed, just update data while preserving loop and selection
           (if possible) */
        st_mixer_sample_info* si = &oldsample->sample;
        gint win_start = sd->win_start;
        gint win_end = win_start + sd->win_length;
        gint sel_start = sd->sel_start;
        gint sel_end = sd->sel_end;

        sample_display_set_data(sd, si->data, ST_MIXER_FORMAT_S16_LE, si->length, FALSE);
        sample_display_set_loop(sd, si->loopstart, si->loopend);
        if (win_end < si->length)
            sample_display_set_window(sd, win_start, win_end);
        else
            loop_factory_reset(panel);
        if (sel_start != -1)
            sample_display_set_selection(sd, sel_start, sel_end);
    } else
        loop_factory_panel_set_sample(panel, newsample, panel->is_end ||
            IS_SAMPLE_PINGPONG(newsample->sample));
}

static void
update_sample_notify_func(STSample* s)
{
    if (dialog && gtk_widget_get_visible(dialog)) {
        if (!IS_SAMPLE_LOOPED(s->sample))
            /* No loop --- no factory! */
            gtk_widget_hide(dialog);
        else {
            loop_factory_update_sampledata(&panel0, s);
            loop_factory_update_sampledata(&panel1, s);
        }
    }
}

static void
loop_factory_xfade_loop_point(LoopFactoryPanel* panel)
{
    SampleEditorDisplay* sed = SAMPLE_EDITOR_DISPLAY(panel->display);
    gfloat range_start = gtk_adjustment_get_value(panel->range_sadj);
    gfloat range_end = gtk_adjustment_get_value(panel->range_eadj);
    gint irange_start, irange_end;
    gint shape = gtk_combo_box_get_active(GTK_COMBO_BOX(panel->transient_combo));
    gfloat s = gtk_spin_button_get_value(GTK_SPIN_BUTTON(panel->sharp_spin));
    gint i;
    gfloat mul, mul2, k = 1.0, k2 = 1.0, range;
    st_mixer_sample_info* si = &sed->sample->sample;
    static GtkWidget* dialog = NULL;

    if ((si->flags & ST_SAMPLE_LOOPTYPE_AMIGA) || panel->is_end) {
        irange_start = lrint(range_start);
        irange_end = 0;
        range = range_start;
        if (si->flags & ST_SAMPLE_LOOPTYPE_AMIGA) {
            if (-irange_start > si->loopstart) {
                gui_warning_dialog(&dialog, _("Not enough data for crossfading.\n"
                    "Decrase range or move the other point away from the sample start."), TRUE);

                return;
            }
        } else if (-irange_start > si->length - si->loopend) {
            gui_warning_dialog(&dialog, _("Not enough data for crossfading.\n"
                "Decrase range or move the loop point away from the sample end."), TRUE);

            return;
        }
    } else {
        /* Start of the pingpong loop */
        irange_start = 0;
        irange_end = lrint(range_end);
        range = range_end;
        if (irange_end > si->loopstart) {
            gui_warning_dialog(&dialog, _("Not enough data for crossfading.\n"
                "Decrase range or move the loop point away from the sample start."), TRUE);

            return;
        }
    }
    if (!sample_editor_check_and_log_sample(sed->sample,
        N_("Crossfading around a loop point"), HISTORY_FLAG_LOG_ALL, 0))
        return;

    switch (shape) {
    case TRANSIENT_EXP:
    case TRANSIENT_REV_EXP:
        k2 = exp(-s * 0.5);
        k = (1.0 - k2);
        break;
    case TRANSIENT_LOG:
        k = 1.0 / (1.0 + exp(2.0 * s));
        break;
    default: /* Linear */
        break;
    }

    for (i = irange_start; i < irange_end; i++) {
        mul2 = (gfloat)i / range;

        switch (shape) {
        case TRANSIENT_EXP:
            mul = (exp((mul2 - 1.0) * 0.5) - k2) / k;
            break;
        case TRANSIENT_REV_EXP:
            mul = (1.0 - exp(-mul2 * 0.5)) / k;
            break;
        case TRANSIENT_LOG:
            mul = (1.0 / (1.0 + exp(-2.0 * s * (mul2 - 0.5))) - k) / (1.0 - 2.0 * k);
            break;
        default: /* Linear */
            mul = mul2;
            break;
        }

        if (si->flags & ST_SAMPLE_LOOPTYPE_AMIGA)
            si->data[si->loopend + i] = lrint((gfloat)si->data[si->loopend + i] * mul +
                (gfloat)si->data[si->loopstart + i] * (1.0 - mul));
        else {
            if (panel->is_end)
                si->data[si->loopend + i] = lrint((gfloat)si->data[si->loopend + i] * mul +
                    (gfloat)si->data[si->loopend - i] * (1.0 - mul));
            else
                si->data[si->loopstart + i] = lrint((gfloat)si->data[si->loopstart + i] * mul +
                    (gfloat)si->data[si->loopstart - i] * (1.0 - mul));
        }
    }
    sample_editor_update();
}

static void
loop_factory_opt_loop_point(LoopFactoryPanel* panel)
{
    SampleEditorDisplay* s = SAMPLE_EDITOR_DISPLAY(panel->display);
    gint pos_min, pos_max, x, x_opt;
    gint area_min = lrint(gtk_adjustment_get_value(panel->area_start_adj));
    gint area_max = lrint(gtk_adjustment_get_value(panel->area_end_adj));
    gint range_start = lrint(gtk_adjustment_get_value(panel->range_sadj));
    gint range_end = lrint(gtk_adjustment_get_value(panel->range_eadj));
    /* This coefficient is calculated so that the attenuation at the range edge to be
       equal to the spin button parameter in dB */
    gint range_max = MAX(-range_start, range_end);
    gfloat sigma2 = 4.342944819 / gtk_spin_button_get_value(GTK_SPIN_BUTTON(panel->sigma_spin)) *
        (gfloat)(range_max * range_max);
    gint ref_pos = panel->is_end ? s->sample->sample.loopstart : s->sample->sample.loopend;
    gfloat corr_max;
    gint shape = gtk_combo_box_get_active(GTK_COMBO_BOX(panel->window_combo));
    static GtkWidget* dialog = NULL;

    if (panel->is_end) {
        pos_min = MAX(area_min, s->sample->sample.loopstart);
        pos_max = area_max;
    } else {
        pos_min = area_min;
        pos_max = MIN(area_max, s->sample->sample.loopend);
    }
    if (pos_min >= pos_max) {
        gui_warning_dialog(&dialog, _("Incorrect search area! Loop points can't jump one over another one."), TRUE);

        return;
    }
    if (!range_start && !range_end) {
        gui_warning_dialog(&dialog, _("Empty range."), TRUE);

        return;
    }

    for (x = pos_min, x_opt = pos_min, corr_max = 0.0; x <= pos_max; x++) {
        gint j;
        gfloat corr = 0.0;
        gint jmin, jmax;

        if (IS_SAMPLE_FORWARD(s->sample->sample)) {
            jmin = MAX(range_start, -MIN(x, ref_pos));
            jmax = MIN(range_end, s->sample->sample.length - MAX(x, ref_pos));
        } else {
            jmin = 1;
            jmax = MIN(x, MIN(range_max, s->sample->sample.length - x));
        }

        for (j = jmin; j < jmax; j++) {
            gint16 a = s->sample->sample.data[x + j];
            gint16 b = s->sample->sample.data[IS_SAMPLE_FORWARD(s->sample->sample) ?
                ref_pos + j : x - j];
            gfloat val = (a == b) ? 1.0 : 1.0 - 0.5 * fabsf(a - b) / (gfloat)MAX(abs(a), abs(b));
            gint jcur;
            gfloat mul;

            switch (shape) {
            default: /* Triangle, no parameters */
                jcur = j < 0 ? jmin : (j > 0 ? jmax : 1);
                mul = ((gfloat)jcur - (gfloat)j) / (gfloat)jcur;
                break;
            case WINDOW_RECTANGLE: /* Peak-to-pedestal ratio does nothing with pingpong loop */
                if (!j)
                    mul = powf(10.0,
                        gtk_spin_button_get_value(GTK_SPIN_BUTTON(panel->ratio_spin)) / 20.0);
                else
                    mul = 1.0;
                break;
            case WINDOW_GAUSS:
                mul = expf(-(gfloat)(j * j) / (2.0 * sigma2));
                break;
            }

            corr += val * mul;
        }
        if (corr > corr_max) {
            corr_max = corr;
            x_opt = x;
        }
    }

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(panel->loop_spin), x_opt);
    loop_factory_reset(panel);
}

static void
loop_factory_make_panel(LoopFactoryPanel* panel,
    GtkWidget* box,
    GtkWidget* status_label,
    const gboolean end)
{
    static const gchar* shape_labels[] = {N_("Triangle"), N_("Rectangle"), N_("Gaussian")};
    static const gchar* transient_labels[] = {N_("Linear"), N_("Exponent"),
        N_("Reverse Exponent"), N_("Logistic")};
    GtkWidget *vbox, *hscroll, *thing, *hbox, *btn;
    GtkListStore* list_store;
    GtkTreeIter iter;
    gint i, w, h;

    panel->window_shape = 0;
    panel->transient_shape = 0;
    panel->is_end = end;
    vbox = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(box), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    hscroll = gtk_hscrollbar_new(NULL);
    thing = sample_editor_display_new(hscroll, TRUE, gui_settings.editor_mode,
        loop_factory_update_status, status_label);
    panel->display = thing;
    sample_display_enable_zero_line(SAMPLE_DISPLAY(thing), TRUE);
    g_signal_connect(thing, "loop_changed",
        G_CALLBACK(display_loop_changed), NULL);
    gtk_widget_set_size_request(thing, 300, 200);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    btn = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(btn), gui_get_pixmap(GUI_PIXMAP_LOOP_FACTORY_RESET));
    gtk_widget_set_tooltip_text(btn, _("Reset to the loop point"));
    gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 0);
    g_signal_connect_swapped(btn, "clicked",
        G_CALLBACK(loop_factory_reset), panel);

    btn = gui_button(GTK_STOCK_ZOOM_IN, G_CALLBACK(sample_editor_display_zoom_in), thing, hbox, TRUE, FALSE);
    gtk_widget_set_tooltip_text(btn, _("Zoom In"));
    btn = gui_button(GTK_STOCK_ZOOM_OUT, G_CALLBACK(sample_editor_display_zoom_out), thing, hbox, TRUE, FALSE);
    gtk_widget_set_tooltip_text(btn, _("Zoom Out"));

    btn = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(btn),
        sample_editor_get_widget(end ? "show-sel1" : "show-sel0"));
    gtk_widget_set_tooltip_text(btn, _("Zoom to Selection"));
    gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 0);
    g_signal_connect_swapped(btn, "clicked",
        G_CALLBACK(sample_editor_display_zoom_to_selection), thing);

    btn = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(btn),
        sample_editor_get_widget(end ? "select-all1" : "select-all0"));
    gtk_widget_set_tooltip_text(btn, _("Select All"));
    gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 0);
    g_signal_connect_swapped(btn, "clicked",
        G_CALLBACK(sample_editor_display_select_all), thing);

    btn = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(btn),
        sample_editor_get_widget(end ? "select-none1" : "select-none0"));
    gtk_widget_set_tooltip_text(btn, _("Select None"));
    gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 0);
    g_signal_connect_swapped(btn, "clicked",
        G_CALLBACK(sample_editor_display_select_none), thing);
    gtk_widget_show_all(hbox);

    gtk_box_pack_start(GTK_BOX(vbox), thing, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hscroll, FALSE, FALSE, 0);
    gtk_widget_show(thing);
    gtk_widget_show(hscroll);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    thing = gui_put_labelled_spin_button_with_adj(end ? N_("Loop end") : N_("Loop start"),
        sample_editor_get_adjustment(end ? SAMPLE_EDITOR_LOOPEND : SAMPLE_EDITOR_LOOPSTART), hbox, FALSE);
    g_signal_connect_swapped(thing, "value-changed",
        G_CALLBACK(loop_spin_changed), panel);
    panel->loop_spin = thing;

    thing = gtk_label_new(_("Value before:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    thing = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    panel->bvalue = gtk_label_new(NULL);
    gui_get_pixel_size(panel->bvalue, "-32768", &w, &h);
    gtk_widget_set_size_request(panel->bvalue, w, -1);
    gtk_misc_set_alignment(GTK_MISC(panel->bvalue), 1.0, 0.5);
    gtk_container_add(GTK_CONTAINER(thing), panel->bvalue);
    thing = gtk_label_new(_("after:"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    thing = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    panel->avalue = gtk_label_new(NULL);
    gtk_widget_set_size_request(panel->avalue, w, -1);
    gtk_misc_set_alignment(GTK_MISC(panel->avalue), 1.0, 0.5);
    gtk_container_add(GTK_CONTAINER(thing), panel->avalue);
    gtk_widget_show_all(hbox);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(hbox, _("Correlation Range"));

    panel->range_sadj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, -10000.0, 0.0, 1.0, 5.0, 0.0));
    thing = gui_put_labelled_spin_button_with_adj(N_("Range"), panel->range_sadj, hbox, FALSE);
    panel->range_eadj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 10000.0, 1.0, 5.0, 0.0));
    thing = gui_put_labelled_spin_button_with_adj("\342\200\223", panel->range_eadj, hbox, FALSE);

    thing = gtk_button_new_with_label(_("From Sel"));
    gtk_box_pack_end(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    gtk_widget_set_tooltip_text(thing, _("Set Range from Selection"));
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(loop_factory_range_from_sel), panel);
    gtk_widget_show_all(hbox);

    thing = gtk_button_new_with_label(_("From Opposite"));
    gtk_box_pack_end(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    gtk_widget_set_tooltip_text(thing, _("Take Range from the Opposite Panel"));
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(loop_factory_range_from_opp), panel);
    gtk_widget_show_all(hbox);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(hbox, _("Search Area"));
    panel->area_start_adj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 1.0, 1.0, 1.0, 5.0, 0.0));
    thing = gui_put_labelled_spin_button_with_adj(N_("Area"), panel->area_start_adj, hbox, FALSE);

    panel->area_end_adj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 1.0, 1.0, 1.0, 5.0, 0.0));
    g_signal_connect(panel->area_end_adj, "value-changed",
        G_CALLBACK(loop_factory_area_end_changed), panel);
    g_signal_connect(panel->area_start_adj, "value-changed",
        G_CALLBACK(loop_factory_area_start_changed), panel);
    thing = gui_put_labelled_spin_button_with_adj("\342\200\223", panel->area_end_adj, hbox, FALSE);

    thing = gtk_button_new_with_label(_("From Sel"));
    gtk_box_pack_end(GTK_BOX(hbox), thing, TRUE, TRUE, 0);
    gtk_widget_set_tooltip_text(thing, _("Set Area from Selection"));
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(loop_factory_area_from_sel), panel);
    gtk_widget_show_all(hbox);

    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);
    thing = gtk_label_new(_("Window"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    list_store = gtk_list_store_new(1, G_TYPE_STRING);
    for (i = 0; i < WINDOW_LAST; i++) {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, _(shape_labels[i]), -1);
    }
    panel->window_combo = thing = gui_combo_new(list_store);
    gtk_combo_box_set_active(GTK_COMBO_BOX(thing), 0);
    g_signal_connect(thing, "changed", G_CALLBACK(window_shape_changed), panel);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    panel->ratio_box = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), panel->ratio_box, FALSE, FALSE, 0);
    thing = gtk_label_new(_("Ratio"));
    gtk_box_pack_start(GTK_BOX(panel->ratio_box), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);
    gtk_widget_set_tooltip_text(panel->ratio_box, _("Peak-to-Pedestal Ratio"));
    panel->ratio_spin = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 60.0, 1.0, 5.0, 0.0)),
        0, 0, FALSE);
    gtk_box_pack_start(GTK_BOX(panel->ratio_box), panel->ratio_spin, FALSE, FALSE, 0);
    gtk_widget_show(panel->ratio_spin);
    thing = gtk_label_new(_("dB"));
    gtk_box_pack_start(GTK_BOX(panel->ratio_box), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    panel->sigma_box = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), panel->sigma_box, FALSE, FALSE, 0);
    thing = gtk_label_new(_("Sigma"));
    gtk_widget_show(thing);
    gtk_widget_set_tooltip_text(panel->sigma_box, _("Attenuation at the range edge"));
    gtk_box_pack_start(GTK_BOX(panel->sigma_box), thing, FALSE, FALSE, 0);
    panel->sigma_spin = extspinbutton_new(GTK_ADJUSTMENT(gtk_adjustment_new(20.0, 6.0, 60.0, 1.0, 5.0, 0.0)),
        0, 0, FALSE);
    gtk_box_pack_start(GTK_BOX(panel->sigma_box), panel->sigma_spin, FALSE, FALSE, 0);
    gtk_widget_show(panel->sigma_spin);
    thing = gtk_label_new(_("dB"));
    gtk_box_pack_start(GTK_BOX(panel->sigma_box), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    thing = gtk_button_new_with_label(_("Optimize"));
    gtk_widget_set_tooltip_text(thing, _("Optimize Loop Point Position"));
    gtk_box_pack_end(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(loop_factory_opt_loop_point), panel);

    thing = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);

    panel->transient_box = hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);
    thing = gtk_label_new(_("Transient"));
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Transient Curve Shape"));
    gtk_widget_show(thing);

    list_store = gtk_list_store_new(1, G_TYPE_STRING);
    for (i = 0; i < TRANSIENT_LAST; i++) {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, 0, _(transient_labels[i]), -1);
    }
    panel->transient_combo = thing = gui_combo_new(list_store);
    gtk_combo_box_set_active(GTK_COMBO_BOX(thing), 0);
    g_signal_connect(thing, "changed", G_CALLBACK(trans_shape_changed), panel);
    gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(thing, _("Transient Curve Shape"));
    gtk_widget_show(thing);

    panel->sharp_box = gui_labelled_spin_button_new(N_("Sharpness"), 1.0, 10.0, &panel->sharp_spin,
        NULL, NULL, FALSE, NULL);
    gtk_box_pack_start(GTK_BOX(hbox), panel->sharp_box, FALSE, FALSE, 0);

    panel->xfade_button = thing = gtk_button_new_with_label(_("Crossfade"));
    gtk_widget_set_tooltip_text(thing, _("Crossfade sample around the loop point"));
    gtk_box_pack_end(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(loop_factory_xfade_loop_point), panel);
}

void loop_factory_dialog(void)
{
    STSample* sample;

    if (!dialog) {
        static GtkWidget *status_label;
        GtkWidget *vbox, *hbox, *thing;

        dialog = gtk_dialog_new_with_buttons(_("Loop Factory"), GTK_WINDOW(mainwindow),
            GTK_DIALOG_MODAL, GTK_STOCK_CLOSE, 0, NULL);
        gtk_container_set_border_width(GTK_CONTAINER(dialog), 4);
        gui_dialog_connect(dialog, NULL);
        gui_translate_keyevents(dialog, mainwindow, TRUE);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        gtk_box_set_spacing(GTK_BOX(vbox), 4);
        gtk_widget_show(vbox);
        hbox = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
        gtk_widget_show(hbox);

        status_label = gtk_label_new("");
        gtk_misc_set_alignment(GTK_MISC(status_label), 0.0, 0.5);
        gtk_misc_set_padding(GTK_MISC(status_label), 0, 4);
        loop_factory_make_panel(&panel0, hbox, status_label, FALSE);
        thing = gtk_vseparator_new();
        gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
        gtk_widget_show(thing);
        loop_factory_make_panel(&panel1, hbox, status_label, TRUE);

        thing = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(thing), GTK_SHADOW_IN);
        gtk_container_add(GTK_CONTAINER(thing), status_label);
        gtk_widget_show_all(thing);
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);

        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
        gtk_widget_show(thing);

        sample_editor_notify_update(update_sample_notify_func);
    }

    gtk_window_present(GTK_WINDOW(dialog));
    sample = sample_editor_get_sample();
    loop_factory_panel_set_sample(&panel0, sample, IS_SAMPLE_PINGPONG(sample->sample));
    loop_factory_panel_set_sample(&panel1, sample, TRUE);
}

void
loop_factory_set_mode(const SampleDisplayMode mode)
{
    if (dialog) {
        sample_display_set_mode(SAMPLE_DISPLAY(panel0.display), mode);
        gtk_widget_queue_draw(panel0.display);
        sample_display_set_mode(SAMPLE_DISPLAY(panel1.display), mode);
        gtk_widget_queue_draw(panel1.display);
    }
}
