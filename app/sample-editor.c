
/*
 * The Real SoundTracker - sample editor
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

#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <math.h>

#if USE_SNDFILE
#include <sndfile.h>
#elif AUDIOFILE_VERSION
#include <audiofile.h>
#endif

#include <gdk/gdkkeysyms.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "audio.h"
#include "clock.h"
#include "colors.h"
#include "draw-interlayer.h"
#include "endian-conv.h"
#include "errors.h"
#include "file-operations.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "gui.h"
#include "extspinbutton.h"
#include "history.h"
#include "instrument-editor.h"
#include "keys.h"
#include "mixer.h"
#include "module-info.h"
#include "sample-display.h"
#include "sample-editor.h"
#include "sample-editor-extensions.h"
#include "st-subs.h"
#include "time-buffer.h"
#include "track-editor.h"
#include "xm.h"

// == GUI variables

/* Dimensions and position of the record indicator:
    size = MAX(MIN_SIZE, MIN(window_width, window_height) / FRACTION_S)
    space between window sides and the indicator is calculated similarly
    to that in scope-group.h (with FRACRION_D as FRACTION) */
static const guint MIN_SIZE = 10, XDISP = 3, YDISP = 3, FRACTION_S = 20, FRACTION_D = 100;

static STSample* tmp_sample = NULL;

static GtkWidget* se_spins[SAMPLE_EDITOR_SPIN_LAST];
static SampleDisplay* sampledisplay;
static SampleEditorDisplay* sed;
static GtkWidget* sample_editor_hscrollbar;
static GtkWidget *loopradio[3], *resolution_radio[2];
static GSList *list_data_sensitive = NULL, *list_loop_sensitive = NULL;
static GSList *list_stereo_visible = NULL, *list_mono_visible = NULL;
static GSList* sample_update_notify_list = NULL;
static GtkAdjustment* hadj;
static GtkBuilder* builder = NULL;
static void (*last_handler)(void) = NULL;

static gint tags[SAMPLE_EDITOR_SPIN_LAST];

#if USE_SNDFILE || AUDIOFILE_VERSION
static file_op *loadsmp, *savesmp, *savereg;
#endif

static gboolean copy_whole_sample = FALSE;

static GtkWidget *sample_editor_volramp_spin_w[2];

enum {
    MODE_MONO = 0,
    MODE_STEREO_MIX,
    MODE_STEREO_LEFT,
    MODE_STEREO_RIGHT,
    MODE_STEREO_2
};

#if USE_SNDFILE || AUDIOFILE_VERSION
static GtkWidget* wavload_dialog;

struct wl {
    gboolean through_library;
    const char* samplename;

#if USE_SNDFILE
    SNDFILE* file;
    SF_INFO wavinfo;
    sf_count_t frameCount;
#else
    AFfilehandle file;
    AFframecount frameCount;
#endif

    gint sampleWidth, channelCount, rate;
    gboolean src_be, dst_be, unsignedwords;
    gboolean isFloat;
};
#endif

struct _SampleEditorDisplayPriv {
    GtkAdjustment* adj;
    GtkWidget* scroll;

    gint prev_xpos, last_xpos;
    gint mixerpos, old_mixerpos;
    gboolean cursor_in;

    guint scrollbar_cb_tag;

    void (*update_status_func)(SampleEditorDisplay*,
        const gboolean,
        const guint32,
        gpointer);
    gpointer status_data;
};

// = Sampler variables

static SampleDisplay* monitorscope = NULL;
static DIGC mask_gc, mask_bg_gc, red_circle_gc;
static GtkWidget *clearbutton, *okbutton, *sclock;
static void *monitor_buf = NULL;

st_driver* sampling_driver = NULL;
void* sampling_driver_object = NULL;

static GtkWidget* samplingwindow = NULL;

static STSampleChain *recordbufs, *current = NULL;
static guint recordedlen, rate, toggled_id, monitor_count;
static gboolean sampling, monitoring, has_data;
static STMixerFormat format;

// = Editing operations variables

static gint16* copybuffer = NULL;
static gsize copybufferlen = 0;
static STSample copybuffer_sampleinfo;

// = Realtime stuff

static int gtktimer = -1;

static void sample_editor_ok_clicked(void);

static void sample_editor_spin_volume_changed(GtkSpinButton* spin);
static void sample_editor_spin_panning_changed(GtkSpinButton* spin);
static void sample_editor_spin_finetune_changed(GtkSpinButton* spin);
static void sample_editor_spin_relnote_changed(GtkSpinButton* spin);
static void sample_editor_loop_changed(GtkSpinButton* spin, gpointer data);
static void sample_editor_display_loop_changed(SampleEditorDisplay*, int start, int end);
static void sample_editor_display_selection_changed(SampleEditorDisplay*, int start, int end);
static void sample_editor_display_window_changed(SampleEditorDisplay*, int start, int end);
static void sample_editor_display_position_changed(SampleEditorDisplay*, int pos);
static void sample_editor_loopradio_changed(GtkToggleButton* tb);
void sample_editor_paste_clicked(void);
void sample_editor_zoom_in_clicked(void);
void sample_editor_zoom_out_clicked(void);

#if USE_SNDFILE || AUDIOFILE_VERSION
static void sample_editor_load_wav(const gchar* name, const gchar* localname);
static void sample_editor_save_wav(const gchar* name, const gchar* localname);
static void sample_editor_save_region_wav(const gchar* name, const gchar* localname);
#endif

static void sample_editor_trim(gboolean beg, gboolean end, gfloat threshold);
static void sample_editor_delete(STSample* sample, int start, int end);

static void
sample_editor_lock_sample(void)
{
    g_mutex_lock(&sed->sample->sample.lock);
}

static void
sample_editor_unlock_sample(void)
{
    if (gui_playing_mode) {
        mixer->updatesample(&sed->sample->sample);
    }
    g_mutex_unlock(&sed->sample->sample.lock);
}

gint sample_editor_display_print_status(SampleEditorDisplay* s,
    const gboolean cursor_in,
    const guint32 pos,
    gchar* buf,
    const gsize bufsize)
{
    SampleDisplay* sd;
    gint n = 0, ss, se;

    g_assert(IS_SAMPLE_EDITOR_DISPLAY(s));
    sd = SAMPLE_DISPLAY(s);
    ss = sd->sel_start;
    se = sd->sel_end;

    if (cursor_in && s->sample->sample.data) {
        if (s->sample->sample.flags & ST_SAMPLE_STEREO)
            n += snprintf(buf, bufsize,
                _(" [Position: %i, Value L: %i, R: %i]"), pos,
                s->sample->sample.data[pos], s->sample->sample.data[pos + s->sample->sample.length]);
        else
            n += snprintf(buf, bufsize,
                _(" [Position: %i, Value: %i]"), pos, s->sample->sample.data[pos]);
    }
    if (ss != -1)
        snprintf(&buf[n], bufsize - n,
            _(" [Selection: %i \342\200\223 %i (%i discretes)]"),
            ss, se, se - ss);

    return n;
}

static void
_sample_editor_update_status(SampleEditorDisplay* s,
    const gboolean cursor_in,
    const guint32 pos,
    gpointer p)
{
    if (notebook_current_page == NOTEBOOK_PAGE_SAMPLE_EDITOR) {
        static gchar se_status[256];
        gint n = snprintf(se_status, sizeof(se_status),
            _("[Length: %i]"), sed->sample->sample.length);

        sample_editor_display_print_status(sed, cursor_in, pos,
            &se_status[n], sizeof(se_status) - n);
        gui_statusbar_update_message_high(se_status);
    }
}

void
sample_editor_update_status(void)
{
    _sample_editor_update_status(sed, sed->priv->cursor_in, sed->priv->last_xpos, NULL);
}

void
sample_editor_display_zoom_in(SampleEditorDisplay* s)
{
    SampleDisplay* sd;
    gint ns, ne, l;

    g_assert(IS_SAMPLE_EDITOR_DISPLAY(s));
    sd = SAMPLE_DISPLAY(s);
    ns = sd->win_start;
    ne = sd->win_start + sd->win_length;

    if (s->sample == NULL || s->sample->sample.data == NULL)
        return;

    l = sd->win_length / 4;

    ns += l;
    ne -= l;

    if (ne <= ns)
        ne = ns + 1;

    sample_display_set_window(sd, ns, ne);
}

void
sample_editor_display_zoom_out(SampleEditorDisplay* s)
{
    SampleDisplay* sd;
    gint ns, ne, l;

    g_assert(IS_SAMPLE_EDITOR_DISPLAY(s));
    sd = SAMPLE_DISPLAY(s);
    ns = sd->win_start;
    ne = sd->win_start + sd->win_length;

    if (s->sample == NULL || s->sample->sample.data == NULL)
        return;

    l = sd->win_length / 2;

    if (ns > l)
        ns -= l;
    else
        ns = 0;

    if (ne <= s->sample->sample.length - l)
        ne += l;
    else
        ne = s->sample->sample.length;

    sample_display_set_window(sd, ns, ne);
}

static gboolean
sample_editor_display_scrolled(SampleEditorDisplay* s,
   GdkEventScroll *event)
{
    SampleDisplay* sd = SAMPLE_DISPLAY(s);
    SampleEditorDisplayPriv* p = s->priv;
    gboolean handled = FALSE;
    gdouble value, incr, pincr, lower, upper, psize;

    if (s->sample == NULL || s->sample->sample.data == NULL)
        return handled;
    if (sd->win_start == 0 &&
        sd->win_length == s->sample->sample.length &&
        !((event->direction == GDK_SCROLL_DOWN || event->direction == GDK_SCROLL_RIGHT)
            && event->state & GDK_CONTROL_MASK))
        return handled;

    value = gtk_adjustment_get_value(p->adj);
    incr = gtk_adjustment_get_step_increment(p->adj);
    pincr = gtk_adjustment_get_page_increment(p->adj);
    psize = gtk_adjustment_get_page_size(p->adj);
    lower = gtk_adjustment_get_lower(p->adj);
    upper = gtk_adjustment_get_upper(p->adj);

    switch (event->direction) {
    case GDK_SCROLL_UP:
    case GDK_SCROLL_LEFT:
        if (event->state & GDK_CONTROL_MASK)
            sample_editor_display_zoom_out(s);
        else {
            value -= ((event->state & GDK_SHIFT_MASK) ? pincr : incr);
            gtk_adjustment_set_value(p->adj, CLAMP(value, lower, upper - psize));
        }
        handled = TRUE;
        break;
    case GDK_SCROLL_DOWN:
    case GDK_SCROLL_RIGHT:
        if (event->state & GDK_CONTROL_MASK)
            sample_editor_display_zoom_in(s);
        else {
            value += ((event->state & GDK_SHIFT_MASK) ? pincr : incr);
            gtk_adjustment_set_value(p->adj, CLAMP(value, lower, upper - psize));
        }
        handled = TRUE;
        break;
    default:
        break;
    }

    if (p->update_status_func)
        p->update_status_func(s, p->cursor_in,
            sample_display_xpos_to_offset(sd, p->last_xpos), p->status_data);

    return handled;
}

static void
sample_editor_hscrollbar_changed(SampleEditorDisplay* s)
{
    sample_display_set_window(SAMPLE_DISPLAY(s), rint(gtk_adjustment_get_value(s->priv->adj)), -1);
}

static void
red_circle_realize(GtkWidget* widget, gpointer data)
{
    static gboolean firsttime = TRUE;

    if (firsttime) {
        mask_gc = di_gc_new(widget->window);
        mask_bg_gc = colors_get_gc(COLOR_BLACK);
        red_circle_gc = colors_get_gc(COLOR_RED);
        firsttime = FALSE;
    }
}

static void
red_circle_draw(GtkWidget* widget, GdkRectangle* area, gpointer data)
{
    if (sampling) {
        static gint width = -1, height = -1;
        static guint oldsize = 0;
        static GdkPixmap* red_circle_pm = NULL;
        static DIDrawable red_circle_drw;

        guint size = MAX(MIN_SIZE, MIN(widget->allocation.width, widget->allocation.height) / FRACTION_S);
        guint x0 = widget->allocation.width - XDISP - widget->allocation.width / FRACTION_D - size;
        guint y0 = YDISP + widget->allocation.height / FRACTION_D;
        GdkRectangle dest_area;
        DIDrawable win = custom_drawing_get_drawable(CUSTOM_DRAWING(widget));

        dest_area.x = x0;
        dest_area.y = y0;
        dest_area.width = size;
        dest_area.height = size;

        /* Update number mask if necessary */
        if (widget->allocation.width != width || widget->allocation.height != height || (!red_circle_pm)) {
            width = widget->allocation.width;
            height = widget->allocation.height;

            /* The indicator itself is to be redrawn */
            if (!red_circle_pm || oldsize != size) {
                oldsize = size;

                if (red_circle_pm)
                    g_object_unref(red_circle_pm);
                red_circle_pm = gdk_pixmap_new(widget->window, size, size, -1);
                red_circle_drw = di_get_drawable(red_circle_pm);

                di_draw_rectangle(red_circle_drw, mask_bg_gc, TRUE, 0, 0, size, size);
                di_draw_arc(red_circle_drw, red_circle_gc, TRUE, 0, 0, size, size, 0, 360 * 64);
            }
            di_update_mask(widget->window, red_circle_pm, mask_gc, x0, y0, size, size, width, height);
        }

        if (gdk_rectangle_intersect(area, &dest_area, &dest_area))
            di_draw_drawable(win, mask_gc, red_circle_drw,
                dest_area.x - x0, dest_area.y - y0,
                dest_area.x, dest_area.y, dest_area.width, dest_area.height);
    }
}

static gboolean
sample_display_cursor_motion(GtkWidget* w, GdkEventMotion* event)
{
    SampleEditorDisplay* s;
    SampleEditorDisplayPriv* p;

    g_assert(IS_SAMPLE_EDITOR_DISPLAY(w));
    s = SAMPLE_EDITOR_DISPLAY(w);
    p = s->priv;

    if (p->last_xpos != event->x) {
        p->last_xpos = event->x;
        if (p->update_status_func)
            p->update_status_func(s, p->cursor_in,
                sample_display_xpos_to_offset(SAMPLE_DISPLAY(s), p->last_xpos), p->status_data);
        gtk_widget_queue_draw_area(w, event->x, 0, 1, w->allocation.height);
    }

    return FALSE;
}

static gboolean
sample_display_border_cross(GtkWidget* w, GdkEventCrossing* event, gpointer p)
{
    SampleEditorDisplay* s;
    SampleEditorDisplayPriv* pr;

    g_assert(IS_SAMPLE_EDITOR_DISPLAY(w));
    s = SAMPLE_EDITOR_DISPLAY(w);
    pr = s->priv;

    pr->cursor_in = GPOINTER_TO_INT(p);
    if (pr->cursor_in)
        pr->last_xpos = event->x;

    if (pr->update_status_func)
        pr->update_status_func(s, pr->cursor_in,
                sample_display_xpos_to_offset(SAMPLE_DISPLAY(s), pr->last_xpos), pr->status_data);
    gtk_widget_queue_draw_area(w, pr->last_xpos, 0, 1, w->allocation.height);

    return FALSE;
}

static void
sample_editor_block_loop_spins(gboolean block)
{
    if (block) {
        g_signal_handler_block(G_OBJECT(se_spins[SAMPLE_EDITOR_LOOPSTART]), tags[SAMPLE_EDITOR_LOOPSTART]);
        g_signal_handler_block(G_OBJECT(se_spins[SAMPLE_EDITOR_LOOPEND]), tags[SAMPLE_EDITOR_LOOPEND]);
    } else {
        g_signal_handler_unblock(G_OBJECT(se_spins[SAMPLE_EDITOR_LOOPSTART]), tags[SAMPLE_EDITOR_LOOPSTART]);
        g_signal_handler_unblock(G_OBJECT(se_spins[SAMPLE_EDITOR_LOOPEND]), tags[SAMPLE_EDITOR_LOOPEND]);
    }
}

static void
sample_editor_loop_changed(GtkSpinButton* spin, gpointer data)
{
    st_mixer_sample_info *s;
    const gint new_v = gtk_spin_button_get_value_as_int(spin);
    const gboolean is_end = GPOINTER_TO_INT(data);
    guint32* v;

    g_return_if_fail(sed->sample != NULL);
    g_return_if_fail(sed->sample->sample.data != NULL);

    s = &sed->sample->sample;
    v = is_end ? &s->loopend : &s->loopstart;
    if (*v == new_v)
        return;

    history_log_spin_button(spin, _(is_end ? N_("Sample loop end setting") : N_("Sample loop start setting")),
        HISTORY_FLAG_LOG_ALL, *v);
    sample_editor_lock_sample();
    *v = new_v;
    sample_editor_unlock_sample();

    if (is_end)
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPSTART]), 0, s->length ? new_v - 1 : 0);
    else
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPEND]), s->length ? new_v + 1 : 1, s->length);
    if (IS_SAMPLE_LOOPED(sed->sample->sample))
        sample_display_set_loop(sampledisplay, s->loopstart, s->loopend);
}

static void
sample_editor_resolution_changed(GtkToggleButton* tb)
{
    STSample* sts = sed->sample;
    gint n, old_v;

    if (!gtk_toggle_button_get_active(tb))
        return;
    if (!sts)
        return;
    n = find_current_toggle(resolution_radio, 2);
    old_v = sts->sample.flags & ST_SAMPLE_16_BIT ? 1 : 0;
    if (n == old_v)
        return;

    history_log_radio_group(resolution_radio, _("Sample resolution setting"),
        HISTORY_FLAG_LOG_ALL, sts->sample.flags & ST_SAMPLE_16_BIT ? 1 : 0, 2);
    if (n == 0)
        sts->sample.flags &= ~ST_SAMPLE_16_BIT;
    else
        sts->sample.flags |= ST_SAMPLE_16_BIT;
}

void
sample_editor_play_loop_toggled(GtkToggleToolButton* tb)
{
    gui_settings.sample_play_loop = gtk_toggle_tool_button_get_active(tb);
}

static void
sample_editor_display_draw(GtkWidget* widget, GdkRectangle* area, gpointer data)
{
    SampleDisplay* sd;
    SampleEditorDisplay* s;
    SampleEditorDisplayPriv* p;
    DIDrawable win = custom_drawing_get_drawable(CUSTOM_DRAWING(widget));
    DIGC meter_line_gc, mixerpos_gc;
    gint mixer_xpos, x_min, x_max;

    g_assert(IS_SAMPLE_EDITOR_DISPLAY(widget));
    s = SAMPLE_EDITOR_DISPLAY(widget);
    sd = SAMPLE_DISPLAY(widget);
    p = s->priv;
    meter_line_gc = SAMPLE_EDITOR_DISPLAY_GET_CLASS(widget)->meter_line_gc;
    mixerpos_gc = SAMPLE_EDITOR_DISPLAY_GET_CLASS(widget)->mixerpos_gc;

    if (p->last_xpos >= area->x && p->last_xpos < area->x + area->width &&
        p->cursor_in && p->prev_xpos != p->last_xpos)
        di_draw_line(win, meter_line_gc, p->last_xpos, area->y,
            p->last_xpos, area->y + area->height);

    if (p->prev_xpos != -1 && p->prev_xpos != p->last_xpos)
        gtk_widget_queue_draw_area(widget, p->prev_xpos, area->y, 1, area->height);

    p->prev_xpos = p->last_xpos;

    if (p->mixerpos != -1 && p->mixerpos >= sd->win_start &&
        p->mixerpos <= sd->win_start + sd->win_length) {
        mixer_xpos = sample_display_offset_to_xpos(sd, p->mixerpos);
        x_min = MAX(0, mixer_xpos - 3);
        x_max = MIN(sd->width, mixer_xpos + 3);

        di_draw_line(win, mixerpos_gc, mixer_xpos, area->y,
            mixer_xpos, area->y + area->height);
        di_draw_rectangle(win, mixerpos_gc, TRUE, x_min, 0,
            x_max - x_min, 10);
        di_draw_rectangle(win, mixerpos_gc, TRUE, x_min, sd->height - 10,
            x_max - x_min, 10);
    }
    if (p->old_mixerpos != p->mixerpos) {
        if (p->old_mixerpos != -1 && p->old_mixerpos >= sd->win_start &&
            p->old_mixerpos <= sd->win_start + sd->win_length) {
            mixer_xpos = sample_display_offset_to_xpos(sd, p->old_mixerpos);
            x_min = MAX(0, mixer_xpos - 3);
            x_max = MIN(sd->width, mixer_xpos + 3);

            gtk_widget_queue_draw_area(widget, x_min, area->y, x_max - x_min, area->height);
        }
        p->old_mixerpos = p->mixerpos;
    }
}

static void
sample_editor_display_realize(GtkWidget* widget, gpointer data)
{
    static gboolean firsttime = TRUE;

    g_assert(IS_SAMPLE_EDITOR_DISPLAY(widget));

    if (firsttime) {
        SAMPLE_EDITOR_DISPLAY_GET_CLASS(widget)->meter_line_gc = colors_get_gc(COLOR_ZEROLINE);
        SAMPLE_EDITOR_DISPLAY_GET_CLASS(widget)->mixerpos_gc = colors_get_gc(COLOR_MIXERPOS);
        firsttime = FALSE;
    }
}

static void
sample_editor_display_init(SampleEditorDisplay* s)
{
    s->priv = g_new(SampleEditorDisplayPriv, 1);
    s->priv->prev_xpos = -1;
    s->priv->last_xpos = 0;
    s->priv->cursor_in = FALSE;
    s->sample = NULL;
    custom_drawing_register_realize_func(CUSTOM_DRAWING(s), sample_editor_display_realize, NULL);
    custom_drawing_register_drawing_func(CUSTOM_DRAWING(s), sample_editor_display_draw, NULL);
}

static void
sample_editor_display_class_init(SampleEditorDisplayClass* c)
{
}

G_DEFINE_TYPE(SampleEditorDisplay, sample_editor_display, sample_display_get_type())

void
sample_editor_display_set_sample(SampleEditorDisplay* s,
    STSample* sample)
{
    g_assert(IS_SAMPLE_EDITOR_DISPLAY(s));

    s->sample = sample;
    s->priv->mixerpos = -1;
    s->priv->old_mixerpos = -1;

    sample_display_set_data(SAMPLE_DISPLAY(s), NULL, ST_MIXER_FORMAT_S16_LE, 0, FALSE);
    if (sample)
        if (sample->sample.data) {
            sample_display_set_data(SAMPLE_DISPLAY(s), sample->sample.data,
                sample->sample.flags & ST_SAMPLE_STEREO ? ST_MIXER_FORMAT_S16_LE | ST_MIXER_FORMAT_STEREO_NI :
                ST_MIXER_FORMAT_S16_LE, sample->sample.length, FALSE);
            if (IS_SAMPLE_LOOPED(sample->sample))
                sample_display_set_loop(SAMPLE_DISPLAY(s),
                    sample->sample.loopstart, sample->sample.loopend);
        }
}

GtkWidget*
sample_editor_display_new(GtkWidget* scroll,
    const gboolean edit,
    SampleDisplayMode mode,
    void (*update_status_func)(SampleEditorDisplay*,
        const gboolean,
        const guint32,
        gpointer),
    gpointer status_data)
{
    SampleEditorDisplay* s = SAMPLE_EDITOR_DISPLAY(g_object_new(sample_editor_display_get_type(), NULL));
    SampleDisplay* sd = SAMPLE_DISPLAY(s);
    SampleEditorDisplayPriv* p = s->priv;

    sample_display_set_edit(sd, edit);
    sample_display_set_mode(sd, mode);
    p->scroll = scroll;
    p->adj = gtk_range_get_adjustment(GTK_RANGE(scroll));
    p->update_status_func = update_status_func;
    p->status_data = status_data;
    s->sample = NULL;

    g_signal_connect(s, "scroll-event",
                     G_CALLBACK(sample_editor_display_scrolled), NULL);
    p->scrollbar_cb_tag = g_signal_connect_swapped(scroll, "value-changed",
                                           G_CALLBACK(sample_editor_hscrollbar_changed), s);
    g_signal_connect(s, "window_changed",
                     G_CALLBACK(sample_editor_display_window_changed), NULL);
    g_signal_connect(s, "position_changed",
                     G_CALLBACK(sample_editor_display_position_changed), NULL);
    g_signal_connect(s, "selection_changed",
        G_CALLBACK(sample_editor_display_selection_changed), NULL);
    g_signal_connect(s, "motion_notify_event",
        G_CALLBACK(sample_display_cursor_motion), NULL);
    gtk_widget_add_events(GTK_WIDGET(s), GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
    g_signal_connect(s, "enter_notify_event",
        G_CALLBACK(sample_display_border_cross), GINT_TO_POINTER(TRUE));
    g_signal_connect(s, "leave_notify_event",
        G_CALLBACK(sample_display_border_cross), GINT_TO_POINTER(FALSE));

    sample_display_enable_zero_line(sd, TRUE);

    return GTK_WIDGET(s);
}

GtkWidget*
sample_editor_get_widget(const gchar* name)
{
#define S_E_UI DATADIR "/" PACKAGE "/sample-editor.ui"
    if (!builder)
        builder = gui_builder_from_file(S_E_UI, NULL);

    return gui_get_widget(builder, name, S_E_UI);
}

void
sample_editor_get_selection(gint* start,
    gint* end)
{
    *start = sampledisplay->sel_start;
    *end = sampledisplay->sel_end;
}

void
sample_editor_set_selection(const gint start,
    const gint end)
{
    g_assert(start == -1 || end > start);
    sample_display_set_selection(sampledisplay, start, end);
}

static inline void
sensitive_list_add(const gchar* const name)
{
    list_data_sensitive = g_slist_prepend(list_data_sensitive,
        sample_editor_get_widget(name));
}

static inline void
loop_sensitive_list_add(const gchar* const name)
{
    list_loop_sensitive = g_slist_prepend(list_loop_sensitive,
        sample_editor_get_widget(name));
}

static inline void
stereo_visible_list_add(const gchar* const name)
{
    list_stereo_visible = g_slist_prepend(list_stereo_visible,
        sample_editor_get_widget(name));
}

static inline void
mono_visible_list_add(const gchar* const name)
{
    list_mono_visible = g_slist_prepend(list_mono_visible,
        sample_editor_get_widget(name));
}

void sample_editor_page_create(GtkNotebook* nb)
{
    GtkWidget *box, *thing, *hbox;
    GtkAccelGroup *accel_group;
    gint i;

    static const char* looplabels[] = {
        N_("No loop"),
        N_("Amiga"),
        N_("PingPong")
    };
    static const char* resolutionlabels[] = {
        N_("8 bits"),
        N_("16 bits")
    };

#if USE_SNDFILE || AUDIOFILE_VERSION
    static const gchar* aiff_f[] = { N_("Apple/SGI audio (*aif, *.aiff, *.aifc)"), "*.[aA][iI][fF]", "*.[aA][iI][fF][fFcC]", NULL };
    static const gchar* au_f[] = { N_("SUN/NeXT audio (*.au, *.snd)"), "*.[aA][uU]", "*.[sS][nN][dD]", NULL };
    static const gchar* avr_f[] = { N_("Audio Visual Research files (*.avr)"), "*.[aA][vV][rR]", NULL };
    static const gchar* caf_f[] = { N_("Apple Core Audio files (*.caf)"), "*.[cC][aA][fF]", NULL };
    static const gchar* iff_f[] = { N_("Amiga IFF/SV8/SV16 (*.iff)"), "*.[iI][fF][fF]", NULL };
    static const gchar* sf_f[] = { N_("Berkeley/IRCAM/CARL audio (*.sf)"), "*.[sS][fF]", NULL };
    static const gchar* voc_f[] = { N_("Creative Labs audio (*.voc)"), "*.[vV][oO][cC]", NULL };
    static const gchar* wavex_f[] = { N_("Microsoft RIFF/NIST Sphere (*.wav)"), "*.[wW][aA][vV]", NULL };
    static const gchar* flac_f[] = { N_("FLAC lossless audio (*.flac)"), "*.flac", NULL };
    static const gchar* wve_f[] = { N_("Psion audio (*.wve)"), "*.wve", NULL };
    static const gchar* ogg_f[] = { N_("OGG compressed audio (*.ogg, *.vorbis)"), "*.ogg", "*.vorbis", NULL };
    static const gchar* rf64_f[] = { N_("RIFF 64 files (*.rf64)"), "*.rf64", NULL };

    static const gchar* wav_f[] = { N_("Microsoft RIFF (*.wav)"), "*.[wW][aA][vV]", NULL };
    static const gchar** out_f[] = { wav_f, NULL };
#endif

#if USE_SNDFILE
    static const gchar* htk_f[] = { N_("HMM Tool Kit files (*.htk)"), "*.[hH][tT][kK]", NULL };
    static const gchar* mat_f[] = { N_("GNU Octave/Matlab files (*.mat)"), "*.[mM][aA][tT]", NULL };
    static const gchar* paf_f[] = { N_("Ensoniq PARIS files (*.paf)"), "*.[pP][aA][fF]", NULL };
    static const gchar* pvf_f[] = { N_("Portable Voice Format files (*.pvf)"), "*.[pP][vV][fF]", NULL };
    static const gchar* raw_f[] = { N_("Headerless raw data (*.raw, *.r8)"), "*.[rR][aA][wW]", "*.[rR]8", NULL };
    static const gchar* sd2_f[] = { N_("Sound Designer II files (*.sd2)"), "*.[sS][dD]2", NULL };
    static const gchar* sds_f[] = { N_("Midi Sample Dump Standard files (*.sds)"), "*.[sS][dD][sS]", NULL };
    static const gchar* w64_f[] = { N_("SoundFoundry WAVE 64 files (*.w64)"), "*.[wW]64", NULL };

    static const gchar** in_f[] = { aiff_f, au_f, avr_f, caf_f, htk_f, iff_f, mat_f, paf_f, pvf_f, raw_f, sd2_f, sds_f, sf_f,
        voc_f, w64_f, wavex_f, flac_f, wve_f, ogg_f, rf64_f, NULL };
#endif

#if !USE_SNDFILE && AUDIOFILE_VERSION
    static const gchar* smp_f[] = { N_("Sample Vision files (*.smp)"), "*.[sS][mM][pP]", NULL };

    static const gchar** in_f[] = { aiff_f, au_f, avr_f, caf_f, iff_f, sf_f, voc_f, wavex_f, smp_f, rf64_f, ogg_f,
        wve_f, flac_f, NULL };
#endif

    const icon_set sample_editor_icons[] =
        {{"st-select-all", "select-all", SIZES_MENU_TOOLBOX},
         {"st-select-none", "select-none", SIZES_MENU_TOOLBOX},
         {"st-crop", "crop", SIZES_MENU},
         {"st-sample-play-loop", "sample-play-loop", SIZES_TOOLBOX},
         {"st-sample-go-loop-beg", "sample-go-loop-beg", SIZES_TOOLBOX},
         {"st-sample-go-loop-end", "sample-go-loop-end", SIZES_TOOLBOX},
         {"st-sample-loop-factory", "loop-factory", SIZES_MENU_TOOLBOX},
         {NULL}};

#if USE_SNDFILE || AUDIOFILE_VERSION
    loadsmp = fileops_dialog_create(DIALOG_LOAD_SAMPLE, _("Load Sample"), &gui_settings.loadsmpl_path, sample_editor_load_wav, TRUE, FALSE, in_f, N_("Load sample into the current sample slot"), NULL);
    savesmp = fileops_dialog_create(DIALOG_SAVE_SAMPLE, _("Save Sample"), &gui_settings.savesmpl_path, sample_editor_save_wav, TRUE, TRUE, out_f, N_("Save the current sample"), "wav");
    savereg = fileops_dialog_create(DIALOG_SAVE_RGN_SAMPLE, _("Save region as WAV..."), &gui_settings.savesmpl_path, sample_editor_save_region_wav, FALSE, TRUE, out_f, NULL, "wav");
#endif

    gui_add_icons(sample_editor_icons);

    box = gtk_vbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);
    gtk_notebook_append_page(nb, box, gtk_label_new(_("Sample Editor")));
    gtk_widget_show(box);

    thing = sample_editor_get_widget("sample_editor_menu");
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);
    thing = sample_editor_get_widget("sample_editor_toolbar");
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);

    sensitive_list_add("selection_menu");
    sensitive_list_add("edit_menu");
    sensitive_list_add("view_menu");

    loop_sensitive_list_add("loop_factory_tbutton");
    loop_sensitive_list_add("loop_factory_menuitem");

    stereo_visible_list_add("norm_chan_item");
    stereo_visible_list_add("convert_to_mono_item");
    stereo_visible_list_add("swop_channels_item");
    stereo_visible_list_add("stereo_separation_item");
    stereo_visible_list_add("stereo_balance_item");
    mono_visible_list_add("convert_to_stereo_item");

    struct {
        const gchar* const title;
        guint key;
        GdkModifierType mods;
    } accel_list[] = {
        {"zoom_sel_tbutton", 'z', GDK_SHIFT_MASK},
        {"show_all_tbutton", '1', GDK_CONTROL_MASK},
        {"zoom_in_tbutton", '=', GDK_CONTROL_MASK},
        {"zoom_out_tbutton", '-', GDK_CONTROL_MASK}
    };

    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(mainwindow), accel_group);
    for (i = 0; i < sizeof(accel_list) / sizeof(accel_list[0]); i++)
        gtk_widget_add_accelerator(sample_editor_get_widget(accel_list[i].title),
        "clicked", accel_group, accel_list[i].key, accel_list[i].mods, 0);

#if USE_SNDFILE || AUDIOFILE_VERSION
    thing = sample_editor_get_widget("save_wav_tbutton");
    list_data_sensitive = g_slist_prepend(list_data_sensitive, thing);
    gtk_widget_show(thing);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(fileops_open_dialog), savesmp);

    thing = sample_editor_get_widget("open_smp_tbutton");
    gtk_widget_show(thing);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(fileops_open_dialog), loadsmp);

    thing = sample_editor_get_widget("open_smp");
    gtk_widget_show(thing);
    g_signal_connect_swapped(thing, "activate",
        G_CALLBACK(fileops_open_dialog), loadsmp);

    thing = sample_editor_get_widget("save_wav");
    gtk_widget_show(thing);
    g_signal_connect_swapped(thing, "activate",
        G_CALLBACK(fileops_open_dialog), savesmp);

    list_data_sensitive = g_slist_prepend(list_data_sensitive, thing);
    thing = sample_editor_get_widget("save_reg");
    gtk_widget_show(thing);
    g_signal_connect_swapped(thing, "activate",
        G_CALLBACK(fileops_open_dialog), savereg);

    list_data_sensitive = g_slist_prepend(list_data_sensitive, thing);
    thing = sample_editor_get_widget("fileop_sep");
    gtk_widget_show(thing);
#endif

    sensitive_list_add("cut_tbutton");
    sensitive_list_add("copy_tbutton");
    sensitive_list_add("delete_tbutton");
    sensitive_list_add("clear_tbutton");
    sensitive_list_add("select_all_tbutton");
    sensitive_list_add("select_none_tbutton");
    sensitive_list_add("zoom_sel_tbutton");
    sensitive_list_add("show_all_tbutton");
    sensitive_list_add("zoom_in_tbutton");
    sensitive_list_add("zoom_out_tbutton");
    sensitive_list_add("go_home_tbutton");
    sensitive_list_add("go_end_tbutton");
    loop_sensitive_list_add("go_loop_beg_tbutton");
    loop_sensitive_list_add("go_loop_end_tbutton");

    thing = sample_editor_get_widget("play_loop_tbutton");
    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(thing), gui_settings.sample_play_loop);

    sample_editor_extensions_fill_menu();

    sample_editor_hscrollbar = gtk_hscrollbar_new(NULL);
    hadj = gtk_range_get_adjustment(GTK_RANGE(sample_editor_hscrollbar));
    gtk_widget_show(sample_editor_hscrollbar);

    thing = sample_editor_display_new(sample_editor_hscrollbar, TRUE,
        gui_settings.editor_mode, _sample_editor_update_status, NULL);
    gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "loop_changed",
        G_CALLBACK(sample_editor_display_loop_changed), NULL);
    sampledisplay = SAMPLE_DISPLAY(thing);
    sed = SAMPLE_EDITOR_DISPLAY(thing);

    gtk_box_pack_start(GTK_BOX(box), sample_editor_hscrollbar, FALSE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 8);
    list_data_sensitive = g_slist_prepend(list_data_sensitive, hbox);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 4);

    tags[SAMPLE_EDITOR_VOLUME] = gui_put_labelled_spin_button(hbox, _("Volume"), 0, 64,
        &se_spins[SAMPLE_EDITOR_VOLUME], sample_editor_spin_volume_changed, NULL, TRUE);
    tags[SAMPLE_EDITOR_PANNING] = gui_put_labelled_spin_button(hbox, _("Panning"), -128, 127,
        &se_spins[SAMPLE_EDITOR_PANNING], sample_editor_spin_panning_changed, NULL, TRUE);
    tags[SAMPLE_EDITOR_FINETUNE] = gui_put_labelled_spin_button(hbox, _("Finetune"), -128, 127,
        &se_spins[SAMPLE_EDITOR_FINETUNE], sample_editor_spin_finetune_changed, NULL, TRUE);
    tags[SAMPLE_EDITOR_RELNOTE] = gui_put_labelled_spin_button(hbox, _("RelNote"), -128, 127,
        &se_spins[SAMPLE_EDITOR_RELNOTE], sample_editor_spin_relnote_changed, NULL, TRUE);
    make_radio_group(resolutionlabels, hbox, resolution_radio, FALSE, FALSE, sample_editor_resolution_changed);
    gtk_widget_show_all(hbox);

    thing = gtk_hseparator_new();
    gtk_widget_show(thing);
    gtk_box_pack_start(GTK_BOX(box), thing, FALSE, TRUE, 0);

    hbox = gtk_hbox_new(FALSE, 8);
    list_data_sensitive = g_slist_prepend(list_data_sensitive, hbox);
    gtk_box_pack_start(GTK_BOX(box), hbox, FALSE, FALSE, 4);

    make_radio_group(looplabels, hbox, loopradio, FALSE, FALSE, sample_editor_loopradio_changed);
    tags[SAMPLE_EDITOR_LOOPSTART] = gui_put_labelled_spin_button(hbox, _("Start"), 0, 0,
        &se_spins[SAMPLE_EDITOR_LOOPSTART], sample_editor_loop_changed, GINT_TO_POINTER(FALSE), TRUE);
    tags[SAMPLE_EDITOR_LOOPEND] = gui_put_labelled_spin_button(hbox, _("End"), 0, 0,
        &se_spins[SAMPLE_EDITOR_LOOPEND], sample_editor_loop_changed, GINT_TO_POINTER(TRUE), TRUE);
    gtk_widget_show_all(hbox);
    sample_editor_loopradio_changed(GTK_TOGGLE_BUTTON(loopradio[0]));

    gtk_builder_connect_signals(builder, NULL);
}

void
sample_editor_set_mode(SampleEditorDisplayInstance display, SampleDisplayMode mode)
{
    SampleDisplay* disp = (display == SAMPLE_EDITOR_DISPLAY_EDITOR) ? sampledisplay : monitorscope;

    if (disp && IS_SAMPLE_DISPLAY(disp)) {
        sample_display_set_mode(disp, mode);
        gtk_widget_queue_draw(GTK_WIDGET(disp));
    }
}

static void
sample_editor_blocked_set_loop_spins(int start,
    int end)
{
    gdouble min, max;

    sample_editor_block_loop_spins(1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPSTART]), 0, end - 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPSTART]), start);
    gtk_spin_button_get_range(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPEND]), &min, &max);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPEND]), start + 1, max);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPEND]), end);
    sample_editor_block_loop_spins(0);
}

static inline void
set_loop_sensitive(const gboolean is_loop)
{
    GSList *l = list_loop_sensitive;

    for (;l;l = l->next)
        gtk_widget_set_sensitive(GTK_WIDGET(l->data), is_loop);
}

static inline void
set_stereo_visible(const gboolean is_stereo)
{
    GSList *l = list_stereo_visible;

    for (;l;l = l->next)
        gtk_widget_set_visible(GTK_WIDGET(l->data), is_stereo);
}

static inline void
set_mono_visible(const gboolean is_mono)
{
    GSList *l = list_mono_visible;

    for (;l;l = l->next)
        gtk_widget_set_visible(GTK_WIDGET(l->data), is_mono);
}

static void
sample_editor_set_sensitive(const gboolean sensitive)
{
    GSList *l = list_data_sensitive;

    for (;l;l = l->next)
        gtk_widget_set_sensitive(GTK_WIDGET(l->data), sensitive);
}

static gboolean
sample_editor_update_idle_func(guint *handler)
{
    STSample *sts = sed->sample;
    st_mixer_sample_info *s;

    gui_block_signals(se_spins, tags, TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_VOLUME]), sts->volume);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_PANNING]), sts->panning);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_FINETUNE]), sts->finetune);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_RELNOTE]), sts->relnote);

    s = &sts->sample;

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(loopradio[s->flags & ST_SAMPLE_LOOP_MASK]), TRUE);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPSTART]), 0, s->length ? s->loopend - 1 : 0);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPEND]), s->length ? s->loopstart + 1 : 1, s->length);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPSTART]), (s->loopstart));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPEND]), (s->loopend));
    gui_block_signals(se_spins, tags, FALSE);

    if (s->data) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(resolution_radio[sts->sample.flags & ST_SAMPLE_16_BIT ? 1 : 0]), TRUE);
        sample_editor_set_sensitive(TRUE);
        set_loop_sensitive((s->flags & ST_SAMPLE_LOOP_MASK) != ST_SAMPLE_LOOPTYPE_NONE);
        set_stereo_visible(s->flags & ST_SAMPLE_STEREO);
        set_mono_visible(!(s->flags & ST_SAMPLE_STEREO));
    }
    sample_editor_update_status();

    *handler = 0;
    return FALSE;
}

void
sample_editor_notify_update(SampleUpdateNotifyFunc func)
{
    sample_update_notify_list = g_slist_prepend(sample_update_notify_list, func);
}

void sample_editor_update_full(const gboolean full)
{
    static guint handler = 0;

    STSample* sts = sed->sample;

    if (full) {
        GSList* l;

        sed->priv->mixerpos = -1;
        sed->priv->old_mixerpos = -1;
        sample_display_set_data(sampledisplay, NULL, ST_MIXER_FORMAT_S16_LE, 0, FALSE);
        if (sts->sample.data)
            sample_display_set_data(sampledisplay, sts->sample.data,
            sts->sample.flags & ST_SAMPLE_STEREO ? ST_MIXER_FORMAT_S16_LE | ST_MIXER_FORMAT_STEREO_NI :
            ST_MIXER_FORMAT_S16_LE, sts->sample.length, FALSE);
        /* See comment in the sample_editor_set_sample() */
        for (l = sample_update_notify_list; l; l = l->next)
            ((SampleUpdateNotifyFunc)l->data)(sts);
    }

    if (!sts || !sts->sample.data) {
        sample_editor_set_sensitive(FALSE);
        set_loop_sensitive(FALSE);
    }

    if (!sts) {
        return;
    }

    gui_block_smplname_entry(TRUE); /* To prevent the callback */
    gtk_entry_set_text(GTK_ENTRY(gui_cursmpl_name), sts->utf_name);
    gui_block_smplname_entry(FALSE);

    if (full && sts->sample.data && IS_SAMPLE_LOOPED(sts->sample))
        sample_display_set_loop(sampledisplay, sts->sample.loopstart, sts->sample.loopend);

    if (!handler)
        handler = g_idle_add((GSourceFunc)sample_editor_update_idle_func, &handler);
    g_assert(handler != 0);
}

void
sample_editor_remove_clicked(void)
{
    sample_editor_copy_cut_common(FALSE, TRUE);
}

void
sample_editor_display_select_none(SampleEditorDisplay* s)
{
    g_assert(IS_SAMPLE_EDITOR_DISPLAY(s));
    sample_display_set_selection(SAMPLE_DISPLAY(s), -1, 1);
}

void
sample_editor_select_none_clicked(void)
{
    sample_editor_display_select_none(sed);
}

void
sample_editor_display_select_all(SampleEditorDisplay* s)
{
    g_assert(IS_SAMPLE_EDITOR_DISPLAY(s));

    g_return_if_fail(sed->sample != NULL);

    sample_display_set_selection(SAMPLE_DISPLAY(s),
        0, s->sample->sample.length);
}

void
sample_editor_select_all_clicked(void)
{
    sample_editor_display_select_all(sed);
}

static gboolean
get_selection(SampleDisplay* sd,
    STSample* s, gint* ss, gint* se)
{
    *ss = sd->sel_start, *se = sd->sel_end;
    if (*ss == -1) {
        *ss = 0;
        *se = s->sample.length;
        return FALSE;
    }

    return TRUE;
}

void
sample_editor_selection_left_clicked(void)
{
    gint ss, se;

    g_return_if_fail(sed->sample != NULL);
    if (get_selection(sampledisplay, sed->sample, &ss, &se)) {
        if (ss > 0)
            sample_display_set_selection(sampledisplay,
                0, ss - 1);
    }
}

void
sample_editor_selection_right_clicked(void)
{
    gint ss, se, s_end;

    g_return_if_fail(sed->sample != NULL);
    s_end = sed->sample->sample.length;

    if (get_selection(sampledisplay, sed->sample, &ss, &se)) {
        if (se < s_end)
            sample_display_set_selection(sampledisplay,
                se + 1, s_end);
    }
}

void
sample_editor_selection_from_loop_clicked(void)
{
    g_return_if_fail(sed->sample != NULL);
    if (IS_SAMPLE_LOOPED(sed->sample->sample))
        sample_display_set_selection(sampledisplay,
            sed->sample->sample.loopstart, sed->sample->sample.loopend);
}

static gboolean
sample_editor_scroll(const gint direction,
   const gboolean scroll_page)
{
    gdouble value, incr, pincr, lower, upper, psize;

    if (sampledisplay->win_start == 0 &&
        sampledisplay->win_length == sed->sample->sample.length)
        return FALSE;

    value = gtk_adjustment_get_value(hadj);
    incr = gtk_adjustment_get_step_increment(hadj);
    pincr = gtk_adjustment_get_page_increment(hadj);
    psize = gtk_adjustment_get_page_size(hadj);
    lower = gtk_adjustment_get_lower(hadj);
    upper = gtk_adjustment_get_upper(hadj);

    value += (gdouble)direction * ((scroll_page) ? pincr : incr);
    gtk_adjustment_set_value(hadj, CLAMP(value, lower, upper - psize));
    sample_editor_update_status();

    return TRUE;
}

enum {
    MOVE_BEG,
    MOVE_END,
    MOVE_LOOPSTART,
    MOVE_LOOPEND
};

static gboolean
sample_editor_move_to(const gint where)
{
    gdouble value, lower, upper, psize;
    guint32 lstart, win_len = sampledisplay->win_length;

    if (sed->sample == NULL || sed->sample->sample.data == NULL)
        return FALSE;
    if (sampledisplay->win_start == 0 &&
        win_len == sed->sample->sample.length)
        return FALSE;

    psize = gtk_adjustment_get_page_size(hadj);
    lower = gtk_adjustment_get_lower(hadj);
    upper = gtk_adjustment_get_upper(hadj);
    win_len >>= 1;

    switch (where) {
    case MOVE_BEG:
        value = lower;
        break;
    case MOVE_END:
        value = upper;
        break;
    case MOVE_LOOPSTART:
        if (!IS_SAMPLE_LOOPED(sed->sample->sample))
            return FALSE;
        lstart = sed->sample->sample.loopstart;
        value = win_len > lstart ? 0.0 : lstart - win_len;
        break;
    case MOVE_LOOPEND:
        if (!IS_SAMPLE_LOOPED(sed->sample->sample))
            return FALSE;
        value = sed->sample->sample.loopend - win_len;
        break;
    default:
        return FALSE;
    }

    gtk_adjustment_set_value(hadj, CLAMP(value, lower, upper - psize));
    sample_editor_update_status();

    return TRUE;
}

void
go_home_clicked(void)
{
    sample_editor_move_to(MOVE_BEG);
}

void
go_end_clicked(void)
{
    sample_editor_move_to(MOVE_END);
}

void
go_loop_beg_clicked(void)
{
    sample_editor_move_to(MOVE_LOOPSTART);
}

void
go_loop_end_clicked(void)
{
    sample_editor_move_to(MOVE_LOOPEND);
}

gboolean
sample_editor_handle_keys(int shift,
    int ctrl,
    int alt,
    guint32 keyval,
    gboolean pressed)
{
    gboolean is_note = FALSE;
    int s = sampledisplay->sel_start, e = sampledisplay->sel_end;
    int modifiers = ENCODE_MODIFIERS(shift, ctrl, alt);

    if (sed->sample == NULL || sed->sample->sample.data == NULL)
        return FALSE;

    if (s == -1) {
        s = 0;
        e = sed->sample->sample.length;
    }

    if (pressed)
        switch (keyval) {
        case GDK_KEY_Delete:
            if (!(shift || alt || ctrl)) {
                sample_editor_remove_clicked();
                return TRUE;
            }
            break;
        case 'a':
            if (ctrl && !alt) {
                sample_editor_select_all_clicked();
                return TRUE;
            }
            break;
        case 'A':
            if (ctrl && !alt) {
                sample_editor_select_none_clicked();
                return TRUE;
            }
            break;
        case GDK_Left:
            if (!(shift || alt || ctrl))
                return sample_editor_scroll(-1, FALSE);
            else
                return FALSE;
        case GDK_Right:
            if (!(shift || alt || ctrl))
                return sample_editor_scroll(1, FALSE);
            else
                return FALSE;
        case GDK_Page_Up:
            if (!(shift || alt || ctrl))
                return sample_editor_scroll(-1, TRUE);
            else
                return FALSE;
        case GDK_Page_Down:
            if (!(shift || alt || ctrl))
                return sample_editor_scroll(1, TRUE);
            else
                return FALSE;
        case GDK_Home:
            if (!(shift || alt))
                return ctrl ? sample_editor_move_to(MOVE_LOOPSTART)
                    : sample_editor_move_to(MOVE_BEG);
            else
                return FALSE;
        case GDK_End:
            if (!(shift || alt))
                return ctrl ? sample_editor_move_to(MOVE_LOOPEND)
                    : sample_editor_move_to(MOVE_END);
            else
                return FALSE;
        default:
            break;
        }

    if (gui_settings.sample_play_loop && IS_SAMPLE_LOOPED(sed->sample->sample) &&
        sed->sample->sample.loopstart >= s && sed->sample->sample.loopend <= e)
        e = 0; /* Indication that the sample is to be looped */
    gui_play_note_no_repeat(keyval, modifiers, pressed,
        sed->sample, s, e, tracker->cursor_ch, TRUE, &is_note, FALSE,
        gui_get_current_instrument(), gui_get_current_sample());
    return is_note;
}

void sample_editor_set_sample(STSample* s)
{
    GSList* l;

    sample_editor_display_set_sample(sed, s);
    sample_editor_update_full(FALSE);

    /* A crutch for informing instances about the current sample update.
       Should be removed when STSample become a GObject and will be able to
       notify other instances itself */
    for (l = sample_update_notify_list; l; l = l->next)
        ((SampleUpdateNotifyFunc)l->data)(s);
}

STSample* sample_editor_get_sample(void)
{
    return sed->sample;
}

static void
sample_editor_display_set_mixer_position(SampleEditorDisplay* s,
    const gint offset)
{
    GtkWidget* w;
    SampleDisplay* sd;

    g_return_if_fail(s != NULL);
    w = GTK_WIDGET(s);
    sd = SAMPLE_DISPLAY(s);

    if (!s->sample)
        return;

    if (offset != s->priv->mixerpos) {
        gint x_min, x_max, mixer_xpos = sample_display_offset_to_xpos(sd,
            offset >= sd->win_start && offset <= sd->win_start + sd->win_length ? offset : s->priv->old_mixerpos);

        s->priv->mixerpos = offset;
        x_min = MAX(0, mixer_xpos - 3);
        x_max = MIN(sd->width, mixer_xpos + 3);

        gtk_widget_queue_draw_area(w, x_min, 0, x_max - x_min, w->allocation.height);
    }
}

static void
sample_editor_update_mixer_position(const double songtime)
{
    audio_mixer_position* p;
    int i;

    if (songtime >= 0.0 && sed->sample && (p = time_buffer_get(audio_mixer_position_tb, songtime))) {
        for (i = 0; i < ARRAY_SIZE(p->dump); i++) {
            if (p->dump[i].current_sample == &sed->sample->sample) {
                sample_editor_display_set_mixer_position(sed, p->dump[i].current_position);
                g_free(p);
                return;
            }
        }
        g_free(p);
    }

    sample_editor_display_set_mixer_position(sed, -1);
}

static gint
sample_editor_update_timeout(gpointer data)
{
    double display_songtime;

    if (current_driver == NULL)
        return FALSE;

    display_songtime = current_driver->get_play_time(current_driver_object);
    sample_editor_update_mixer_position(display_songtime);

    return TRUE;
}

void sample_editor_start_updating(void)
{
    if (gtktimer != -1)
        return;

    gtktimer = g_timeout_add(1000 / gui_settings.scopes_update_freq, sample_editor_update_timeout, NULL);
}

void sample_editor_stop_updating(void)
{
    if (gtktimer == -1)
        return;

    g_source_remove(gtktimer);
    gtktimer = -1;
    sample_editor_update_mixer_position(-1.0);
}

static void
sample_editor_spin_volume_changed(GtkSpinButton* spin)
{
    g_return_if_fail(sed->sample != NULL);

    history_log_spin_button(spin, _("Sample volume setting"), HISTORY_FLAG_LOG_ALL,
        sed->sample->volume);
    sed->sample->volume = gtk_spin_button_get_value_as_int(spin);
}

static void
sample_editor_spin_panning_changed(GtkSpinButton* spin)
{
    g_return_if_fail(sed->sample != NULL);

    if (sed->sample->sample.data) {
        history_log_spin_button(spin, _("Sample panning setting"), HISTORY_FLAG_LOG_ALL,
            sed->sample->panning);
        sed->sample->panning = gtk_spin_button_get_value_as_int(spin);
    }
}

static void
sample_editor_spin_finetune_changed(GtkSpinButton* spin)
{
    if (!sed->sample)
        return;

    if (sed->sample->sample.data) {
        history_log_spin_button(spin, _("Sample finetune setting"), HISTORY_FLAG_LOG_ALL,
            sed->sample->finetune);
        sed->sample->finetune = gtk_spin_button_get_value_as_int(spin);
    }
}

static void
sample_editor_spin_relnote_changed(GtkSpinButton* spin)
{
    if (!sed->sample)
        return;

    history_log_spin_button(spin, _("Sample relnote setting"), HISTORY_FLAG_LOG_ALL,
        sed->sample->relnote);
    sed->sample->relnote = gtk_spin_button_get_value_as_int(spin);
}

struct LoopArg {
    gint start, end;
    STSampleFlags type;
};

static void
sample_loop_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    struct LoopArg* la = arg;
    STSample* s = data;
    gint tmp_val;

    g_assert(s != NULL);

    sample_editor_lock_sample();
    tmp_val = s->sample.loopstart;
    s->sample.loopstart = la->start;
    la->start = tmp_val;
    tmp_val = s->sample.loopend;
    s->sample.loopend = la->end;
    la->end = tmp_val;
    tmp_val = s->sample.flags & ST_SAMPLE_LOOP_MASK;
    s->sample.flags &= ~ST_SAMPLE_LOOP_MASK;
    s->sample.flags |= la->type;
    la->type = tmp_val;
    sample_editor_unlock_sample();

    sample_editor_update();
}

void
sample_editor_selection_to_loop_clicked(void)
{
    int s = sampledisplay->sel_start, e = sampledisplay->sel_end;
    struct LoopArg* la;

    g_return_if_fail(sed->sample != NULL);
    g_return_if_fail(sed->sample->sample.data != NULL);

    if (s == -1) {
        return;
    }

    la = g_new(struct LoopArg, 1);
    la->start = sed->sample->sample.loopstart;
    la->end = sed->sample->sample.loopend;
    la->type = sed->sample->sample.flags & ST_SAMPLE_LOOP_MASK;
    history_log_action(HISTORY_ACTION_POINTER, _("Sample loop setting"), HISTORY_FLAG_LOG_ALL,
        sample_loop_undo, sed->sample, sizeof(struct LoopArg), la);

    sample_editor_lock_sample();
    sed->sample->sample.loopend = e;
    sed->sample->sample.loopstart = s;
    if (!IS_SAMPLE_LOOPED(sed->sample->sample)) {
        sed->sample->sample.flags |= ST_SAMPLE_LOOPTYPE_AMIGA;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(loopradio[1]), TRUE);
    }
    sample_editor_unlock_sample();

    sample_editor_blocked_set_loop_spins(s, e);
    sample_display_set_loop(sampledisplay, s, e);
}

static void
sample_editor_display_loop_changed(SampleEditorDisplay* sd,
    int start,
    int end)
{
    g_return_if_fail(sd->sample != NULL);
    g_return_if_fail(sd->sample->sample.data != NULL);
    g_return_if_fail(IS_SAMPLE_LOOPED(sd->sample->sample));
    g_return_if_fail(start < end);

    if (start != sd->sample->sample.loopstart)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPSTART]), start);
    if (end != sd->sample->sample.loopend)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(se_spins[SAMPLE_EDITOR_LOOPEND]), end);
}

static void
sample_editor_display_selection_changed(SampleEditorDisplay* s,
    int start,
    int end)
{
    g_return_if_fail(s->sample != NULL);
    g_return_if_fail(s->sample->sample.data != NULL);
    g_return_if_fail(start < end);

    if (s->priv->update_status_func)
        s->priv->update_status_func(s, s->priv->cursor_in,
                sample_display_xpos_to_offset(SAMPLE_DISPLAY(s), s->priv->last_xpos),
                s->priv->status_data);
}

static void
sample_editor_display_window_changed(SampleEditorDisplay* sd,
    int start,
    int end)
{
    SampleDisplay* s = SAMPLE_DISPLAY(sd);
    SampleEditorDisplayPriv* p = sd->priv;
    const gint w = end - start;
    const gdouble step = (s->width == 0 || s->width >= w ) ? 1.0 : (gdouble)w / (gdouble)s->width;

    if (sd->sample == NULL)
        return;

    g_signal_handler_block(G_OBJECT(p->scroll), p->scrollbar_cb_tag);
    gtk_adjustment_configure(p->adj, start, 0, sd->sample->sample.length,
        step, w > 2 ? w - 2 : 1, w);
    g_signal_handler_unblock(G_OBJECT(p->scroll), p->scrollbar_cb_tag);
}

static void
sample_editor_display_position_changed(SampleEditorDisplay* sd,
   int pos)
{
    SampleEditorDisplayPriv* p = sd->priv;
    if (sd->sample == NULL)
        return;

    g_signal_handler_block(G_OBJECT(p->scroll), p->scrollbar_cb_tag);
    gtk_adjustment_set_value(p->adj, pos);
    g_signal_handler_unblock(G_OBJECT(p->scroll), p->scrollbar_cb_tag);
}

static void
sample_editor_loopradio_changed(GtkToggleButton* tb)
{
    gint n;

    if (!gtk_toggle_button_get_active(tb))
        return;

    n = find_current_toggle(loopradio, 3);
    gtk_widget_set_sensitive(se_spins[SAMPLE_EDITOR_LOOPSTART], n != 0);
    gtk_widget_set_sensitive(se_spins[SAMPLE_EDITOR_LOOPEND], n != 0);
    set_loop_sensitive(n != 0);

    if (sed->sample != NULL) {
        if ((sed->sample->sample.flags & ST_SAMPLE_LOOP_MASK) == n)
            return;

        history_log_radio_group(loopradio, _("Sample loop type setting"),
            HISTORY_FLAG_LOG_ALL, sed->sample->sample.flags & ST_SAMPLE_LOOP_MASK, 3);
        sample_editor_lock_sample();
        sed->sample->sample.flags &= ~ST_SAMPLE_LOOP_MASK;
        sed->sample->sample.flags |= n;
        sample_editor_unlock_sample();

        if (n != ST_SAMPLE_LOOPTYPE_NONE) {
            sample_display_set_loop(sampledisplay, sed->sample->sample.loopstart,
                sed->sample->sample.loopend);
        } else {
            sample_display_set_loop(sampledisplay, -1, 1);
        }
    }
}

struct SampleBackup {
    STSample sample;
    gchar iname[24];
    gint16 data[1];
};

static void
sample_total_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    STSample *sample = data;
    STInstrument *curins;
    struct SampleBackup *sb = arg;
    gsize data_length = sample->sample.length * sizeof(sample->sample.data[0]);
    gint16* tmp_data = NULL;
    STSample tmp_smp;
    gchar tmp_name[24];
    GMutex tmp_lock;

    if (data_length) {
        if (sample->sample.flags & ST_SAMPLE_STEREO)
            data_length <<= 1;
        tmp_data = g_malloc(data_length);
        if (!tmp_data) {
            gui_oom_error();
            return;
        }
        memcpy(tmp_data, sample->sample.data, data_length);
    }
    tmp_smp = *sample;

    g_mutex_lock(&sample->sample.lock);
    tmp_lock = sample->sample.lock;
    if (sample->sample.length && sample->sample.data)
        g_free(sample->sample.data);
    *sample = sb->sample;
    if (sample->sample.length) {
        gsize len = (sample->sample.flags & ST_SAMPLE_STEREO ?
            sample->sample.length << 1 : sample->sample.length) * sizeof(sample->sample.data[0]);
        sample->sample.data = g_malloc(len);
        memcpy(sample->sample.data, sb->data, len);
    } else
        sample->sample.data = NULL;
    sample->sample.lock = tmp_lock;
    g_mutex_unlock(&sample->sample.lock);

    sb->sample = tmp_smp;
    if (tmp_data) {
        memcpy(sb->data, tmp_data, data_length);
        g_free(tmp_data);
    }

    if (sample == sed->sample) {
        gint ss, se;
        gboolean is_sel = get_selection(sampledisplay, sample, &ss, &se);

        sample_editor_update();
        if (is_sel && se < sample->sample.length)
            sample_display_set_selection(sampledisplay, ss, se);
    }
    curins = instrument_editor_get_instrument();
    strcpy(tmp_name, curins->utf_name);
    strcpy(curins->utf_name, sb->iname);
    strcpy(sb->iname, tmp_name);
    curins->needs_conversion = TRUE;
    instrument_editor_update(TRUE);
}

gboolean
sample_editor_check_and_log_sample(STSample* sample,
    const gchar* title,
    const gint flags,
    const gsize data_length)
{
    gsize arg_size = sample->sample.length;

    if (sample->sample.flags & ST_SAMPLE_STEREO)
        arg_size <<= 1;
    arg_size = (MAX(arg_size, data_length) - 1) * sizeof(sample->sample.data[0])
        + sizeof(struct SampleBackup);

    if (history_check_size(arg_size)) {
        /* Undo is possible */
        struct SampleBackup* arg = g_malloc(arg_size);
        gsize len = sample->sample.length * sizeof(sample->sample.data[0]);

        if (sample->sample.flags & ST_SAMPLE_STEREO)
            len <<= 1;
        if (!arg) {
            gui_oom_error();
            return FALSE;
        }
        arg->sample = *sample;
        memcpy(arg->data, sample->sample.data, len);
        strcpy(arg->iname, instrument_editor_get_instrument()->utf_name);
        history_log_action(HISTORY_ACTION_POINTER, _(title), flags | HISTORY_FLAG_LOG_INS,
            sample_total_undo, sample, arg_size, arg);
    } else if (!history_query_oversized(mainwindow))
        /* User rejected to continue without undo */
        return FALSE;

    return TRUE;
}

struct RangeBackup {
    gsize start, end;
    gint16 data[1];
};

static void
sample_range_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    STSample *sample = data;
    struct RangeBackup *rb = arg;
    const gsize offset = rb->end - rb->start;
    const gsize data_length = offset * sizeof(sample->sample.data[0]);
    gint16* tmp_data = g_malloc(sample->sample.flags & ST_SAMPLE_STEREO ? data_length << 1 : data_length);

    if (!tmp_data) {
        gui_oom_error();
        return;
    }
    memcpy(tmp_data, &sample->sample.data[rb->start], data_length);
    if (sample->sample.flags & ST_SAMPLE_STEREO)
        memcpy(&tmp_data[offset], &sample->sample.data[rb->start + sample->sample.length], data_length);
    g_mutex_lock(&sample->sample.lock);
    memcpy(&sample->sample.data[rb->start], rb->data, data_length);
    if (sample->sample.flags & ST_SAMPLE_STEREO)
        memcpy(&sample->sample.data[rb->start + sample->sample.length], &rb->data[offset], data_length);
    g_mutex_unlock(&sample->sample.lock);
    memcpy(rb->data, tmp_data,
        sample->sample.flags & ST_SAMPLE_STEREO ? data_length << 1 : data_length);

    g_free(tmp_data);
    if (sample == sed->sample) {
        gint ss, se;
        gboolean is_sel = get_selection(sampledisplay, sample, &ss, &se);

        sample_editor_update();
        if (is_sel)
            sample_display_set_selection(sampledisplay, ss, se);
    }
}

static gboolean
sample_editor_check_and_log_range(STSample* sample,
    const gchar* title,
    const gint flags,
    const gsize start,
    const gsize end)
{
    gsize arg_size = (end - start - 1) * sizeof(sample->sample.data[0]);

    if (sample->sample.flags & ST_SAMPLE_STEREO)
        arg_size <<= 1;
    arg_size += sizeof(struct RangeBackup);

    g_assert(end > start);

    if (history_check_size(arg_size)) {
        /* Undo is possible */
        struct RangeBackup* arg = g_malloc(arg_size);
        const gsize byteslen = (end - start) * sizeof(sample->sample.data[0]);

        if (!arg) {
            gui_oom_error();
            return FALSE;
        }
        arg->start = start;
        arg->end = end;
        memcpy(arg->data, &sample->sample.data[start], byteslen);
        if (sample->sample.flags & ST_SAMPLE_STEREO)
            memcpy(&arg->data[end - start],
                &sample->sample.data[start + sample->sample.length], byteslen);
        history_log_action(HISTORY_ACTION_POINTER, _(title), flags,
            sample_range_undo, sample, arg_size, arg);
    } else if (!history_query_oversized(mainwindow))
        /* User rejected to continue without undo */
        return FALSE;

    return TRUE;
}

static void
sample_editor_do_clear(const gchar* title)
{
    STInstrument* instr;
    gint nsam;

    sample_editor_lock_sample();

    instr = instrument_editor_get_instrument();
    nsam = st_instrument_num_samples(instr);
    if (!nsam) /* All samples are empty, nothing to do */
        return;

    if (nsam > 1) {
        if (!sample_editor_check_and_log_sample(sed->sample,
            title, HISTORY_FLAG_LOG_ALL, 0))
            return;
    } else {
        if (!instrument_editor_check_and_log_instrument(instr,
            title, HISTORY_FLAG_LOG_ALL, 0))
            return;
    }
    st_clean_sample_full(sed->sample, NULL, NULL, FALSE);

    if (nsam == 1) /* Was 1 _before_ cleaning */
        st_clean_instrument_full(instr, NULL, FALSE);

    instrument_editor_update(TRUE);
    sample_editor_update();

    sample_editor_unlock_sample();
}

void
sample_editor_clear_clicked(void)
{
    sample_editor_do_clear(N_("Sample cleaning"));
}

void
sample_editor_crop_clicked(void)
{
    gint l, start = sampledisplay->sel_start, end = sampledisplay->sel_end;

    if (sed->sample == NULL || start == -1)
        return;

    if (!sample_editor_check_and_log_sample(sed->sample,
        N_("Sample cropping"), HISTORY_FLAG_LOG_ALL, 0))
        return;
    l = sed->sample->sample.length;

    sample_editor_log_action(sample_editor_crop_clicked);
    sample_editor_lock_sample();
    sample_editor_delete(sed->sample, 0, start);
    sample_editor_delete(sed->sample, end - start, l - start);
    sample_editor_unlock_sample();

    sample_editor_update();
}

void
sample_editor_show_all_clicked(void)
{
    if (sed->sample == NULL || sed->sample->sample.data == NULL)
        return;
    sample_display_set_window(sampledisplay, 0, sed->sample->sample.length);
}

void
sample_editor_zoom_in_clicked(void)
{
    sample_editor_display_zoom_in(sed);
}

void
sample_editor_zoom_out_clicked(void)
{
    sample_editor_display_zoom_out(sed);
}

void
sample_editor_display_zoom_to_selection(SampleEditorDisplay* s)
{
    gint ss, se, sl;
    SampleDisplay* sd;

    g_assert(IS_SAMPLE_EDITOR_DISPLAY(s));
    sd = SAMPLE_DISPLAY(s);

    if (s->sample == NULL ||
        s->sample->sample.data == NULL ||
        sd->sel_start == -1)
        return;

    sl = s->sample->sample.length;
    if (sl <= 2)
        return;

    ss = sd->sel_start;
    se = sd->sel_end;
    /* There's no sence in zooming to one sample, but this causes some problems.
       In order to avoid this we restrict the minimal zoom size to 2 samples */
    if (se <= ss + 1)
        se = ss + 2;
    if (se >= sl) {
        ss -= (se - sl);
        se = sl;
    }

    sample_display_set_window(sd, ss, se);
}

void
sample_editor_zoom_to_selection_clicked(void)
{
    sample_editor_display_zoom_to_selection(sed);
}

static void
copy_smp_to_tmp(STSample* smp)
{
    if (!tmp_sample)
        tmp_sample = g_new0(STSample, 1);
    if (!tmp_sample) {
        gui_oom_error();
        return;
    }
    st_copy_sample(smp, tmp_sample);
}

void
sample_editor_xcopy_samples(STSample* src_smp,
    STSample* dest_smp,
    const gboolean xchg)
{
    if (xchg) {
        /* We need a place to store one of the samples during the exchange */
        copy_smp_to_tmp(dest_smp);
    }
    g_mutex_lock(&dest_smp->sample.lock);
    st_copy_sample(src_smp, dest_smp);
    g_mutex_unlock(&dest_smp->sample.lock);
    if (xchg) {
        g_mutex_lock(&src_smp->sample.lock);
        st_copy_sample(tmp_sample, src_smp);
        g_mutex_unlock(&src_smp->sample.lock);
    }
}

void sample_editor_copy_cut_common(gboolean copy,
    gboolean spliceout)
{
    int cutlen, newlen;
    gint16* newsample;
    STSample* oldsample = sed->sample;
    int ss = sampledisplay->sel_start, se;

    if (oldsample == NULL || oldsample->sample.length == 0)
        return;

    se = sampledisplay->sel_end;

    cutlen = se - ss;
    newlen = oldsample->sample.length - cutlen;

    if (copy) {
        if (ss == -1) { /* Whole sample */
            copy_smp_to_tmp(oldsample);
            copy_whole_sample = TRUE;
            newlen = 0; /* If we will cut sample, cut it all */
        } else {
            gsize byteslen = oldsample->sample.flags & ST_SAMPLE_STEREO ? cutlen << 1 : cutlen;
            byteslen *= sizeof(oldsample->sample.data[0]);

            if (copybufferlen < byteslen) {
                free(copybuffer);
                copybufferlen = byteslen + (byteslen >> 1);
                copybuffer = malloc(copybufferlen);
                if (!copybuffer) {
                    gui_oom_error();
                    return;
                }
            }

            memcpy(copybuffer,
                oldsample->sample.data + ss,
                cutlen * sizeof(oldsample->sample.data[0]));
            if (oldsample->sample.flags & ST_SAMPLE_STEREO)
                memcpy(&copybuffer[cutlen],
                    oldsample->sample.data + ss + oldsample->sample.length,
                    cutlen * sizeof(oldsample->sample.data[0]));

            copybuffer_sampleinfo = *oldsample;
            copybuffer_sampleinfo.sample.length = cutlen;
            copy_whole_sample = FALSE;
        }
    }

    if (!spliceout || ss == -1)
        return;

    if (newlen == 0) {
        sample_editor_do_clear(N_("Sample cut"));
        return;
    }

    if (!sample_editor_check_and_log_sample(sed->sample,
        N_("Sample cut"), HISTORY_FLAG_LOG_ALL, 0))
        return;

    newsample = malloc((oldsample->sample.flags & ST_SAMPLE_STEREO ? newlen << 1 : newlen) *
        sizeof(oldsample->sample.data[0]));
    if (!newsample) {
        gui_oom_error();
        return;
    }

    sample_editor_lock_sample();

    memcpy(newsample,
        oldsample->sample.data,
        ss * sizeof(oldsample->sample.data[0]));
    memcpy(newsample + ss,
        oldsample->sample.data + se,
        (oldsample->sample.length - se) * sizeof(oldsample->sample.data[0]));
    if (oldsample->sample.flags & ST_SAMPLE_STEREO) {
        memcpy(newsample + newlen,
            oldsample->sample.data + oldsample->sample.length,
            ss * sizeof(oldsample->sample.data[0]));
        memcpy(newsample + ss + newlen,
            oldsample->sample.data + se + oldsample->sample.length,
            (oldsample->sample.length - se) * sizeof(oldsample->sample.data[0]));
    }

    free(oldsample->sample.data);

    oldsample->sample.data = newsample;
    oldsample->sample.length = newlen;

    /* Move loop start and end along with splice */
    if (oldsample->sample.loopstart > ss && oldsample->sample.loopend < se) {
        /* loop was wholly within selection -- remove it */
        oldsample->sample.flags &= ~ST_SAMPLE_LOOP_MASK;
        oldsample->sample.loopstart = 0;
        oldsample->sample.loopend = 1;
    } else {
        if (oldsample->sample.loopstart > se) {
            /* loopstart was after selection */
            oldsample->sample.loopstart -= (se - ss);
        } else if (oldsample->sample.loopstart > ss) {
            /* loopstart was within selection */
            oldsample->sample.loopstart = ss;
        }

        if (oldsample->sample.loopend > se) {
            /* loopend was after selection */
            oldsample->sample.loopend -= (se - ss);
        } else if (oldsample->sample.loopend > ss) {
            /* loopend was within selection */
            oldsample->sample.loopend = ss;
        }
    }

    st_sample_fix_loop(oldsample);
    sample_editor_unlock_sample();
    sample_editor_set_sample(oldsample);
}

void
sample_editor_cut_clicked(void)
{
    sample_editor_copy_cut_common(TRUE, TRUE);
}

void
sample_editor_copy_clicked(void)
{
    sample_editor_copy_cut_common(TRUE, FALSE);
}

static void
sample_editor_init_sample_full(STSample* sample,
    const char* samplename,
    const gboolean modify_ins_name,
    const gboolean modify_smp_name)
{
    STInstrument* instr;

    instr = instrument_editor_get_instrument();
    if (st_instrument_num_samples(instr) == 0) {
        st_clean_instrument_full2(instr, samplename, modify_ins_name, modify_smp_name);
        memset(instr->samplemap, gui_get_current_sample(), sizeof(instr->samplemap));
        if (samplename && modify_smp_name)
            st_set_sample_name(sample, samplename);
    } else
        st_clean_sample_full(sample, samplename, NULL, modify_smp_name);

    sample->volume = 64;
    sample->finetune = 0;
    sample->panning = 0;
    sample->relnote = 0;
}

static inline void
sample_editor_init_sample(const char* samplename)
{
    sample_editor_init_sample_full(sed->sample, samplename, TRUE, TRUE);
}

/* Code CNP'ed from gtkdialog.c */
typedef struct {
    gint response_id;
    GMainLoop* loop;
} RunInfo;

static void
shutdown_loop(RunInfo *ri)
{
    if (ri->loop && g_main_loop_is_running(ri->loop))
        g_main_loop_quit(ri->loop);
}

static void
run_unmap_handler(GtkDialog *dialog, gpointer data)
{
    RunInfo *ri = data;

    shutdown_loop(ri);
}

static void
run_response_handler(GtkDialog *dialog,
    gint response_id,
    gpointer data)
{
    RunInfo *ri;

    ri = data;
    ri->response_id = response_id;
    shutdown_loop(ri);
}

static gint
run_delete_handler(GtkDialog *dialog,
    GdkEventAny *event,
    gpointer data)
{
    RunInfo *ri = data;

    shutdown_loop(ri);
    return TRUE; /* Do not destroy */
}

static gint
sample_editor_stereo_dialog_run(void)
{
    static GtkWidget* window = NULL;
    static GtkWidget* stereo_buttons[3];
    static RunInfo ri = {GTK_RESPONSE_NONE, NULL};

    if (!window) {
        static const gchar* labels[] = {N_("Mix Left and Right"), N_("Left"), N_("Right")};
        GtkWidget *thing, *box1;

        window = gtk_dialog_new_with_buttons(_("Stereo to mono pasting"),
            GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(window, 2);
        box1 = gtk_dialog_get_content_area(GTK_DIALOG(window));
        gtk_box_set_spacing(GTK_BOX(box1), 2);
        g_object_set(window, "has-separator", TRUE, NULL);
        g_signal_connect(window, "response",
            G_CALLBACK(run_response_handler), &ri);
        g_signal_connect(window, "unmap",
            G_CALLBACK(run_unmap_handler), &ri);
        g_signal_connect(window, "delete-event",
            G_CALLBACK(run_delete_handler), &ri);

        thing = gtk_label_new("You are about to paste a stereo fragment to a mono sample.\n"
            "Please choose the data source:");
        gtk_label_set_justify(GTK_LABEL(thing), GTK_JUSTIFY_CENTER);
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
        gtk_widget_show(thing);

        make_radio_group(labels, box1, stereo_buttons, FALSE, FALSE, NULL);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stereo_buttons[0]), TRUE);
    }

    gtk_window_present(GTK_WINDOW(window));
    g_object_ref(window);
    ri.loop = g_main_loop_new(NULL, FALSE);
    GDK_THREADS_LEAVE();
    g_main_loop_run(ri.loop);
    GDK_THREADS_ENTER();
    g_main_loop_unref(ri.loop);
    ri.loop = NULL;
    g_object_unref(window);
    gtk_widget_hide(window);

    if (ri.response_id != GTK_RESPONSE_OK)
        return GTK_RESPONSE_NONE;
    return find_current_toggle(stereo_buttons, 3);
}

typedef enum {
    PASTE_MODE_MONO = 0,
    PASTE_MODE_MIX,
    PASTE_MODE_L,
    PASTE_MODE_R,
    PASTE_MODE_BOTH,
} SamplePasteMode;

void sample_editor_paste_clicked(void)
{
    gint16* newsample;
    STSample* oldsample = sed->sample;
    gint ss = sampledisplay->sel_start, newlen;
    gboolean update_ie = FALSE;
    SamplePasteMode mode = PASTE_MODE_MONO;
    gsize num_samples = copybuffer_sampleinfo.sample.length;

    if (oldsample == NULL)
        return;

    if (copy_whole_sample) {
        gsize byteslen;

        if (!tmp_sample)
            return;

        if (!oldsample->sample.data) {
            STInstrument* curins = instrument_editor_get_instrument();

            if (!st_instrument_num_samples(curins)) {
                /* pasting into empty instrument; backing up the whole instrument */
                gsize i_size = num_samples * sizeof(oldsample->sample.data[0]);

                if (copybuffer_sampleinfo.sample.flags & ST_SAMPLE_STEREO)
                    i_size <<= 1;
                i_size += sizeof(STInstrument);

                if (!instrument_editor_check_and_log_instrument(curins, N_("Sample paste"),
                    HISTORY_FLAG_LOG_ALL, i_size))
                    return;
                sample_editor_init_sample(NULL);
            } else {
                byteslen = tmp_sample->sample.length;
                if (tmp_sample->sample.flags & ST_SAMPLE_STEREO)
                    byteslen <<= 1;
                if (!sample_editor_check_and_log_sample(sed->sample,
                    N_("Sample paste"), HISTORY_FLAG_LOG_ALL, byteslen))
                    return;
            }
            update_ie = TRUE;
        } else {
            byteslen = tmp_sample->sample.flags & ST_SAMPLE_STEREO ?
                tmp_sample->sample.length << 1 : tmp_sample->sample.length;

            if (!sample_editor_check_and_log_sample(sed->sample,
                N_("Sample paste"), HISTORY_FLAG_LOG_ALL, byteslen))
                return;
        }
        sample_editor_lock_sample();
        st_copy_sample(tmp_sample, oldsample);
    } else {
        gsize newlen2;

        if ((oldsample->sample.data && ss == -1) || copybuffer == NULL)
            return;
        newlen = oldsample->sample.length + num_samples;
        newlen2 = oldsample->sample.flags & ST_SAMPLE_STEREO ? newlen << 1 : newlen;

        if (!oldsample->sample.data) {
            STInstrument* curins = instrument_editor_get_instrument();

            /* Pasting stereo to empty */
            if (copybuffer_sampleinfo.sample.flags & ST_SAMPLE_STEREO)
                newlen2 <<= 1;

            if (!st_instrument_num_samples(curins)) {
                /* pasting into empty instrument; backing up the whole instrument */
                gsize i_size = num_samples * sizeof(oldsample->sample.data[0]);

                if (copybuffer_sampleinfo.sample.flags & ST_SAMPLE_STEREO)
                    i_size <<= 1;
                i_size += sizeof(STInstrument);

                if (!instrument_editor_check_and_log_instrument(curins, N_("Sample paste"),
                    HISTORY_FLAG_LOG_ALL, i_size))
                    return;
            } else {
                if (!sample_editor_check_and_log_sample(sed->sample,
                    N_("Sample paste"), HISTORY_FLAG_LOG_ALL, newlen2))
                    return;
            }
            /* pasting into empty sample */
            sample_editor_lock_sample();
            sample_editor_init_sample(_("<just pasted>")); /* Use only charachers from FT2 codeset in the translation! */
            oldsample->sample.flags = copybuffer_sampleinfo.sample.flags;
            oldsample->volume = copybuffer_sampleinfo.volume;
            oldsample->finetune = copybuffer_sampleinfo.finetune;
            oldsample->panning = copybuffer_sampleinfo.panning;
            oldsample->relnote = copybuffer_sampleinfo.relnote;
            sample_editor_unlock_sample();
            ss = 0;
            update_ie = TRUE;
        } else {
            /* Pasting mono fragment to both sterep channels */
            if (oldsample->sample.flags & ST_SAMPLE_STEREO &&
                !(copybuffer_sampleinfo.sample.flags & ST_SAMPLE_STEREO))
                mode = PASTE_MODE_BOTH;
            /* Pasting stereo fragment to mono -- query */
            if (!(oldsample->sample.flags & ST_SAMPLE_STEREO) &&
                (copybuffer_sampleinfo.sample.flags & ST_SAMPLE_STEREO)) {
                gint resp = sample_editor_stereo_dialog_run();

                if (resp < 0)
                    return;
                mode = resp + PASTE_MODE_MIX;
            }

            if (!sample_editor_check_and_log_sample(sed->sample,
                N_("Sample paste"), HISTORY_FLAG_LOG_ALL, newlen2))
                return;
        }

        newsample = malloc(newlen2 * sizeof(oldsample->sample.data[0]));
        if (!newsample) {
            gui_oom_error();
            return;
        }

        sample_editor_lock_sample();

        if (mode == PASTE_MODE_MIX) {
            guint i;

            memcpy(newsample,
                oldsample->sample.data,
                ss * sizeof(oldsample->sample.data[0]));
            for (i = 0; i < num_samples; i++) {
                gint32 tmp = copybuffer[i] + copybuffer[i + num_samples];

                newsample[ss + i] = tmp >> 1;
            }
            memcpy(newsample + (ss + num_samples),
                oldsample->sample.data + ss,
                (oldsample->sample.length - ss) * sizeof(oldsample->sample.data[0]));
        } else {
            if (mode == PASTE_MODE_MONO ||
                mode == PASTE_MODE_L ||
                mode == PASTE_MODE_BOTH) {
                memcpy(newsample,
                    oldsample->sample.data,
                    ss * sizeof(oldsample->sample.data[0]));
                memcpy(newsample + ss, copybuffer,
                    num_samples * sizeof(oldsample->sample.data[0]));
                memcpy(newsample + (ss + num_samples),
                    oldsample->sample.data + ss,
                    (oldsample->sample.length - ss) * sizeof(oldsample->sample.data[0]));
            }

            if (mode == PASTE_MODE_BOTH) {
                memcpy(&newsample[newlen],
                    oldsample->sample.data,
                    ss * sizeof(oldsample->sample.data[0]));
                memcpy(newsample + ss + newlen, copybuffer,
                    num_samples * sizeof(oldsample->sample.data[0]));
                memcpy(newsample + (ss + num_samples) + newlen,
                    oldsample->sample.data + ss,
                    (oldsample->sample.length - ss) * sizeof(oldsample->sample.data[0]));
            } else if (mode == PASTE_MODE_R) {
                memcpy(newsample,
                    oldsample->sample.data,
                    ss * sizeof(oldsample->sample.data[0]));
                memcpy(newsample + ss, &copybuffer[num_samples],
                    num_samples * sizeof(oldsample->sample.data[0]));
                memcpy(newsample + (ss + num_samples),
                    oldsample->sample.data + ss,
                    (oldsample->sample.length - ss) * sizeof(oldsample->sample.data[0]));
            }

        /* Pasting stereo to stereo is performed with MONO mode ;-) */
            if (oldsample->sample.flags & ST_SAMPLE_STEREO &&
                copybuffer_sampleinfo.sample.flags  & ST_SAMPLE_STEREO) {
                memcpy(&newsample[newlen],
                    &oldsample->sample.data[oldsample->sample.length],
                    ss * sizeof(oldsample->sample.data[0]));
                memcpy(newsample + ss + newlen, &copybuffer[num_samples],
                    num_samples * sizeof(oldsample->sample.data[0]));
                memcpy(newsample + (ss + num_samples) + newlen,
                    oldsample->sample.data + ss + oldsample->sample.length,
                    (oldsample->sample.length - ss) * sizeof(oldsample->sample.data[0]));
            }
        }

        free(oldsample->sample.data);

        oldsample->sample.data = newsample;
        oldsample->sample.length = newlen;
        if (oldsample->sample.loopstart >= ss)
            oldsample->sample.loopstart += num_samples;
        if (oldsample->sample.loopend >= ss)
            oldsample->sample.loopend += num_samples;
    }

    sample_editor_unlock_sample();
    sample_editor_update();
    if (update_ie)
        instrument_editor_update(TRUE);
}

void
sample_editor_reverse_clicked(void)
{
    gint ss, se;
    int i;
    gint16 *p, *q;
    gboolean set_selection;
    STSample* s = sed->sample;

    if (!sed->sample) {
        return;
    }
    set_selection = get_selection(sampledisplay, s, &ss, &se);

    if (!sample_editor_check_and_log_range(s,
        N_("Sample reversing"), HISTORY_FLAG_LOG_ALL, ss, se))
        return;
    sample_editor_log_action(sample_editor_reverse_clicked);

    sample_editor_lock_sample();

    p = q = s->sample.data;
    p += ss;
    q += se;

    for (i = 0; i < (se - ss) / 2; i++) {
        gint16 t = *p;
        *p++ = *--q;
        *q = t;
    }
    if (s->sample.flags & ST_SAMPLE_STEREO){
        p = q = &s->sample.data[s->sample.length];
        p += ss;
        q += se;

        for (i = 0; i < (se - ss) / 2; i++) {
            gint16 t = *p;
            *p++ = *--q;
            *q = t;
        }
    }

    sample_editor_unlock_sample();
    sample_editor_update();
    if (set_selection)
        sample_display_set_selection(sampledisplay, ss, se);
}

static gboolean
check_and_log_smp_ins(const gchar* title,
    const gint mode,
    STSample* cs)
{
    STInstrument* curins = instrument_editor_get_instrument();

    if (mode == MODE_STEREO_2) {
        /* We overwrite two samples.
           The whole instrument is to be backed up */
        if (!instrument_editor_check_and_log_instrument(curins, title,
            HISTORY_FLAG_LOG_SMP | HISTORY_FLAG_LOG_INS |
            HISTORY_SET_PAGE(NOTEBOOK_PAGE_SAMPLE_EDITOR), 0))
            /* The length of the new instrument is not greater than the old one,
               if a stereo sample is converted to two mono ones */
            return FALSE;
    } else {
        /* Only one sample is affected */
        if (!sample_editor_check_and_log_sample(cs, title,
            HISTORY_FLAG_LOG_SMP | HISTORY_FLAG_LOG_INS | HISTORY_SET_PAGE(NOTEBOOK_PAGE_SAMPLE_EDITOR),
            0)) /* Converting to mono always reduces sample size */
            return FALSE;
    }
    return TRUE;
}

void
sample_editor_to_mono(void)
{
    static const gchar* labels[] = { N_("Mix"), N_("Left"), N_("Right"), N_("2 samples") };
    static GtkWidget *window = NULL, *buttons[4];
    STSample *s, *next;
    gint response, cursam, mode;
    gsize i;
    GMutex lock;

    s = sed->sample;
    g_assert(s != NULL);
    if (!(s->sample.flags & ST_SAMPLE_STEREO))
        return;

    if (!window) {
        GtkWidget *thing, *box1;

        window = gtk_dialog_new_with_buttons(_("Converting to Mono"),
            GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(window, 2);
        g_object_set(window, "has-separator", TRUE, NULL);
        box1 = gtk_dialog_get_content_area(GTK_DIALOG(window));
        gtk_box_set_spacing(GTK_BOX(box1), 2);

        thing = gtk_label_new(_("Converting method:"));
        gtk_label_set_justify(GTK_LABEL(thing), GTK_JUSTIFY_CENTER);
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
        gtk_widget_show(thing);

        make_radio_group(labels, box1, buttons, FALSE, FALSE, NULL);
        gtk_widget_set_tooltip_text(buttons[3], _("Put left and right channels into the current sample and the next one"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(buttons[0]), TRUE);
    }

    cursam = modinfo_get_current_sample();
    (cursam == 127 ? gtk_widget_hide : gtk_widget_show)(buttons[3]);
    gtk_window_reshow_with_initial_size(GTK_WINDOW(window));
    response = gtk_dialog_run(GTK_DIALOG(window));
    gtk_widget_hide(window);
    if (response != GTK_RESPONSE_OK)
        return;

    mode = find_current_toggle(buttons, 4) + MODE_STEREO_MIX;
    if (mode == MODE_STEREO_2) {
        next = &(instrument_editor_get_instrument()->samples[cursam + 1]);
        if (next->sample.length) {
            if (!gui_ok_cancel_modal(mainwindow, _("The next sample which is about to be overwritten is not empty!\n"
                                                   "Would you like to overwrite it?")))
                return;
        }
    }
    if (!check_and_log_smp_ins(N_("Converting sample to mono"), mode, s))
        return;
    sample_editor_log_action(sample_editor_to_mono);

    sample_editor_lock_sample();
    s->sample.flags &= ~ST_SAMPLE_STEREO;
    switch (mode) {
    case MODE_STEREO_LEFT:
        /* Nothing to do, just resize sample data */
        break;
    case MODE_STEREO_RIGHT:
        memcpy(s->sample.data, &s->sample.data[s->sample.length],
            s->sample.length * sizeof(s->sample.data[0]));
        break;
    case MODE_STEREO_MIX:
        for (i = 0; i < s->sample.length; i++) {
            gint tmp = s->sample.data[i] + s->sample.data[i + s->sample.length];
            s->sample.data[i] = tmp >> 1;
        }
        break;
    case MODE_STEREO_2:
        g_mutex_lock(&next->sample.lock);

        if (next->sample.data)
            g_free(next->sample.data);
        lock = next->sample.lock;
        next->sample = s->sample;
        next->volume = s->volume;
        next->finetune = s->finetune;
        next->panning = s->panning;
        next->relnote = s->relnote;
        next->sample.lock = lock;
        next->sample.data = g_new(gint16, s->sample.length);
        memcpy(next->sample.data, &s->sample.data[s->sample.length],
            s->sample.length * sizeof(s->sample.data[0]));

        if (gui_playing_mode) {
            mixer->updatesample(&next->sample);
        }
        g_mutex_unlock(&next->sample.lock);
        break;
    default:
        g_assert_not_reached();
    }

    s->sample.data = realloc(s->sample.data, s->sample.length * sizeof(s->sample.data[0]));

    sample_editor_unlock_sample();
    instrument_editor_update(TRUE);
    sample_editor_update();
}

void
sample_editor_to_stereo(void)
{
    STSample *s, *next;
    gint response, cursam, mode = MODE_MONO;
    gsize nextlen = 0, length, newlen;

    s = sed->sample;
    length = s->sample.length;
    g_assert(s != NULL);
    if (s->sample.flags & ST_SAMPLE_STEREO)
        return;

    cursam = modinfo_get_current_sample();
    if (cursam < 127) {
        next = &(instrument_editor_get_instrument()->samples[cursam + 1]);
        nextlen = next->sample.length;

        if (nextlen && !(next->sample.flags & ST_SAMPLE_STEREO)) {
            static const gchar* labels[] = { N_("To Both"), N_("2 samples") };
            static GtkWidget *window = NULL, *buttons[2];

            /* Dialog window only if it makes sense */
            if (!window) {
                GtkWidget *thing, *box1;

                window = gtk_dialog_new_with_buttons(_("Converting to Stereo"),
                    GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
                    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                    GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
                gui_dialog_adjust(window, 2);
                g_object_set(window, "has-separator", TRUE, NULL);
                box1 = gtk_dialog_get_content_area(GTK_DIALOG(window));
                gtk_box_set_spacing(GTK_BOX(box1), 2);

                thing = gtk_label_new(_("Converting method:"));
                gtk_label_set_justify(GTK_LABEL(thing), GTK_JUSTIFY_CENTER);
                gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
                gtk_widget_show(thing);

                make_radio_group(labels, box1, buttons, FALSE, FALSE, NULL);
                gtk_widget_set_tooltip_text(buttons[0], _("Put mono sample to both left and right channels"));
                gtk_widget_set_tooltip_text(buttons[1], _("Put the current sample to the left channel and the next one to the right"));
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(buttons[0]), TRUE);
            }

            response = gtk_dialog_run(GTK_DIALOG(window));
            gtk_widget_hide(window);
            if (response != GTK_RESPONSE_OK)
                return;

            if (find_current_toggle(buttons, 2))
                mode = MODE_STEREO_2;
        }
    }

    newlen = MAX(length, nextlen); /* nextlen is 0 if non-2-samples mode */
    if (!sample_editor_check_and_log_sample(s, N_("Converting sample to stereo"),
        HISTORY_FLAG_LOG_SMP | HISTORY_FLAG_LOG_INS | HISTORY_SET_PAGE(NOTEBOOK_PAGE_SAMPLE_EDITOR),
        newlen << 1))
        return;
    sample_editor_log_action(sample_editor_to_stereo);

    sample_editor_lock_sample();
    s->sample.flags |= ST_SAMPLE_STEREO;
    s->sample.data = realloc(s->sample.data, (newlen * sizeof(s->sample.data)) << 1);
    switch (mode) {
    case MODE_STEREO_2:
        s->sample.length = newlen;
        memcpy(&s->sample.data[newlen], next->sample.data,
            nextlen * sizeof(s->sample.data[0]));
        /* Fill the rest of either sample with zeroes */
        if (length > nextlen)
            memset(&s->sample.data[length + nextlen], 0,
                (length - nextlen) * sizeof(s->sample.data[0]));
        else
            memset(&s->sample.data[length], 0,
                (nextlen - length) * sizeof(s->sample.data[0]));
        break;
    default: /* Both */
        memcpy(&s->sample.data[length], s->sample.data,
            length * sizeof(s->sample.data[0]));
    }

    sample_editor_unlock_sample();
    instrument_editor_update(TRUE);
    sample_editor_update();
}

void
sample_editor_swop_channels(void)
{
    gsize i, l;
    STSample* s = sed->sample;

    g_assert(s != NULL);
    if (!(s->sample.flags & ST_SAMPLE_STEREO))
        return;
    l = s->sample.length;

    if (!sample_editor_check_and_log_sample(s, _("Swopping channels"),
        HISTORY_FLAG_LOG_ALL, 0))
        return;
    sample_editor_log_action(sample_editor_swop_channels);

    sample_editor_lock_sample();
    for (i = 0; i < l; i++) {
        gint16 tmp = s->sample.data[i];

        s->sample.data[i] = s->sample.data[i + l];
        s->sample.data[i + l] = tmp;
    }
    sample_editor_unlock_sample();
    sample_editor_update();
}

void
sample_editor_stereo_separation(void)
{
    gsize i, l;
    gint response;
    gdouble sep;
    STSample* s = sed->sample;
    static GtkWidget* window = NULL;
    static GtkAdjustment* adj;

    g_assert(s != NULL);
    if (!(s->sample.flags & ST_SAMPLE_STEREO))
        return;
    l = s->sample.length;

    if (!window) {
        GtkWidget *thing, *box1, *hbox;

        window = gtk_dialog_new_with_buttons(_("Stereo separation"),
            GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(window, 2);
        g_object_set(window, "has-separator", TRUE, NULL);
        box1 = gtk_dialog_get_content_area(GTK_DIALOG(window));
        gtk_box_set_spacing(GTK_BOX(box1), 4);

        thing = gtk_label_new(_("Stereo separation"));
        gtk_label_set_justify(GTK_LABEL(thing), GTK_JUSTIFY_CENTER);
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);

        hbox = gtk_hbox_new(FALSE, 4);
        adj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, -1.0, 1.0, 0.1, 0.1, 0.0));
        thing = gtk_hscale_new(adj);
        gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
        gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);

        thing = extspinbutton_new(adj, 0.0, 1, FALSE);
        gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(box1), hbox, FALSE, TRUE, 0);
        gtk_widget_show_all(window);
    }
    response = gtk_dialog_run(GTK_DIALOG(window));
    gtk_widget_hide(window);
    if (response != GTK_RESPONSE_OK)
        return;

    if (!sample_editor_check_and_log_sample(s, _("Changing stereo separation"),
        HISTORY_FLAG_LOG_ALL, 0))
        return;
    sample_editor_log_action(sample_editor_stereo_separation);

    sep = gtk_adjustment_get_value(adj) * 0.5;
    sample_editor_lock_sample();
    for (i = 0; i < l; i++) {
        gdouble vl = s->sample.data[i], vr = s->sample.data[i + l];

        s->sample.data[i] = lrint(vl * (1.0 - fabs(sep)) - sep * vr);
        s->sample.data[i + l] = lrint(vr * (1.0 - fabs(sep)) - sep * vl);
    }
    sample_editor_unlock_sample();

    sample_editor_update();
}

void
sample_editor_stereo_balance(void)
{
    gsize i, l;
    gint response;
    gdouble bal;
    STSample* s = sed->sample;
    static GtkWidget* window = NULL;
    static GtkAdjustment* adj;

    g_assert(s != NULL);
    if (!(s->sample.flags & ST_SAMPLE_STEREO))
        return;
    l = s->sample.length;

    if (!window) {
        GtkWidget *thing, *box1, *hbox;

        window = gtk_dialog_new_with_buttons(_("Stereo balance"),
            GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(window, 2);
        g_object_set(window, "has-separator", TRUE, NULL);
        box1 = gtk_dialog_get_content_area(GTK_DIALOG(window));
        gtk_box_set_spacing(GTK_BOX(box1), 4);

        thing = gtk_label_new(_("L - Balance - R"));
        gtk_label_set_justify(GTK_LABEL(thing), GTK_JUSTIFY_CENTER);
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);

        hbox = gtk_hbox_new(FALSE, 4);
        adj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, -1.0, 1.0, 0.1, 0.1, 0.0));
        thing = gtk_hscale_new(adj);
        gtk_scale_set_draw_value(GTK_SCALE(thing), FALSE);
        gtk_box_pack_start(GTK_BOX(hbox), thing, TRUE, TRUE, 0);

        thing = extspinbutton_new(adj, 0.0, 1, FALSE);
        gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(box1), hbox, FALSE, TRUE, 0);
        gtk_widget_show_all(window);
    }
    response = gtk_dialog_run(GTK_DIALOG(window));
    gtk_widget_hide(window);
    if (response != GTK_RESPONSE_OK)
        return;

    if (!sample_editor_check_and_log_sample(s, _("Adjusting stereo balance"),
        HISTORY_FLAG_LOG_ALL, 0))
        return;
    sample_editor_log_action(sample_editor_stereo_balance);

    bal = gtk_adjustment_get_value(adj);
    sample_editor_lock_sample();
    for (i = 0; i < l; i++) {
        gdouble vl = s->sample.data[i], vr = s->sample.data[i + l];

        s->sample.data[i] = lrint(vl * (1.0 - bal) / (1.0 + fabs(bal)));
        s->sample.data[i + l] = lrint(vr * (1.0 + bal) / (1.0 + fabs(bal)));
    }
    sample_editor_unlock_sample();

    sample_editor_update();
}

#if USE_SNDFILE || AUDIOFILE_VERSION

static void
sample_editor_load_wav_main(FILE* f, struct wl* wavload)
{
    void *sbuf, *sbuf_loadto, *tmp;
#if USE_SNDFILE
    sf_count_t len, frames;
#else
    gsize len, frames;
#endif
    gfloat rate;
    gint sampleWidth = wavload->sampleWidth & 0xff;
    gsize bwidth = sampleWidth >> 3;

    frames = len = wavload->frameCount * wavload->channelCount;
    gui_statusbar_update(STATUS_LOADING_SAMPLE, TRUE);

#if USE_SNDFILE
    if (wavload->through_library) {
        if (wavload->isFloat)
            len *= sizeof(float);
        else
            len <<= 1; /* for 16 bit, fix this when changing internal format */
    } else {
#else
        if (wavload->sampleWidth == 24) /* Special case of 24 bit int stored into 32 bit frames */
            bwidth++;
        len *= MAX(2, bwidth);
#endif
#if USE_SNDFILE
    }
#endif
    if (!(tmp = malloc(len))) {
        gui_oom_error();
        goto errnobuf;
    }
    if (wavload->channelCount == 1)
        sbuf = tmp;
    else if (!(sbuf = malloc(len))) {
        gui_oom_error();
        free(tmp);
        goto errnobuf;
    }

    if (sampleWidth > 8) {
        sbuf_loadto = tmp;
    } else {
        sbuf_loadto = tmp + frames;
    }

    if (wavload->through_library) {
#if USE_SNDFILE
        if (wavload->frameCount != (wavload->isFloat ?
            sf_readf_float(wavload->file, sbuf_loadto, wavload->frameCount) :
            sf_readf_short(wavload->file, sbuf_loadto, wavload->frameCount)))
#else
        if (wavload->frameCount != afReadFrames(wavload->file, AF_DEFAULT_TRACK, sbuf_loadto, wavload->frameCount))
#endif
        {
            static GtkWidget* dialog = NULL;

            gui_error_dialog(&dialog, _("Read error."), FALSE);
            goto errnodata;
        }
    } else {
        if (!f)
            goto errnodata;
        if (wavload->frameCount != fread(sbuf_loadto, wavload->channelCount * bwidth, wavload->frameCount, f)) {
            static GtkWidget* dialog = NULL;
            gui_error_dialog(&dialog, _("Read error."), FALSE);
            goto errnodata;
        }
    }

    if (wavload->isFloat) {
        gsize i;

        if (sampleWidth == 32) {
            gfloat temp, mi = 0.0, ma = 0.0;

            /* FP WAVs can violate spec and be out of range [-1.0, 1.0].
               So auto-normalization is provided */
            for (i = 0; i < frames; i++) {
                temp = ((gfloat*)sbuf_loadto)[i];
                if (temp < mi)
                    mi = temp;
                else if (temp > ma)
                    ma = temp;
                ma = MAX(-mi, ma);
            }

            for (i = 0; i < frames; i++) {
                temp = ((gfloat*)sbuf_loadto)[i];
                ((gint16*)tmp)[i] = lrint(temp / ma * 32767.0);
            }
        } else if (sampleWidth == 64) {
            gdouble temp, mi = 0.0, ma = 0.0;

            for (i = 0; i < frames; i++) {
                temp = ((gdouble*)sbuf_loadto)[i];
                if (temp < mi)
                    mi = temp;
                else if (temp > ma)
                    ma = temp;
                ma = MAX(-mi, ma);
            }

            for (i = 0; i < frames; i++) {
                temp = ((gdouble*)sbuf_loadto)[i];
                ((gint16*)tmp)[i] = lrint(temp / ma * 32767.0);
            }
        } else {
            static GtkWidget* dialog = NULL;
            static gchar* line = NULL;

            if (!line)
                line = g_strdup_printf(_("Unsupported file format (%i bit float)."), sampleWidth);

            gui_error_dialog(&dialog, line, FALSE);
            goto errnodata;
        }
    } else {
        if (sampleWidth != 16)
            st_convert_sample(sbuf_loadto, tmp, wavload->sampleWidth, 16, frames, wavload->src_be, wavload->dst_be);
        else if (!wavload->through_library && wavload->src_be != wavload->dst_be)
            byteswap_16_array(tmp, frames);
        if (wavload->unsignedwords)
            st_sample_16bit_signed_unsigned(tmp, frames);
    }

    if (wavload->channelCount == 2)
        st_convert_to_nonint(tmp, sbuf, wavload->frameCount, wavload->frameCount);

    if (!sample_editor_check_and_log_sample(sed->sample,
        N_("Sample loading"), HISTORY_FLAG_LOG_SMP | HISTORY_SET_PAGE(NOTEBOOK_PAGE_SAMPLE_EDITOR), frames))
        goto errnodata;
    sample_editor_lock_sample();
    sample_editor_init_sample_full(sed->sample, wavload->samplename,
        gui_settings.ins_name_overwrite, gui_settings.smp_name_overwrite);
    if (sampleWidth >= 16)
        sed->sample->sample.flags |= ST_SAMPLE_16_BIT;
    else
        sed->sample->sample.flags &= ~ST_SAMPLE_16_BIT;
    if (wavload->channelCount == 2)
        sed->sample->sample.flags |= ST_SAMPLE_STEREO;
    else
        sed->sample->sample.flags &= ~ST_SAMPLE_STEREO;
    sed->sample->sample.length = wavload->frameCount;

    // Initialize relnote and finetune such that sample is played in original speed
    if (wavload->through_library) {
#if USE_SNDFILE
        if (wavload->wavinfo.format == SF_FORMAT_PCM_S8 ||
            wavload->wavinfo.format == SF_FORMAT_PCM_U8)
            sed->sample->sample.flags &= ~ST_SAMPLE_16_BIT;
        else
            sed->sample->sample.flags |= ST_SAMPLE_16_BIT;
        rate = wavload->wavinfo.samplerate;
#else
        rate = afGetRate(wavload->file, AF_DEFAULT_TRACK);
#endif
    } else {
        rate = wavload->rate;
    }
    xm_freq_note_to_relnote_finetune(rate,
        4 * 12 + 1, // at C-4
        &sed->sample->relnote,
        &sed->sample->finetune);

    if (sbuf != tmp)
        free(tmp);
    if (sampleWidth > 16)
        sbuf = realloc(sbuf, frames << 1);
    sed->sample->sample.data = sbuf;

    sample_editor_unlock_sample();
    instrument_editor_update(TRUE);
    sample_editor_update();
    gui_statusbar_update(STATUS_SAMPLE_LOADED, FALSE);
    return;

errnodata:
    free(sbuf);
    if (sbuf != tmp)
        free(tmp);
errnobuf:
    gui_statusbar_update(STATUS_IDLE, FALSE);
    return;
}

static void
sample_editor_open_raw_sample_dialog(FILE* f, struct wl* wavload)
{
    static GtkWidget *window = NULL, *combo;
    static GtkListStore* ls;

    static GtkWidget* wavload_raw_resolution_w[7];
    static GtkWidget* wavload_raw_channels_w[2];
    static GtkWidget* wavload_raw_signed_w[2];
    static GtkWidget* wavload_raw_endian_w[2];

    GtkWidget* box1;
    GtkWidget* box2;
    GtkWidget* separator;
    GtkWidget* label;
    GtkWidget* thing;
    GtkTreeIter iter;
    gint response, i, active = 0;

    static const char* resolutionlabels[] = { N_("8 bits"), N_("16 bits"), N_("24 (32)"),
        N_("24 bits"), N_("32 bits"), N_("Float"), N_("Double") };
    static const char* signedlabels[] = { N_("Signed"), N_("Unsigned") };
    static const char* endianlabels[] = { N_("Little-Endian"), N_("Big-Endian") };
    static const char* channelslabels[] = { N_("Mono"), N_("Stereo") };

    // Standard sampling rates
    static const guint rates[] = { 8000, 8363, 11025, 12000, 16000, 22050, 24000, 32000, 33452, 44100, 48000 };

    if (!window) {
        window = gtk_dialog_new_with_buttons(_("Load raw sample"), GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

        wavload_dialog = window;
        gui_dialog_adjust(window, GTK_RESPONSE_OK);
        g_object_set(window, "has-separator", TRUE, NULL);

        box1 = gtk_dialog_get_content_area(GTK_DIALOG(window)); // upper part (vbox)
        gtk_box_set_spacing(GTK_BOX(gtk_bin_get_child(GTK_BIN(window))), 4);

        label = gtk_label_new(_("You have selected a sample that is not\nin a known format. You can load the raw data now.\n\nPlease choose a format:"));
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
        gtk_box_pack_start(GTK_BOX(box1), label, FALSE, TRUE, 0);
        gtk_widget_show(label);

        separator = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(box1), separator, FALSE, TRUE, 5);
        gtk_widget_show(separator);

        // The toggles

        box2 = gtk_hbox_new(FALSE, 4);
        gtk_widget_show(box2);
        gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

        thing = gtk_label_new(_("Resolution:"));
        gtk_widget_show(thing);
        gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
        add_empty_hbox(box2);
        make_radio_group(resolutionlabels, box2, wavload_raw_resolution_w,
            FALSE, TRUE, NULL);
        gtk_widget_set_tooltip_text(wavload_raw_resolution_w[2],
            _("Unpacked 24-bit samples stored in 32-bit words"));
        gtk_widget_set_tooltip_text(wavload_raw_resolution_w[5],
            _("Single precision floating-point"));
        gtk_widget_set_tooltip_text(wavload_raw_resolution_w[6],
            _("Double precision floating-point"));

        box2 = gtk_hbox_new(FALSE, 4);
        gtk_widget_show(box2);
        gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

        thing = gtk_label_new(_("Word format:"));
        gtk_widget_show(thing);
        gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
        add_empty_hbox(box2);
        make_radio_group(signedlabels, box2, wavload_raw_signed_w,
            FALSE, TRUE, NULL);

        box2 = gtk_hbox_new(FALSE, 4);
        gtk_widget_show(box2);
        gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

        add_empty_hbox(box2);
        make_radio_group(endianlabels, box2, wavload_raw_endian_w,
            FALSE, TRUE, NULL);

        box2 = gtk_hbox_new(FALSE, 4);
        gtk_widget_show(box2);
        gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

        thing = gtk_label_new(_("Channels:"));
        gtk_widget_show(thing);
        gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
        add_empty_hbox(box2);
        make_radio_group(channelslabels, box2, wavload_raw_channels_w,
            FALSE, TRUE, NULL);

        // Rate selection combo
        box2 = gtk_hbox_new(FALSE, 4);
        gtk_widget_show(box2);
        gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);

        thing = gtk_label_new(_("Sampling Rate:"));
        gtk_widget_show(thing);
        gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
        add_empty_hbox(box2);

        ls = gtk_list_store_new(1, G_TYPE_UINT);
        for (i = 0; i < ARRAY_SIZE(rates); i++) {
            gtk_list_store_append(ls, &iter);
            gtk_list_store_set(ls, &iter, 0, rates[i], -1);
            if (rates[i] == 8363)
                active = i;
        }

        combo = gui_combo_new(ls);
        gtk_box_pack_start(GTK_BOX(box2), combo, FALSE, TRUE, 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active); // default is 8363
        gtk_widget_show(combo);

        gtk_widget_show(window);
    } else
        gtk_window_present(GTK_WINDOW(window));

    response = gtk_dialog_run(GTK_DIALOG(window));
    gtk_widget_hide(window);

    if (response == GTK_RESPONSE_OK) {
        switch (find_current_toggle(wavload_raw_resolution_w,
            ARRAY_SIZE(wavload_raw_resolution_w))) {
        case 0:
            wavload->sampleWidth = 8;
            break;
        case 1:
            wavload->sampleWidth = 16;
            break;
        case 2:
            wavload->sampleWidth = 24;
            break;
        case 3:
            wavload->sampleWidth = 24 | 256; /* Packed 24 bit indicated by 9th bit */
            break;
        case 4:
            wavload->sampleWidth = 32;
            break;
        case 5:
            wavload->isFloat = TRUE;
            wavload->sampleWidth = sizeof(float) << 3;
            break;
        case 6:
            wavload->isFloat = TRUE;
            wavload->sampleWidth = sizeof(double) << 3;
            break;
        default:
            g_assert_not_reached();
        }
        wavload->src_be = find_current_toggle(wavload_raw_endian_w, 2);
#ifdef WORDS_BIGENDIAN
        wavload->dst_be = TRUE;
#else
        wavload->dst_be = FALSE;
#endif
        wavload->channelCount = find_current_toggle(wavload_raw_channels_w, 2) == 1 ? 2 : 1;
        wavload->unsignedwords = find_current_toggle(wavload_raw_signed_w, 2);

        if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
            wavload->rate = 8363;
        else
            gtk_tree_model_get(GTK_TREE_MODEL(ls), &iter, 0, &wavload->rate, -1);

        if (wavload->sampleWidth == 24)
            /* 24 bit stored into 32-bit words */
            wavload->frameCount >>= 2;
        else
            wavload->frameCount /= (wavload->sampleWidth & 0xff) >> 3;
        if (wavload->channelCount == 2) {
            wavload->frameCount >>= 1;
        }
        sample_editor_load_wav_main(f, wavload);
    }
}

static void
sample_editor_load_wav(const gchar* fn, const gchar* localname)
{
    struct wl wavload;

#if USE_SNDFILE != 1
    int sampleFormat;
#endif
    g_return_if_fail(sed->sample != NULL);

    wavload.unsignedwords = FALSE;
    wavload.samplename = strrchr(fn, '/');
    if (!wavload.samplename)
        wavload.samplename = fn;
    else
        wavload.samplename++;

#if USE_SNDFILE
    wavload.wavinfo.format = 0;
    wavload.file = sf_open(localname, SFM_READ, &wavload.wavinfo);
    wavload.wavinfo.format &= SF_FORMAT_SUBMASK;
#else
    wavload.file = afOpenFile(localname, "r", NULL);
#endif
    wavload.isFloat = FALSE;
    if (!wavload.file) {
        FILE* f = gui_fopen(localname, fn, "rb");

        if (f) {
            fseek(f, 0, SEEK_END);
            wavload.frameCount = ftell(f);
            fseek(f, 0, SEEK_SET);
            wavload.through_library = FALSE;
            sample_editor_open_raw_sample_dialog(f, &wavload);
            fclose(f);
        }
        return;
    }

    wavload.through_library = TRUE;
#ifdef WORDS_BIGENDIAN
    wavload.dst_be = TRUE;
#else
    wavload.dst_be = FALSE;
#endif
    wavload.src_be = wavload.dst_be;

#if USE_SNDFILE
    wavload.frameCount = wavload.wavinfo.frames;
#else
    wavload.frameCount = afGetFrameCount(wavload.file, AF_DEFAULT_TRACK);
#endif
    if (wavload.frameCount > mixer->max_sample_length) {
        static GtkWidget* dialog = NULL;
        gui_warning_dialog(&dialog, _("Sample is too long for current mixer module. Loading anyway."), FALSE);
    }

#if USE_SNDFILE

    wavload.channelCount = wavload.wavinfo.channels;
    if (wavload.wavinfo.format == SF_FORMAT_FLOAT ||
        wavload.wavinfo.format == SF_FORMAT_DOUBLE) {
        wavload.isFloat = TRUE;
        wavload.sampleWidth = sizeof(float) * 8;
    } else
        wavload.sampleWidth = 16;

#else

    wavload.channelCount = afGetChannels(wavload.file, AF_DEFAULT_TRACK);
    afGetSampleFormat(wavload.file, AF_DEFAULT_TRACK, &sampleFormat, &wavload.sampleWidth);
    if (sampleFormat == AF_SAMPFMT_FLOAT || sampleFormat == AF_SAMPFMT_DOUBLE)
        wavload.isFloat = TRUE;
    else if (sampleFormat == AF_SAMPFMT_UNSIGNED)
        wavload.unsignedwords = TRUE;

    /* I think audiofile-0.1.7 does this automatically, but I'm not sure */
#ifdef WORDS_BIGENDIAN
    afSetVirtualByteOrder(wavload.file, AF_DEFAULT_TRACK, AF_BYTEORDER_BIGENDIAN);
#else
    afSetVirtualByteOrder(wavload.file, AF_DEFAULT_TRACK, AF_BYTEORDER_LITTLEENDIAN);
#endif

#endif

    if ((wavload.sampleWidth & 7) || wavload.channelCount > 2) {
        static GtkWidget* dialog = NULL;

        gui_error_dialog(&dialog, _("Sound file format is too exotic or it has more than 2 channels"), FALSE);
    } else
        sample_editor_load_wav_main(NULL, &wavload);

#if USE_SNDFILE
    sf_close(wavload.file);
#else
    afCloseFile(wavload.file);
#endif
    return;
}

static void
sample_editor_save_wav_main(const gchar* fn,
    const gchar* localname,
    guint32 offset,
    guint32 length)
{
    static const guint freqs[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 64000, 88200, 96000 };
    static GtkWidget *dialog = NULL, *freq_combo;
    gint response, selected;
    gint16* outbuf;
#if USE_SNDFILE
    SNDFILE* outfile;
    SF_INFO sfinfo;
    int rate;
#else
    AFfilehandle outfile;
    AFfilesetup outfilesetup;
    double rate;
#endif

    if (!dialog) {
        GtkWidget *mainbox, *box2, *thing;
        GtkListStore* ls;
        GtkTreeIter iter;
        guint i;

        dialog = gtk_dialog_new_with_buttons(_("Saving parameters"), GTK_WINDOW(mainwindow),
            GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(dialog, GTK_RESPONSE_OK);
        g_object_set(dialog, "has-separator", TRUE, NULL);

        mainbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        gtk_box_set_spacing(GTK_BOX(mainbox), 2);

        box2 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, FALSE, 0);
        gtk_widget_set_tooltip_text(box2, _("Frequency to be specified in the WAV file."
            "\"From Sample\" means that the frequency will be based on the sample's Relnote "
            "and Finetune values"));

        thing = gtk_label_new(_("Frequency [Hz]:"));
        gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
        ls = gtk_list_store_new(1, G_TYPE_STRING);
        for (i = 0; i < ARRAY_SIZE(freqs); i++) {
            gchar buf[6];

            snprintf(buf, sizeof(buf), "%i", freqs[i]);
            gtk_list_store_append(ls, &iter);
            gtk_list_store_set(ls, &iter, 0, buf, -1);
        }
        gtk_list_store_append(ls, &iter);
        gtk_list_store_set(ls, &iter, 0, _("From sample"), -1);
        freq_combo = gui_combo_new(ls);
        /* 44100 by default */
        gtk_combo_box_set_active(GTK_COMBO_BOX(freq_combo), 5);
        g_object_unref(ls);
        gtk_box_pack_end(GTK_BOX(box2), freq_combo, FALSE, TRUE, 0);

        gtk_widget_show_all(dialog);
    }

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(dialog);

    if (response != GTK_RESPONSE_OK)
        return;

    selected = gtk_combo_box_get_active(GTK_COMBO_BOX(freq_combo));
    rate = selected < (sizeof(freqs) / sizeof(freqs[0])) ?
        freqs[selected] : SAMPLE_GET_RATE(sed->sample);

    gui_statusbar_update(STATUS_SAVING_SAMPLE, TRUE);

#if USE_SNDFILE
    if (sed->sample->sample.flags & ST_SAMPLE_16_BIT) {
        sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
    } else {
        sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_U8;
    }
    sfinfo.channels = sed->sample->sample.flags & ST_SAMPLE_STEREO ? 2 : 1;
    sfinfo.samplerate = rate;
    outfile = sf_open(fn, SFM_WRITE, &sfinfo);
#else
    outfilesetup = afNewFileSetup();
    afInitFileFormat(outfilesetup, AF_FILE_WAVE);
    afInitChannels(outfilesetup, AF_DEFAULT_TRACK, sed->sample->sample.flags & ST_SAMPLE_STEREO ? 2 : 1);
    if (sed->sample->sample.flags & ST_SAMPLE_16_BIT) {
        afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);
    } else {
#if AUDIOFILE_VERSION == 1
        // for audiofile-0.1.x
        afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 8);
#else
        // for audiofile-0.2.x and 0.3.x
        afInitSampleFormat(outfilesetup, AF_DEFAULT_TRACK, AF_SAMPFMT_UNSIGNED, 8);
#endif
    }
    afInitRate(outfilesetup, AF_DEFAULT_TRACK, rate);
    outfile = afOpenFile(localname, "w", outfilesetup);
    afFreeFileSetup(outfilesetup);
#endif

    if (!outfile) {
        static GtkWidget* dialog = NULL;

        gui_error_dialog(&dialog, _("Can't open file for writing."), FALSE);
        return;
    }

    if (sed->sample->sample.flags & ST_SAMPLE_STEREO) {
        outbuf = calloc(length, sizeof(sed->sample->sample.data[0]) << 1);
        g_assert(outbuf);
        st_convert_to_int(sed->sample->sample.data + offset, outbuf,
            sed->sample->sample.length, length);
    } else
        outbuf = sed->sample->sample.data + offset;

#if USE_SNDFILE
    sf_writef_short(outfile, outbuf, length);
    sf_close(outfile);
#else
    if (sed->sample->sample.flags & ST_SAMPLE_16_BIT) {
        afWriteFrames(outfile, AF_DEFAULT_TRACK, outbuf, length);
    } else {
        void* buf = malloc(1 * length);
#ifdef WORDS_BIGENDIAN
        const gboolean is_be = TRUE;
#else
        const gboolean is_be = FALSE;
#endif

        if (!buf)
            gui_oom_error();
        else {
            st_convert_sample(outbuf, buf, 16, 8, length, is_be, is_be);
            st_sample_8bit_signed_unsigned(buf, length);
            afWriteFrames(outfile, AF_DEFAULT_TRACK, buf, length);
            free(buf);
        }
    }
    afCloseFile(outfile);
#endif

    if (outbuf != sed->sample->sample.data + offset)
        g_free(outbuf);
    gui_statusbar_update(STATUS_SAMPLE_SAVED, FALSE);
}

static void
sample_editor_save_wav(const gchar* fn, const gchar* localname)
{
    g_return_if_fail(sed->sample != NULL);
    if (sed->sample->sample.data == NULL) {
        return;
    }
    sample_editor_save_wav_main(fn, localname, 0, sed->sample->sample.length);
}

static void
sample_editor_save_region_wav(const gchar* fn, const gchar* localname)
{
    int rss = sampledisplay->sel_start, rse = sampledisplay->sel_end;

    g_return_if_fail(sed->sample != NULL);
    if (sed->sample->sample.data == NULL) {
        return;
    }

    if (rss == -1) {
        static GtkWidget* dialog = NULL;
        gui_error_dialog(&dialog, _("Please select region first."), FALSE);
        return;
    }
    sample_editor_save_wav_main(fn, localname, rss, rse - rss);
}

#endif /* USE_SNDFILE || AUDIOFILE_VERSION */

/* ============================ Sampling functions coming up -------- */

void
sample_editor_clear_buffers(STSampleChain* bufs)
{
    STSampleChain *r, *r2;

    /* Free allocated sample buffers */
    for (r = bufs; r; r = r2) {
        r2 = r->next;
        free(r->data);
        free(r);
    }
}

static void
enable_widgets(gboolean enable)
{
    gtk_widget_set_sensitive(okbutton, enable);
    gtk_widget_set_sensitive(clearbutton, enable);
}

static void
sampling_response(GtkWidget* dialog, gint response, GtkToggleButton* button)
{
    gtk_widget_hide(samplingwindow);
    sampling = FALSE;

    if (button->active) {
        g_signal_handler_block(G_OBJECT(button), toggled_id); /* To prevent data storing on record stop */
        gtk_toggle_button_set_active(button, FALSE);
        g_signal_handler_unblock(G_OBJECT(button), toggled_id);
    }
    if (monitoring) {
        monitoring = FALSE;
        sampling_driver->release(sampling_driver_object);
    }

    if (response == GTK_RESPONSE_OK)
        sample_editor_ok_clicked();

    clock_stop(CLOCK(sclock));
    clock_set_seconds(CLOCK(sclock), 0);
    sample_editor_clear_buffers(recordbufs);
    sample_display_set_data(monitorscope, NULL, format, 0, FALSE);
    recordbufs = NULL;
    has_data = FALSE;
}

static void
sampling_widget_hide(GtkToggleButton* button)
{
    sampling_response(NULL, GTK_RESPONSE_CANCEL, button);
}

static void
record_toggled(GtkWidget* button)
{
    if (GTK_TOGGLE_BUTTON(button)->active) {
        enable_widgets(FALSE);
        recordedlen = 0;
        if (recordbufs) {
            sample_editor_clear_buffers(recordbufs);
            sample_display_set_data(monitorscope, NULL, format, 0, FALSE);
            recordbufs = NULL;
        }

        if (!monitoring) {
            sampling_driver->open(sampling_driver_object);
            monitoring = TRUE;
        }
        sampling = TRUE;
        gtk_widget_queue_draw(GTK_WIDGET(monitorscope));
        clock_set_seconds(CLOCK(sclock), 0);
        clock_start(CLOCK(sclock));
    } else {
        sampling = FALSE;
        monitoring = FALSE;
        sampling_driver->release(sampling_driver_object);

        has_data = TRUE;
        enable_widgets(has_data);
        if (recordbufs) /* Recordbufs might be not allocated yet */
            sample_display_set_chain(monitorscope, recordbufs, format);
        clock_stop(CLOCK(sclock));
    }
}

void
clear_clicked(void)
{
    has_data = FALSE;
    enable_widgets(has_data);

    recordedlen = 0;
    sample_editor_clear_buffers(recordbufs);
    sample_display_set_data(monitorscope, NULL, format, 0, FALSE);
    recordbufs = NULL;
    if (!monitoring) {
        sampling_driver->open(sampling_driver_object);
        monitoring = TRUE;
    }
    clock_set_seconds(CLOCK(sclock), 0);
}

static gboolean
sampling_window_keyevent(GtkWidget* widget,
    GdkEventKey* event, GtkToggleButton* rec_btn)
{
    gint shift = event->state & GDK_SHIFT_MASK;
    gint ctrl = event->state & GDK_CONTROL_MASK;
    gint alt = event->state & GDK_MOD1_MASK;
    gint m = keys_get_key_meaning(event->keyval, ENCODE_MODIFIERS(shift, ctrl, alt), event->hardware_keycode);

    if (KEYS_MEANING_TYPE(m) == KEYS_MEANING_FUNC) {
        gboolean handled;

        if (KEYS_MEANING_VALUE(m) == KEY_SMP_TOGGLE) {
            gtk_toggle_button_set_active(rec_btn, !gtk_toggle_button_get_active(rec_btn));
            handled = TRUE;
        } else
            handled = gui_handle_standard_keys(shift, ctrl, alt, event->keyval, event->hardware_keycode);

        return handled;
    }

    if (event->keyval == ' ') {
        gui_play_stop();
        return TRUE;
    }

    return FALSE;
}

void
sample_editor_monitor_clicked(void)
{
    if (!sampling_driver || !sampling_driver_object) {
        static GtkWidget* dialog = NULL;

        gui_error_dialog(&dialog, _("No sampling driver available"), FALSE);
        return;
    }

    if (!samplingwindow) {
        GtkWidget *mainbox, *thing, *box2;
        GtkAccelGroup* group = gtk_accel_group_new();
        GClosure* closure;

        samplingwindow = gtk_dialog_new_with_buttons(_("Sampling Window"), GTK_WINDOW(mainwindow), 0,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
        gtk_window_add_accel_group(GTK_WINDOW(samplingwindow), group);
        g_object_set(samplingwindow, "has-separator", TRUE, NULL);

        okbutton = gtk_dialog_add_button(GTK_DIALOG(samplingwindow), GTK_STOCK_OK, GTK_RESPONSE_OK);
        gtk_container_set_border_width(GTK_CONTAINER(samplingwindow), 4);

        mainbox = gtk_dialog_get_content_area(GTK_DIALOG(samplingwindow));

        gtk_container_set_border_width(GTK_CONTAINER(mainbox), 4);
        gtk_box_set_spacing(GTK_BOX(mainbox), 2);

        thing = sample_display_new(FALSE, gui_settings.sampling_mode);
        custom_drawing_register_drawing_func(CUSTOM_DRAWING(thing), red_circle_draw, NULL);
        custom_drawing_register_realize_func(CUSTOM_DRAWING(thing), red_circle_realize, NULL);
        gtk_box_pack_start(GTK_BOX(mainbox), thing, TRUE, TRUE, 0);
        monitorscope = SAMPLE_DISPLAY(thing);

        box2 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box2, FALSE, TRUE, 0);

        thing = gtk_toggle_button_new_with_label(_("Record"));
        closure = g_cclosure_new_swap(G_CALLBACK(sampling_widget_hide), thing, NULL);
        gtk_accel_group_connect(group, GDK_Escape, 0, 0, closure);
        toggled_id = g_signal_connect(thing, "toggled",
            G_CALLBACK(record_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
        gui_dialog_connect_data(samplingwindow, G_CALLBACK(sampling_response), thing);
        g_signal_connect(samplingwindow, "key-press-event",
            G_CALLBACK(sampling_window_keyevent), thing);

        clearbutton = thing = gtk_button_new_with_label(_("Clear"));
        g_signal_connect(thing, "clicked",
            G_CALLBACK(clear_clicked), NULL);
        gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);

        sclock = clock_new();
        clock_set_format(CLOCK(sclock), _("%M:%S"));
        clock_set_seconds(CLOCK(sclock), 0);
        gtk_box_pack_start(GTK_BOX(box2), sclock, FALSE, TRUE, 0);

        gtk_widget_show_all(samplingwindow);
    } else
        gtk_window_present(GTK_WINDOW(samplingwindow));

    enable_widgets(FALSE);
    gtk_window_set_focus(GTK_WINDOW(samplingwindow), NULL);

    recordbufs = NULL;
    sampling = FALSE;
    has_data = FALSE;
    recordedlen = 0;
    current = NULL;
    rate = 44100;
    format = ST_MIXER_FORMAT_S16_LE;

    if (!sampling_driver->open(sampling_driver_object)) {
        static GtkWidget* dialog = NULL;

        sample_editor_stop_sampling();
        gui_error_dialog(&dialog, _("Sampling failed!"), FALSE);
    } else
        monitoring = TRUE;
}

static gboolean
monitorscope_queue(SampleDisplay* mscope)
{
    if (monitoring)
        sample_display_set_data(mscope, monitor_buf, format,
            monitor_count >> (mixer_get_resolution(format) + mixer_is_format_stereo(format) - 1), FALSE);

    return FALSE;
}

/* Count is in bytes, not samples. The function returns TRUE if the buffer is acquired and the driver shall allocate a new one */
gboolean
sample_editor_sampled(void* src,
    guint32 count,
    gint mixfreq,
    gint mixformat)
{
    gboolean sampled = FALSE;

    if (monitoring) {
        /* Actual data for sample monitor */
        format = mixformat;
        monitor_count = count;
        monitor_buf = src;
        g_idle_add((GSourceFunc)monitorscope_queue, (gpointer)monitorscope);

        if (sampling) {
            STSampleChain *newbuf = malloc(sizeof(STSampleChain));

            if (!newbuf) {
                /* It is called from audio thread AFAIK */
                error_error(_("Out of memory while sampling!"));
                sampling = 0;
                return FALSE;
            }
            newbuf->next = NULL;
            newbuf->length = count;
            newbuf->data = src;

            if (!recordbufs){ /* Sampling start */
                recordbufs = newbuf;
                rate = mixfreq;
            } else
                current->next = newbuf;

            current = newbuf;
            sampled = TRUE;
            recordedlen += count;
        }
    }

    return sampled;
}

void sample_editor_stop_sampling(void)
{

    sampling = FALSE;
    has_data = FALSE;

    if (samplingwindow) {
        if (monitoring) {
            monitoring = FALSE;
            sampling_driver->release(sampling_driver_object);
        }
        gtk_widget_hide(samplingwindow);

        sample_editor_clear_buffers(recordbufs);
        sample_display_set_data(monitorscope, NULL, format, 0, FALSE);
        recordbufs = NULL;
    }
}

void
sample_editor_chain_to_sample(STSampleChain *rbufs,
    gsize rlen,
    STMixerFormat fmt,
    guint32 srate,
    const gchar* samplename,
    const gchar* action,
    const gchar* log_name)
{
    STInstrument* instr;
    STSampleChain *r;
    gint16 *sbuf;
    guint multiply;
    gsize slen;
    const gboolean stereo = mixer_is_format_stereo(fmt);

    g_return_if_fail(sed->sample != NULL);

    fmt &= 0x7;
    g_return_if_fail(fmt == ST_MIXER_FORMAT_S16_LE || fmt == ST_MIXER_FORMAT_S16_BE ||
                     fmt == ST_MIXER_FORMAT_U16_LE || fmt == ST_MIXER_FORMAT_U16_BE ||
                     fmt == ST_MIXER_FORMAT_S8 || fmt == ST_MIXER_FORMAT_U8);

    multiply = mixer_get_resolution(fmt) - 1; /* 0 - 8 bit, 1 - 16, used for shifts */
    if (!multiply) /* 8bit */
        rlen <<= 1;

    if (!(sbuf = malloc(rlen))) {
        gui_oom_error();
        return;
    }

    if (!sample_editor_check_and_log_sample(sed->sample, log_name,
        HISTORY_FLAG_LOG_ALL, rlen >> 1))
        return;
    sample_editor_lock_sample();
    st_clean_sample(sed->sample, NULL, NULL);
    instr = instrument_editor_get_instrument();
    if (st_instrument_num_samples(instr) == 0)
        st_clean_instrument(instr, samplename);
    st_clean_sample_full(sed->sample, samplename, NULL, samplename != NULL);
    sed->sample->sample.data = sbuf;
    slen = stereo ? rlen >> 2 : rlen >> 1;

    for (r = rbufs; r; r = r->next) {
        guint j;

#ifdef WORDS_BIGENDIAN
        if (fmt == ST_MIXER_FORMAT_S16_LE || fmt == ST_MIXER_FORMAT_U16_LE)
#else
        if (fmt == ST_MIXER_FORMAT_S16_BE || fmt == ST_MIXER_FORMAT_U16_BE)
#endif
            byteswap_16_array(r->data, r->length);

        switch (fmt) {
        case ST_MIXER_FORMAT_S16_LE:
        case ST_MIXER_FORMAT_S16_BE:
            if (stereo) {
                st_convert_to_nonint(r->data, sbuf, slen, r->length >> 2);
                sbuf += r->length >> 2;
            } else {
                memcpy(sbuf, r->data, r->length);
                sbuf += r->length >> 1;
            }
            break;
        case ST_MIXER_FORMAT_U16_LE:
        case ST_MIXER_FORMAT_U16_BE:
            if (stereo)
                for (j = 0; j < r->length >> 2; j++, sbuf++) {
                    *sbuf = ((gint16*)r->data)[j << 1] - 32768;
                    sbuf[slen] = ((gint16*)r->data)[(j << 1) + 1] - 32768;
                }
            else
                for (j = 0; j < r->length >> 1; j++, sbuf++)
                    *sbuf = ((gint16*)r->data)[j] - 32768;
            break;
        case ST_MIXER_FORMAT_S8:
            if (stereo)
                for (j = 0; j < r->length >> 1; j++, sbuf++) {
                    *sbuf = ((gint8*)r->data)[j << 1] << 8;
                    sbuf[slen] = ((gint8*)r->data)[(j << 1) + 1] << 8;
                }
            else
                for (j = 0; j < r->length; j++, sbuf++)
                    *sbuf = ((gint8*)r->data)[j] << 8;
            break;
        case ST_MIXER_FORMAT_U8:
            if (stereo)
                for (j = 0; j < r->length >> 1; j++, sbuf++) {
                    *sbuf = (((gint8*)r->data)[j << 1] << 8) - 32768;
                    sbuf[slen] = (((gint8*)r->data)[(j << 1) + 1] << 8) - 32768;
                }
            else
                for (j = 0; j < r->length; j++, sbuf++)
                    *sbuf = (((gint8*)r->data)[j] << 8) - 32768;
            break;
        default: 
            /* Other formats are excluded by previous check and masking.
               This is just to avoid compiler warning. */
            break;
        }
    }

    if (rlen > mixer->max_sample_length) {
        static GtkWidget* dialog = NULL;
        gui_warning_dialog(&dialog, _("Recorded sample is too long for current mixer module. Using it anyway."), FALSE);
    }

    sed->sample->sample.length = slen; /* Sample size is given in 16-bit words */
    sed->sample->volume = 64;
    sed->sample->panning = 0;
    if (multiply)
        sed->sample->sample.flags |= ST_SAMPLE_16_BIT;
    else
        sed->sample->sample.flags &= ~ST_SAMPLE_16_BIT;
    if (stereo)
        sed->sample->sample.flags |= ST_SAMPLE_STEREO;
    else
        sed->sample->sample.flags &= ~ST_SAMPLE_STEREO;

    if (srate)
        xm_freq_note_to_relnote_finetune(srate,
            4 * 12 + 1, // at C-4
            &sed->sample->relnote,
            &sed->sample->finetune);

    sample_editor_unlock_sample();
    instrument_editor_update(TRUE);
    sample_editor_update();
}

static void
sample_editor_ok_clicked(void)
{
    if (has_data)
        sample_editor_chain_to_sample(recordbufs, recordedlen, format, rate,
            N_("<just sampled>"), N_("recorded"), N_("Sampling"));
}

/* ==================== VOLUME RAMPING DIALOG =================== */

static void
sample_editor_lrvol(GtkWidget* widget,
    gpointer data)
{
    int mode = GPOINTER_TO_INT(data);

    switch (mode) {
    case 0:
    case 2:
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[mode / 2]), 50);
        break;
    case 4:
    case 8:
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[mode / 8]), 200);
        break;
    }
}

static void
sample_editor_perform_ramp(const gint action,
    gint curve,
    void (*fn)(void))
{
    double left, right, k;
    gint ss, se;
    gint i, j, m, m2 = 16384, q;
    gint16* p;
    gboolean set_selection;
    const gchar* title;
    STSample* s;

    s = sed->sample;
    if (!s)
        return;
    set_selection = get_selection(sampledisplay, s, &ss, &se);

    switch (action) {
    case 1:
    case 3:
        // Find maximum amplitude
        p = s->sample.data;
        p += ss;
        for (i = 0, m = 0; i < se - ss; i++) {
            q = *p++;
            q = ABS(q);
            if (q > m)
                m = q;
        }
        if (s->sample.flags & ST_SAMPLE_STEREO){
            p = &s->sample.data[s->sample.length];
            p += ss;
            if (action == 1)
                for (i = 0; i < se - ss; i++) {
                    q = *p++;
                    q = ABS(q);
                    if (q > m)
                        m = q;
                }
            else
                for (i = 0, m2 = 0; i < se - ss; i++) {
                    q = *p++;
                    q = ABS(q);
                    if (q > m2)
                        m2 = q;
                }
        }
        left = right = (double)0x7fff / m;
        title = N_("Sample normalizing");
        break;
    case 2:
        left = gtk_spin_button_get_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[0])) / 100.0;
        right = gtk_spin_button_get_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[1])) / 100.0;
        title = N_("Sample volume ramping");
        break;
    default:
        return;
    }

    if (!sample_editor_check_and_log_range(s, title, HISTORY_FLAG_LOG_ALL, ss, se))
        return;
    sample_editor_log_action(fn);

    if (fabs(left - right) < 0.005) /* Equal => linear */
        curve = 0;
    switch (curve) {
    case 1:
    case 2:
        k = log(right / left) / (se - ss);
        break;
    default:
        k = 0.0;
        break;
    }

    // Now perform the actual operation
    sample_editor_lock_sample();

    p = s->sample.data;
    p += ss;
    for (j = 0; j < (s->sample.flags & ST_SAMPLE_STEREO ? 2 : 1);
        j++, p = &s->sample.data[s->sample.length], p += ss) {
        for (i = 0; i < se - ss; i++) {
            double q = *p, mul;

            switch (curve) {
            case 1:
                mul = left * exp(k * i);
                break;
            case 2:
                mul = right + left * (1.0 - exp(k * (se - ss - i)));
                break;
            default:
                mul = left + i * (right - left) / (se - ss);
                break;
            }

            q *= mul;
            *p++ = CLAMP((int)q, -32768, +32767);
        }
        if (action == 3) /* This happens only in case of stereo */
            left = right = (double)0x7fff / m2;
    }

    sample_editor_unlock_sample();
    sample_editor_update();
    if (set_selection)
        sample_display_set_selection(sampledisplay, ss, se);
}

void
sample_editor_normalize(void)
{
    sample_editor_perform_ramp(1, 0, sample_editor_normalize);
}

void
sample_editor_normalize_channels(void)
{
    sample_editor_perform_ramp(3, 0, sample_editor_normalize_channels);
}

static void
volfade_curve_changed(GtkComboBox* combo)
{
    gint active = gtk_combo_box_get_active(combo);
    gdouble lowlim;

    switch (active) {
    case 0:
        lowlim = -1000.0;
        break;
    case 1:
    case 2:
        lowlim = 1.0;
        break;
    default:
        return;
    }
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[0]), lowlim, 1000.0);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[1]), lowlim, 1000.0);
}

void
sample_editor_open_volume_ramp_dialog(void)
{
    static GtkWidget *volrampwindow = NULL, *combo;
    gint response;

    if (volrampwindow == NULL) {
        GtkWidget *mainbox, *box1, *thing;
        GtkListStore* ls;
        GtkTreeIter iter;
        gint i;
        const gchar* const curves[] = { N_("Linear"), N_("Exponent"),
            N_("Reverse Exponent") };

        volrampwindow = gtk_dialog_new_with_buttons(_("Volume Ramping"), GTK_WINDOW(mainwindow), 0,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, 2, NULL);

        gui_dialog_adjust(volrampwindow, 2);
        g_object_set(volrampwindow, "has-separator", TRUE, NULL);
        mainbox = gtk_dialog_get_content_area(GTK_DIALOG(volrampwindow));
        gtk_box_set_spacing(GTK_BOX(mainbox), 2);

        thing = gtk_label_new(_("Perform volume fade on Selection"));
        gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, TRUE, 0);

        box1 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box1, FALSE, TRUE, 4);

        gui_put_labelled_spin_button(box1, _("Left [%]:"), -1000, 1000,
            &sample_editor_volramp_spin_w[0], NULL, NULL, FALSE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[0]), 100);

        thing = gtk_button_new_with_label(_("50%"));
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
        g_signal_connect(thing, "clicked",
            G_CALLBACK(sample_editor_lrvol), GINT_TO_POINTER(0));

        thing = gtk_button_new_with_label(_("200%"));
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
        g_signal_connect(thing, "clicked",
            G_CALLBACK(sample_editor_lrvol), GINT_TO_POINTER(4));

        box1 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box1, FALSE, TRUE, 4);

        gui_put_labelled_spin_button(box1, _("Right [%]:"), -1000, 1000,
            &sample_editor_volramp_spin_w[1], NULL, NULL, FALSE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(sample_editor_volramp_spin_w[1]), 100);

        thing = gtk_button_new_with_label(_("H"));
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
        gtk_widget_set_tooltip_text(thing, _("Half amp"));
        g_signal_connect(thing, "clicked",
            G_CALLBACK(sample_editor_lrvol), GINT_TO_POINTER(2));

        thing = gtk_button_new_with_label(_("D"));
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);
        gtk_widget_set_tooltip_text(thing, _("Double amp"));
        g_signal_connect(thing, "clicked",
            G_CALLBACK(sample_editor_lrvol), GINT_TO_POINTER(8));

        box1 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box1, FALSE, TRUE, 0);

        thing = gtk_label_new(_("Curve:"));
        gtk_box_pack_start(GTK_BOX(box1), thing, FALSE, TRUE, 0);

        ls = gtk_list_store_new(1, G_TYPE_STRING);
        for (i = 0; i < ARRAY_SIZE(curves); i++) {
            gtk_list_store_append(ls, &iter);
            gtk_list_store_set(ls, &iter, 0, _(curves[i]), -1);
        }

        combo = gui_combo_new(ls);
        gtk_box_pack_end(GTK_BOX(box1), combo, FALSE, TRUE, 0);
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
        g_signal_connect(combo, "changed",
            G_CALLBACK(volfade_curve_changed), NULL);

        gtk_widget_show_all(volrampwindow);
    }

    response = gtk_dialog_run(GTK_DIALOG(volrampwindow));
    gtk_widget_hide(volrampwindow);
    sample_editor_perform_ramp(response, gtk_combo_box_get_active(GTK_COMBO_BOX(combo)),
        sample_editor_open_volume_ramp_dialog);
}

static void
sample_editor_perform_agc(const gint response,
    const gdouble attack,
    const gdouble decay,
    const gboolean keep_cons,
    const gboolean indep,
    void (*action)(void))
{
    gint ss, se;
    gint i;
    gboolean set_selection;
    gdouble data, data0 = 0.0, data0_r = 0.0, feedback = 0.0, t;
    gdouble *tmp_data, *tmp_data_r = NULL, maxamp = 0.0, maxamp_r = 0.0;
    gint16* sdata;
    st_mixer_sample_info* s;

    if (response != GTK_RESPONSE_APPLY)
        return;

    if (!sed->sample)
        return;
    set_selection = get_selection(sampledisplay, sed->sample, &ss, &se);
    s = &sed->sample->sample;
    sdata = s->data;

    if (!sample_editor_check_and_log_range(sed->sample,
        N_("Performing AGC"), HISTORY_FLAG_LOG_ALL, ss, se))
        return;
    sample_editor_log_action(action);

    tmp_data = g_new(gdouble, se - ss);
    if (s->flags & ST_SAMPLE_STEREO) {
        gdouble t_r, feedback_r = 0.0, data_r;

        tmp_data_r = g_new(gdouble, se - ss);
        if (indep) /* Process channels independently */
            for (i = ss; i < se; i++) {
                t = (gdouble)sdata[i];
                t_r = (gdouble)sdata[i + s->length];

                data = fabs(t);
                data_r = fabs(t_r);
                if (i == ss) {
                    feedback = data;
                    data0 = data;
                    feedback_r = data_r;
                    data0_r = data_r;
                } else {
                    if (data > feedback) /* Peak detector */
                        feedback += (data - feedback) / attack;
                    if (data_r > feedback_r)
                        feedback_r += (data_r - feedback_r) / attack;
                }
                feedback -= feedback / decay; /* 1st order IIR LP filter*/
                tmp_data[i - ss] = t / feedback;
                feedback_r -= feedback_r / decay; /* 1st order IIR LP filter*/
                tmp_data_r[i - ss] = t_r / feedback_r;
                if (!keep_cons) {
                    t = fabs(tmp_data[i - ss]);
                    if (t > maxamp)
                        maxamp = t;
                    t_r = fabs(tmp_data_r[i - ss]);
                    if (t_r > maxamp_r)
                        maxamp_r = t_r;
                }
            }
        else {/* Process channels together */
            for (i = ss; i < se; i++) {
                t = (gdouble)sdata[i];
                t_r = (gdouble)sdata[i + s->length];

                data = MAX(fabs(t), fabs(t_r));
                if (i == ss) {
                    feedback = data;
                    data0 = data;
                } else if (data > feedback) /* Peak detector */
                    feedback += (data - feedback) / attack;
                feedback -= feedback / decay; /* 1st order IIR LP filter*/
                tmp_data[i - ss] = t / feedback;
                tmp_data_r[i - ss] = t_r / feedback;
                if (!keep_cons) {
                    t = fabs(tmp_data[i - ss]);
                    if (t > maxamp)
                        maxamp = t;
                    t_r = fabs(tmp_data_r[i - ss]);
                    if (t_r > maxamp)
                        maxamp = t_r;
                }
            }
            data0_r = data0;
            maxamp_r = maxamp;
        }
    } else
        for (i = ss; i < se; i++) {
            t = (gdouble)sdata[i];

            data = fabs(t);
            if (i == ss) {
                feedback = data;
                data0 = data;
            } else if (data > feedback) /* Peak detector */
                feedback += (data - feedback) / attack;
            feedback -= feedback / decay; /* 1st order IIR LP filter*/
            tmp_data[i - ss] = t / feedback;
            if (!keep_cons) {
                t = fabs(tmp_data[i - ss]);
                if (t > maxamp)
                    maxamp = t;
            }
        }

    /* Now perform the actual operation */
    sample_editor_lock_sample();

    for (i = ss; i < se; i++) {
        gdouble tmp = tmp_data[i - ss] * ((keep_cons) ? data0 : (32767.0 / maxamp));

        sdata[i] = CLAMP(lrint(tmp), -32768, +32767);
    }
    g_free(tmp_data);
    if (s->flags & ST_SAMPLE_STEREO) {
        for (i = ss; i < se; i++) {
            gdouble tmp = tmp_data_r[i - ss] * ((keep_cons) ? data0_r : (32767.0 / maxamp_r));

            sdata[i + s->length] = CLAMP(lrint(tmp), -32768, +32767);
        }
        g_free(tmp_data_r);
    }

    sample_editor_unlock_sample();
    sample_editor_update();
    if (set_selection)
        sample_display_set_selection(sampledisplay, ss, se);
}

void
sample_editor_open_agc_dialog(void)
{
    static const gchar *agc_modes[] = {N_("Keep consistency"), N_("Normalize")};
    static GtkWidget *window = NULL, *spin_attack, *spin_decay, *check_ind;
    static GtkWidget* sample_editor_agc_mode_w[2];
    gint response;

    if (window == NULL) {
        GtkWidget *mainbox, *box1;

        window = gtk_dialog_new_with_buttons(_("Automatic Gain Control"), GTK_WINDOW(mainwindow), 0,
            GTK_STOCK_EXECUTE, GTK_RESPONSE_APPLY,
            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);

        gui_dialog_adjust(window, 2);
        g_object_set(window, "has-separator", TRUE, NULL);
        mainbox = gtk_dialog_get_content_area(GTK_DIALOG(window));
        gtk_box_set_spacing(GTK_BOX(mainbox), 2);

        box1 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box1, FALSE, TRUE, 0);
        gui_put_labelled_spin_button(box1, _("Attack:"), 1, 100, &spin_attack, NULL, NULL, FALSE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_attack), 10);
        gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin_attack), 1);
        add_empty_hbox(box1);

        gui_put_labelled_spin_button(box1, _("Decay:"), 10, 10000, &spin_decay, NULL, NULL, FALSE);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_decay), 1000);
        add_empty_hbox(box1);

        box1 = gtk_hbox_new(FALSE, 4);
        gtk_box_pack_start(GTK_BOX(mainbox), box1, FALSE, TRUE, 0);
        make_radio_group(agc_modes, box1, sample_editor_agc_mode_w, FALSE, TRUE, NULL);

        check_ind = gtk_check_button_new_with_label(_("Process channels independently"));
        gtk_box_pack_start(GTK_BOX(mainbox), check_ind, FALSE, TRUE, 0);

        gtk_widget_show_all(window);
    }
    (sed->sample->sample.flags & ST_SAMPLE_STEREO ? gtk_widget_show : gtk_widget_hide)(check_ind);
    gtk_window_reshow_with_initial_size(GTK_WINDOW(window));

    response = gtk_dialog_run(GTK_DIALOG(window));
    gtk_widget_hide(window);
    sample_editor_perform_agc(response,
        gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin_attack)),
        gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin_decay)),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sample_editor_agc_mode_w[0])),
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_ind)),
        sample_editor_open_agc_dialog);
}

/* ====================== SILENCE INSERTION ===================== */
void
sample_editor_insert_silence(void)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *position[2], *discretes;

    gint response, start, length, orig_length, newlen, tail;
    gboolean reselection, at_beg;
    gint ss, se;
    gint ns = sampledisplay->win_start,
         ne = sampledisplay->win_start + sampledisplay->win_length;
    st_mixer_sample_info* smp;

    const gchar* labels[] = {
        N_("Before selection or at the beginnning"),
        N_("After selection or at the end")
    };

    if (!sed->sample)
        return;
    smp = &sed->sample->sample;
    if (!smp->length)
        return;

    reselection = get_selection(sampledisplay, sed->sample, &ss, &se);

    if (!dialog) {
        GtkWidget *mainbox, *thing, *box;

        dialog = gtk_dialog_new_with_buttons(_("Insert silence"), GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(dialog, GTK_RESPONSE_OK);
        g_object_set(dialog, "has-separator", TRUE, NULL);
        mainbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        gtk_box_set_spacing(GTK_BOX(mainbox), 2);

        make_radio_group(labels, mainbox, position, FALSE, FALSE, NULL);

        box = gtk_hbox_new(FALSE, 0);

        thing = gtk_label_new(_("Number of discretes"));
        gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);
        discretes = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(1000.0, 1.0, INT_MAX, 1.0, 5.0, 0.0)), 0, 0);
        gtk_box_pack_end(GTK_BOX(box), discretes, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(mainbox), box, FALSE, FALSE, 0);

        gtk_widget_show_all(dialog);
    }

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(dialog);

    if (response != GTK_RESPONSE_OK)
        return;

    length = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(discretes));
    newlen = smp->length + length;
    if (!sample_editor_check_and_log_sample(sed->sample,
        N_("Silence insertion"), HISTORY_FLAG_LOG_ALL,
        smp->flags & ST_SAMPLE_STEREO ? newlen << 1 : newlen))
        return;
    sample_editor_log_action(sample_editor_insert_silence);

    sample_editor_lock_sample();
    at_beg = (find_current_toggle(position, 2) == 0);
    start =  at_beg ? ss : se;
    tail = smp->length - start;
    orig_length = smp->length;
    smp->length = newlen;
    smp->data = g_renew(gint16, smp->data, smp->flags & ST_SAMPLE_STEREO ? smp->length << 1 : smp->length);
    if (smp->flags & ST_SAMPLE_STEREO) {
        memmove(&smp->data[start + orig_length + (length << 1)], &smp->data[start + orig_length],
            tail * sizeof(smp->data[0]));
        memmove(&smp->data[start + length], &smp->data[start], orig_length * sizeof(smp->data[0]));
        memset(&smp->data[start + smp->length], 0, length * sizeof(smp->data[0]));
    } else
        memmove(&smp->data[start + length], &smp->data[start], tail * sizeof(smp->data[0]));
    memset(&smp->data[start], 0, length * sizeof(smp->data[0]));

    if (smp->loopend > start)
        smp->loopend += length;
    if (smp->loopstart >= start)
        smp->loopstart += length;

    sample_editor_unlock_sample();
    sample_editor_update();
    sample_display_set_window(sampledisplay, ns, ne);

    if (reselection) {
        if (at_beg) {
            ss += length;
            se += length;
        }
        sample_display_set_selection(sampledisplay, ss, se);
    }
}

/* =================== TRIM AND CROP FUNCTIONS ================== */
void
sample_editor_trim_dialog(void)
{
    static GtkAdjustment* adj;
    static GtkWidget *trimdialog = NULL, *trimbeg, *trimend;
    GtkWidget *mainbox, *thing, *box;
    gint response;

    sample_editor_log_action(sample_editor_trim_dialog);
    if (!trimdialog) {
        trimdialog = gtk_dialog_new_with_buttons(_("Trim parameters"), GTK_WINDOW(mainwindow), GTK_DIALOG_MODAL,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
        gui_dialog_adjust(trimdialog, GTK_RESPONSE_OK);
        g_object_set(trimdialog, "has-separator", TRUE, NULL);
        mainbox = gtk_dialog_get_content_area(GTK_DIALOG(trimdialog));
        gtk_box_set_spacing(GTK_BOX(mainbox), 2);

        trimbeg = thing = gtk_check_button_new_with_label(_("Trim at the beginning"));
        gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), TRUE);

        trimend = thing = gtk_check_button_new_with_label(_("Trim at the end"));
        gtk_box_pack_start(GTK_BOX(mainbox), thing, FALSE, FALSE, 0);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), TRUE);

        box = gtk_hbox_new(FALSE, 0);

        thing = gtk_label_new(_("Threshold (dB)"));
        gtk_box_pack_start(GTK_BOX(box), thing, FALSE, FALSE, 0);
        thing = gtk_spin_button_new(adj = GTK_ADJUSTMENT(gtk_adjustment_new(-50.0, -80.0, -20.0, 1.0, 5.0, 0.0)), 0, 0);
        gtk_box_pack_end(GTK_BOX(box), thing, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(mainbox), box, FALSE, FALSE, 0);

        gtk_widget_show_all(trimdialog);
    } else
        gtk_window_present(GTK_WINDOW(trimdialog));

    response = gtk_dialog_run(GTK_DIALOG(trimdialog));
    gtk_widget_hide(trimdialog);

    if (response == GTK_RESPONSE_OK)
        sample_editor_trim(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(trimbeg)),
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(trimend)), adj->value);
}

static void
sample_editor_trim(gboolean trbeg, gboolean trend, gfloat thrshld)
{
    int start = sampledisplay->sel_start, end = sampledisplay->sel_end;
    int i, c, ofs;
    int amp = 0, val, bval = 0, maxamp, ground;
    int on, off;
    double avg;
    int reselect = 1;
    gint16* data;

    if (sed->sample == NULL)
        return;
    if (!trbeg && !trend)
        return;

    /* if there's no selection, we operate on the entire sample */
    if (start == -1) {
        start = 0;
        end = sed->sample->sample.length;
        reselect = 0;
    }

    data = sed->sample->sample.data;
    /* Finding the maximum amplitude */
    for (i = 0, maxamp = 0; i < end - start; i++) {
        val = *(data + i);
        val = ABS(val);
        if (val > maxamp)
            maxamp = val;
    }

    if (maxamp == 0)
        return;

    ground = rint((gfloat)maxamp * pow(10.0, thrshld / 20));

    /* Computing the beginning average level until we reach the ground level */
    for (c = 0, ofs = start, amp = 0, avg = 0; ofs < end && amp < ground; ofs++) {
        val = *(data + ofs);
        if (ofs == start) {
            bval = -val;
            amp = ABS(val);
        }
        if ((val < 0 && bval >= 0) || (val >= 0 && bval < 0)) {
            avg += amp;
            c++;
            amp = 0;
        } else {
            if (ABS(val) > amp)
                amp = ABS(val);
        }
        bval = val;
    }
    avg = avg / c;

    /* Locating when sounds turns on.
       That is : when we *last* got higher than the average level. */
    for (amp = maxamp; ofs > start && amp > avg; --ofs) {
        amp = 0;
        for (val = 1; ofs > start && val > 0; --ofs) {
            val = *(data + ofs);
            if (val > amp)
                amp = val;
        }
        for (; ofs > start && val <= 0; --ofs) {
            val = *(data + ofs);
            if (-val > amp)
                amp = -val;
        }
    }
    on = ofs;

    /* Computing the ending average level until we reach the ground level */
    for (ofs = end - 1, avg = 0, amp = 0, c = 0; ofs > on && amp < ground; ofs--) {
        val = *(data + ofs);
        if (ofs == end - 1) {
            bval = -val;
            amp = ABS(val);
        }
        if ((val < 0 && bval >= 0) || (val >= 0 && bval < 0)) {
            avg += amp;
            c++;
            amp = 0;
        } else {
            if (ABS(val) > amp)
                amp = ABS(val);
        }
        bval = val;
    }
    avg = avg / c;

    /* Locating when sounds turns off.
       That is : when we *last* got higher than the average level. */
    for (amp = maxamp; ofs < end && amp > avg; ++ofs) {
        amp = 0;
        for (val = 1; ofs < end && val > 0; ++ofs) {
            val = *(data + ofs);
            if (val > amp)
                amp = val;
        }
        for (; ofs < end && val <= 0; ++ofs) {
            val = *(data + ofs);
            if (-val > amp)
                amp = -val;
        }
    }
    off = ofs;

    // Wiping blanks out
    if (on < start)
        on = start;
    if (off > end)
        off = end;
    if (!sample_editor_check_and_log_sample(sed->sample,
        N_("Sample trimming"), HISTORY_FLAG_LOG_ALL, 0))
        return;

    sample_editor_lock_sample();
    if (trbeg) {
        sample_editor_delete(sed->sample, start, on);
        off -= on - start;
        end -= on - start;
    }
    if (trend)
        sample_editor_delete(sed->sample, off, end);
    st_sample_fix_loop(sed->sample);
    sample_editor_unlock_sample();

    sample_editor_update();

    if (reselect == 1 && off > on)
        sample_display_set_selection(sampledisplay, start, start + off - on);
}

/* deletes the portion of *sample data from start to end-1 */
static void sample_editor_delete(STSample* sample, int start, int end)
{
    int newlen;
    gint16* newdata;

    if (sample == NULL || start == -1 || start >= end)
        return;

    newlen = sample->sample.length - end + start;

    newdata = calloc(newlen, sample->sample.flags & ST_SAMPLE_STEREO ?
        sizeof(sample->sample.data[0]) <<  1 : sizeof(sample->sample.data[0]));
    if (!newdata)
        return;

    memcpy(newdata, sample->sample.data, start * sizeof(sample->sample.data[0]));
    memcpy(newdata + start, sample->sample.data + end,
        (sample->sample.length - end) * sizeof(sample->sample.data[0]));
    if (sample->sample.flags & ST_SAMPLE_STEREO) {
        memcpy(&newdata[newlen], &sample->sample.data[sample->sample.length],
            start * sizeof(sample->sample.data[0]));
        memcpy(&newdata[newlen + start], &sample->sample.data[sample->sample.length + end],
            (sample->sample.length - end) * sizeof(sample->sample.data[0]));
    }

    free(sample->sample.data);

    sample->sample.data = newdata;
    sample->sample.length = newlen;

    /* Move loop start and end along with splice */
    if (sample->sample.loopstart > start && sample->sample.loopend < end) {
        /* loop was wholly within selection -- remove it */
        sample->sample.flags &= ~ST_SAMPLE_LOOP_MASK;
        sample->sample.loopstart = 0;
        sample->sample.loopend = 1;
    } else {
        if (sample->sample.loopstart > end) {
            /* loopstart was after selection */
            sample->sample.loopstart -= (end - start);
        } else if (sample->sample.loopstart > start) {
            /* loopstart was within selection */
            sample->sample.loopstart = start;
        }

        if (sample->sample.loopend > end) {
            /* loopend was after selection */
            sample->sample.loopend -= (end - start);
        } else if (sample->sample.loopend > start) {
            /* loopend was within selection */
            sample->sample.loopend = start;
        }
    }

    st_sample_fix_loop(sample);
}

GtkAdjustment*
sample_editor_get_adjustment(SampleEditorSpin n)
{
    GtkWidget* spn = se_spins[n];

    g_assert(GTK_IS_SPIN_BUTTON(spn));
    return gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spn));
}

void
sample_editor_repeat_last(void)
{
    g_assert(last_handler != NULL);
    last_handler();
}

void
sample_editor_log_action(void (*action)(void))
{
    if (!last_handler) {
        gtk_widget_set_sensitive(sample_editor_get_widget("repeat_last_menuitem"), TRUE);
        gtk_widget_set_sensitive(sample_editor_get_widget("repeat_last_tbutton"), TRUE);
    }
    last_handler = action;
}
