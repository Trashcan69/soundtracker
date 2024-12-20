
/*
 * The Real SoundTracker - GTK+ sample display widget
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

#include <stdlib.h>
#include <string.h>

#include "colors.h"
#include "marshal.h"
#include "sample-display.h"

#define XPOS_TO_OFFSET(x) (s->win_start + ((guint64)(x)) * s->win_length / s->width)
#define OFFSET_RANGE(m, l, x) (x < m ? m : (x >= l ? l - 1 : x))

enum {
    SELECTING_NOTHING = 0,
    SELECTING_SELECTION_START,
    SELECTING_SELECTION_END,
    SELECTING_LOOP_START,
    SELECTING_LOOP_END,
    SELECTING_PAN_WINDOW,
};

enum {
    SIG_SELECTION_CHANGED,
    SIG_LOOP_CHANGED,
    SIG_WINDOW_CHANGED,
    SIG_POS_CHANGED,
    LAST_SIGNAL
};

struct _SampleDisplayPriv {
    DISegment *segments;
    guint seg_allocated;
    gboolean edit; /* enable loop / selection editing */
    SampleDisplayMode mode;

    DIGC bg_gc, fg_gc, loop_gc;
    guint idle_handler;

    void* data;
    gint datalen;
    gint datatype;
    gboolean datacopy; /* do we have our own copy */
    guint datacopylen;
    STSampleChain* chain;

    gboolean display_zero_line;
    DIGC zeroline_gc;

    /* selection handling */
    gint old_ss, old_se;
    gint button; /* button index which started the selection */
    gint selecting;
    gint selecting_x0; /* the coordinate where the mouse was clicked */
    gint selecting_wins0;

    /* loop points */
    gint loop_start, loop_end; /* offsets into the sample data or -1 */
};

#define IS_INITIALIZED(s) (s->priv->datalen != 0)

static guint sample_display_signals[LAST_SIGNAL] = { 0 };

static gint sample_display_idle_draw_function(SampleDisplay* s);

G_DEFINE_TYPE(SampleDisplay, sample_display, custom_drawing_get_type());

gint
sample_display_xpos_to_offset(SampleDisplay* s, const gint x)
{
    return XPOS_TO_OFFSET(x);
}

static void
sample_display_idle_draw(SampleDisplay* s)
{
    if (!s->priv->idle_handler) {
        s->priv->idle_handler = g_idle_add((GSourceFunc)sample_display_idle_draw_function,
            (gpointer)s);
        g_assert(s->priv->idle_handler != 0);
    }
}

void sample_display_enable_zero_line(SampleDisplay* s,
    gboolean enable)
{
    s->priv->display_zero_line = enable;

    if (s->priv->datalen) {
        gtk_widget_queue_draw(GTK_WIDGET(s));
    }
}

void sample_display_set_mode(SampleDisplay* s,
    const SampleDisplayMode mode)
{
    s->priv->mode = mode;
}

void sample_display_set_edit(SampleDisplay* s,
    const gboolean edit)
{
    s->priv->edit = edit;
}

/* Len is in samples, not bytes! */
void sample_display_set_data(SampleDisplay* s,
    void* data,
    STMixerFormat type,
    int len,
    gboolean copy)
{
    SampleDisplayPriv* p;

    g_return_if_fail(s != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(s));
    p = s->priv;

    if (!data || !len) {
        p->datalen = 0;
    } else {
        if (copy) {
            guint byteslen = len << (mixer_get_resolution(type) + mixer_is_format_stereo(type) - 1);

            if (p->datacopy) {
                if (p->datacopylen < byteslen) {
                    g_free(p->data);
                    p->data = g_new(gint8, byteslen);
                    p->datacopylen = byteslen;
                }
            } else {
                p->data = g_new(gint8, byteslen);
                p->datacopylen = byteslen;
            }
            g_assert(p->data != NULL);
            memcpy(p->data, data, byteslen);
        } else {
            if (p->datacopy) {
                g_free(p->data);
            }
            p->data = data;
        }
        p->datalen = len;
        p->datacopy = copy;
        p->datatype = type;
    }

    s->win_start = 0;
    s->win_length = len;
    if (p->edit)
        g_signal_emit(G_OBJECT(s), sample_display_signals[SIG_WINDOW_CHANGED], 0, s->win_start, s->win_start + s->win_length);

    s->sel_start = -1;
    p->old_ss = p->old_se = -1;
    p->selecting = 0;

    p->loop_start = -1;
    p->chain = NULL;

    gtk_widget_queue_draw(GTK_WIDGET(s));
}

void sample_display_set_chain(SampleDisplay* s, STSampleChain* chain, STMixerFormat type)
{
    SampleDisplayPriv* p;
    STSampleChain* cur;

    g_return_if_fail(s != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(s));
    p = s->priv;
    g_return_if_fail(!p->edit); /* Chain editing isn't supported */

    if (p->datacopy)
        g_free(p->data);

    for (cur = chain, p->datalen = 0; cur; cur = cur->next)
        p->datalen += cur->length;

    p->datalen >>= mixer_get_resolution(type) + mixer_is_format_stereo(type) - 1;
    p->chain = p->datalen ? chain : NULL;
    p->datatype = type;
    p->datacopy = FALSE;

    s->win_start = 0;
    s->win_length = p->datalen;

    s->sel_start = -1;
    p->old_ss = p->old_se = -1;
    p->selecting = 0;

    p->loop_start = -1;

    gtk_widget_queue_draw(GTK_WIDGET(s));
}

void sample_display_set_loop(SampleDisplay* s,
    int start,
    int end)
{
    SampleDisplayPriv* p;

    g_return_if_fail(s != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(s));
    p = s->priv;

    if (!p->edit || !IS_INITIALIZED(s))
        return;

    if (p->loop_start == start && p->loop_end == end)
        return;

    g_return_if_fail(start >= -1 && start < p->datalen);
    g_return_if_fail(end > 0 && end <= p->datalen);
    g_return_if_fail(end > start);

    p->loop_start = start;
    p->loop_end = end;

    gtk_widget_queue_draw(GTK_WIDGET(s));
}

void sample_display_set_selection(SampleDisplay* s,
    int start,
    int end)
{
    SampleDisplayPriv* p;

    g_return_if_fail(s != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(s));
    p = s->priv;

    if (!p->edit || !IS_INITIALIZED(s))
        return;

    g_return_if_fail(start >= -1 && start < p->datalen);
    g_return_if_fail(end >= 1 && end <= p->datalen);
    g_return_if_fail(end > start);

    s->sel_start = start;
    s->sel_end = end;

    sample_display_idle_draw(s);
    g_signal_emit(G_OBJECT(s), sample_display_signals[SIG_SELECTION_CHANGED], 0, start, end);
}

static void
sample_display_set_window_full(SampleDisplay *s,
   int start,
   int end,
   gboolean sig_pos_changed)
{
    SampleDisplayPriv* p;

    g_return_if_fail(s != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(s));
    p = s->priv;
    g_return_if_fail(start >= 0 && start < p->datalen);
    g_return_if_fail(end < 0 || (end > 0 && end <= p->datalen));
    g_return_if_fail(end < 0 || end > start);

    s->win_start = start;
    if (end > 0) {
        s->win_length = end - start;
        if (p->edit)
            g_signal_emit(G_OBJECT(s), sample_display_signals[SIG_WINDOW_CHANGED], 0, start, end);
    } else if (sig_pos_changed)
        g_signal_emit(G_OBJECT(s), sample_display_signals[SIG_POS_CHANGED], 0, start);

    gtk_widget_queue_draw(GTK_WIDGET(s));
}

void
sample_display_set_window(SampleDisplay *s,
   int start,
   int end)
{
    sample_display_set_window_full(s, start, end, FALSE);
}

static void
sample_display_init_display(SampleDisplay* s,
    int w,
    int h)
{
    s->width = w;
    s->height = h;
}

static void
sample_display_size_allocate(GtkWidget* widget,
    GtkAllocation* allocation)
{
    SampleDisplay* s;
    SampleDisplayPriv* p;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation = *allocation;
    s = SAMPLE_DISPLAY(widget);
    p = s->priv;

    if (allocation->width << 1 > p->seg_allocated) {
        /* Twice the required amount for potential stereo samples */
        p->seg_allocated = (allocation->width << 1) + allocation->width;

        if (!p->segments)
            p->segments = g_new(DISegment, p->seg_allocated);
        else
            p->segments = g_renew(DISegment, p->segments, p->seg_allocated);
    }

    if (gtk_widget_get_realized(widget)) {
        gdk_window_move_resize(widget->window,
            allocation->x, allocation->y,
            allocation->width, allocation->height);

        sample_display_init_display(s, allocation->width, allocation->height);
    }
    (*GTK_WIDGET_CLASS(sample_display_parent_class)->size_allocate)(widget, allocation);
}

static void
sample_display_realize(GtkWidget* widget)
{
    GdkWindowAttr attributes;
    gint attributes_mask;
    SampleDisplay* s;
    SampleDisplayPriv* p;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(IS_SAMPLE_DISPLAY(widget));

    gtk_widget_set_realized(widget, TRUE);
    s = SAMPLE_DISPLAY(widget);
    p = s->priv;

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events(widget)
        | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
        | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK;

    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(widget->parent->window, &attributes, attributes_mask);

    widget->style = gtk_style_attach(widget->style, widget->window);

    if (!p->bg_gc) {
        p->bg_gc = colors_get_gc(COLOR_BG);
        colors_add_widget(COLOR_BG, widget);
        p->fg_gc = colors_get_gc(COLOR_NOTES);
        p->zeroline_gc = colors_get_gc(COLOR_ZEROLINE);
        if (p->edit)
            p->loop_gc = colors_get_gc(COLOR_CHANNUMS);
    }

    sample_display_init_display(s, attributes.width, attributes.height);

    gdk_window_set_user_data(widget->window, widget);
    (*GTK_WIDGET_CLASS(sample_display_parent_class)->realize)(widget);
}

static void
sample_display_size_request(GtkWidget* widget,
    GtkRequisition* requisition)
{
    requisition->width = 10;
    requisition->height = 32;
}

static inline gint32
normalize(void* data,
    guint offset,
    guint min,
    guint len,
    const guint sh,
    const gint format)
{
    gint32 c;

    if (format & ST_MIXER_FORMAT_STEREO) {
        offset <<= 1;
        len <<= 1;
    }

    /* Since format is a constant, this should be expanded to a single case
       by compiler optimization */
    switch (format) {
    case ST_MIXER_FORMAT_S16:
        c = ((gint16*)data)[OFFSET_RANGE(min, len, offset)];
        c = ((32767 - c) * sh) >> 16;
        break;
    case ST_MIXER_FORMAT_U16:
        c = ((guint16*)data)[OFFSET_RANGE(min, len, offset)];
        c = ((65535 - c) * sh) >> 16;
        break;
    case ST_MIXER_FORMAT_S8:
        c = ((gint8*)data)[OFFSET_RANGE(min, len, offset)];
        c = ((127 - c) * sh) >> 8;
        break;
    case ST_MIXER_FORMAT_U8:
        c = ((guint8*)data)[OFFSET_RANGE(min, len, offset)];
        c = ((255 - c) * sh) >> 8;
        break;
    /* Average left and right channels if stereo */
    case ST_MIXER_FORMAT_S16 | ST_MIXER_FORMAT_STEREO:
        c = (((gint16*)data)[OFFSET_RANGE(min, len, offset)]
            + ((gint16*)data)[OFFSET_RANGE(min, len, offset + 1)]) >> 1;
        c = ((32767 - c) * sh) >> 16;
        break;
    case ST_MIXER_FORMAT_U16 | ST_MIXER_FORMAT_STEREO:
        c = (((guint16*)data)[OFFSET_RANGE(min, len, offset)]
            + ((guint16*)data)[OFFSET_RANGE(min, len, offset + 1)]) >> 1;
        c = ((65535 - c) * sh) >> 16;
        break;
    case ST_MIXER_FORMAT_S8 | ST_MIXER_FORMAT_STEREO:
        c = (((gint8*)data)[OFFSET_RANGE(min, len, offset)]
            + ((gint8*)data)[OFFSET_RANGE(min, len, offset + 1)]) >> 1;
        c = ((127 - c) * sh) >> 8;
        break;
    case ST_MIXER_FORMAT_U8 | ST_MIXER_FORMAT_STEREO:
        c = (((guint8*)data)[OFFSET_RANGE(min, len, offset)]
            + ((guint8*)data)[OFFSET_RANGE(min, len, offset + 1)]) >> 1;
        c = ((255 - c) * sh) >> 8;
        break;
    default:
        c = 0;
    }

    return c;
}

STSampleChain*
offset_lookup(SampleDisplayPriv* p,
    STSampleChain* ch,
    guint offset,
    guint* len,
    guint* buffer_offset)
{
    gint format = p->datatype;
    guint lgth, new_bo = 0;

    g_assert(ch != NULL);
    while (offset - new_bo >= (lgth =
        ch->length >> (mixer_get_resolution(format) + mixer_is_format_stereo(format) - 1))) {
        new_bo += lgth;
        ch = ch->next;
        g_assert(ch != NULL);
    }

    p->data = ch->data;
    *len = lgth;
    *buffer_offset = new_bo;

    return ch;
}

#define sample_display_prepare_data_strobo_do(TYPE)\
    for (;nsegs < width; nsegs++, x++, c = d) {\
        offs = XPOS_TO_OFFSET(x) - buffer_offset;\
        if (offs >= len)\
            break;\
\
        d = normalize(p->data, offs, 0, len, s->height, TYPE);\
        p->segments[nsegs].x1 = x - 1;\
        p->segments[nsegs].y1 = c;\
        p->segments[nsegs].x2 = x;\
        p->segments[nsegs].y2 = d;\
    }

#define sample_display_prepare_data_strobo_do_stereo(TYPE)\
    for (;nsegs < width; nsegs++, x++, c = d, c1 = d1) {\
        guint addr1 = nsegs << 1, addr2 = addr1 + 1;\
        offs = XPOS_TO_OFFSET(x) - buffer_offset;\
        if (offs >= len)\
            break;\
\
        d = normalize(p->data, offs, 0, len, h2, TYPE);\
        p->segments[addr1].x1 = x - 1;\
        p->segments[addr1].y1 = c;\
        p->segments[addr1].x2 = x;\
        p->segments[addr1].y2 = d;\
\
        d1 = normalize(p->data, offs + len, len, len2, h2, TYPE);\
        p->segments[addr2].x1 = x - 1;\
        p->segments[addr2].y1 = c1 + h2;\
        p->segments[addr2].x2 = x;\
        p->segments[addr2].y2 = d1 + h2;\
    }

static gint
sample_display_prepare_data_strobo(const SampleDisplay* s,
    guint x,
    const guint width)
{
    STSampleChain* ch;
    SampleDisplayPriv* p = s->priv;
    guint nsegs = 0, len, buffer_offset = 0, new_bo, offs;
    gsize len2, h2;
    gint c, d, c1, d1;
    gint prev_x = (x > 0 ? x - 1 : 0);
    const gint type = p->datatype;

    g_assert(p->segments != NULL);
    /* Take the previous offset for consistency */
    offs = XPOS_TO_OFFSET(prev_x);
    if (p->chain)
        ch = offset_lookup(p, p->chain, offs, &len, &buffer_offset);
    else {
        ch = NULL;
        len = p->datalen;
    }
    len2 = len << 1;
    h2 = s->height >> 1;
    if (type & ST_MIXER_FORMAT_STEREO_NI) {
        c = normalize(p->data, offs - buffer_offset, 0, len, h2, type & ~ST_MIXER_FORMAT_STEREO_NI);
        c1 = normalize(p->data, offs - buffer_offset + len, len, len2, h2, type & ~ST_MIXER_FORMAT_STEREO_NI);
    } else
        c1 = c = normalize(p->data, offs - buffer_offset, 0, len, s->height, type);

    while (1) {
        switch (type) {
        case ST_MIXER_FORMAT_S16:
            sample_display_prepare_data_strobo_do(ST_MIXER_FORMAT_S16)
            break;
        case ST_MIXER_FORMAT_U16:
            sample_display_prepare_data_strobo_do(ST_MIXER_FORMAT_U16)
            break;
        case ST_MIXER_FORMAT_S8:
            sample_display_prepare_data_strobo_do(ST_MIXER_FORMAT_S8)
            break;
        case ST_MIXER_FORMAT_U8:
            sample_display_prepare_data_strobo_do(ST_MIXER_FORMAT_U8)
            break;
        case ST_MIXER_FORMAT_S16 | ST_MIXER_FORMAT_STEREO_NI:
            sample_display_prepare_data_strobo_do_stereo(ST_MIXER_FORMAT_S16)
            break;
        case ST_MIXER_FORMAT_U16 | ST_MIXER_FORMAT_STEREO_NI:
            sample_display_prepare_data_strobo_do_stereo(ST_MIXER_FORMAT_U16)
            break;
        case ST_MIXER_FORMAT_S8 | ST_MIXER_FORMAT_STEREO_NI:
            sample_display_prepare_data_strobo_do_stereo(ST_MIXER_FORMAT_S8)
            break;
        case ST_MIXER_FORMAT_U8 | ST_MIXER_FORMAT_STEREO_NI:
            sample_display_prepare_data_strobo_do_stereo(ST_MIXER_FORMAT_U8)
            break;
        case ST_MIXER_FORMAT_S16 | ST_MIXER_FORMAT_STEREO:
            sample_display_prepare_data_strobo_do(ST_MIXER_FORMAT_S16 | ST_MIXER_FORMAT_STEREO)
            break;
        case ST_MIXER_FORMAT_U16 | ST_MIXER_FORMAT_STEREO:
            sample_display_prepare_data_strobo_do(ST_MIXER_FORMAT_U16 | ST_MIXER_FORMAT_STEREO)
            break;
        case ST_MIXER_FORMAT_S8 | ST_MIXER_FORMAT_STEREO:
            sample_display_prepare_data_strobo_do(ST_MIXER_FORMAT_S8 | ST_MIXER_FORMAT_STEREO)
            break;
        case ST_MIXER_FORMAT_U8 | ST_MIXER_FORMAT_STEREO:
            sample_display_prepare_data_strobo_do(ST_MIXER_FORMAT_U8 | ST_MIXER_FORMAT_STEREO)
            break;
        default:
            g_assert_not_reached();
        }

        if (!p->chain || nsegs >= width)
            break;
        ch = offset_lookup(p, ch, offs, &len, &new_bo);
        buffer_offset += new_bo;
    }

    return type & ST_MIXER_FORMAT_STEREO_NI ? nsegs << 1 : nsegs;
}

#define sample_display_prepare_data_minmax_do(TYPE, __n)\
    for (; nsegs <= width; nsegs++, x++) {\
        gint tmp;\
        guint endoffs = XPOS_TO_OFFSET(x + 1) - buffer_offset;\
\
        for(; offs < endoffs; offs += 1) {\
            gint curval;\
\
            if (offs >= len) { /* The chunk is out */\
                goto out ## __n;\
            }\
            curval = normalize(p->data, offs, 0, len, s->height, TYPE);\
            if (curval < c)\
                c = curval;\
            if (curval > d)\
                d = curval;\
        }\
        p->segments[nsegs].x1 = x;\
        p->segments[nsegs].y1 = c;\
        p->segments[nsegs].x2 = x;\
        p->segments[nsegs].y2 = d;\
\
        offs = endoffs;\
        tmp = d + 1;\
        d = c - 1;\
        c = tmp;\
    }\
out ## __n:

#define sample_display_prepare_data_minmax_do_stereo(TYPE, __n)\
    for (; nsegs <= width; nsegs++, x++) {\
        guint tmp, addr1 = nsegs << 1, addr2 = addr1 + 1;\
        guint endoffs = XPOS_TO_OFFSET(x + 1) - buffer_offset;\
\
        for(; offs < endoffs; offs += 1) {\
            gint curval;\
\
            if (offs >= len) { /* The chunk is out */\
                goto outs ## __n;\
            }\
            curval = normalize(p->data, offs, 0, len, h2, TYPE);\
            if (curval < c)\
                c = curval;\
            if (curval > d)\
                d = curval;\
            curval = normalize(p->data, offs + len, len, len2, h2, TYPE);\
            if (curval < c1)\
                c1 = curval;\
            if (curval > d1)\
                d1 = curval;\
        }\
        p->segments[addr1].x1 = x;\
        p->segments[addr1].y1 = c;\
        p->segments[addr1].x2 = x;\
        p->segments[addr1].y2 = d;\
        p->segments[addr2].x1 = x;\
        p->segments[addr2].y1 = c1 + h2;\
        p->segments[addr2].x2 = x;\
        p->segments[addr2].y2 = d1 + h2;\
\
        offs = endoffs;\
        tmp = d + 1;\
        d = c - 1;\
        c = tmp;\
        tmp = d1 + 1;\
        d1 = c1 - 1;\
        c1 = tmp;\
    }\
outs ## __n:

static guint
sample_display_prepare_data_minmax(const SampleDisplay* s,
    guint x,
    guint width)
{
    guint nsegs, len;
    gsize len2, h2;
    STSampleChain* ch;
    guint buffer_offset = 0, new_bo, offs;
    gint c, d, c1, d1;
    SampleDisplayPriv* p = s->priv;
    gint type = p->datatype;

    if (s->width >= s->win_length)
        return sample_display_prepare_data_strobo(s, x, width);

    g_assert(p->segments != NULL);

    nsegs = 0;
    offs = XPOS_TO_OFFSET(x);
    /* Take the previous offset for consistency */
    offs = (offs == 0 ? 0 : offs - 1);
    if (p->chain)
        ch = offset_lookup(p, p->chain, offs, &len, &buffer_offset);
    else {
        ch = NULL;
        len = p->datalen;
    }
    len2 = len << 1;
    h2 = s->height >> 1;
    if (type & ST_MIXER_FORMAT_STEREO_NI) {
        c = d = normalize(p->data, offs - buffer_offset, 0, len, h2, type & ~ST_MIXER_FORMAT_STEREO_NI);
        c1 = d1 = normalize(p->data, offs - buffer_offset + len, len, len2, h2, type & ~ST_MIXER_FORMAT_STEREO_NI);
    } else
        c1 = d1 = c = d = normalize(p->data, offs - buffer_offset, 0, len, s->height, type);

    while (1) {
        switch (type) {
        case ST_MIXER_FORMAT_S16:
            sample_display_prepare_data_minmax_do(ST_MIXER_FORMAT_S16, 0)
            break;
        case ST_MIXER_FORMAT_U16:
            sample_display_prepare_data_minmax_do(ST_MIXER_FORMAT_U16, 1)
            break;
        case ST_MIXER_FORMAT_S8:
            sample_display_prepare_data_minmax_do(ST_MIXER_FORMAT_S8, 2)
            break;
        case ST_MIXER_FORMAT_U8:
            sample_display_prepare_data_minmax_do(ST_MIXER_FORMAT_U8, 3)
            break;
        case ST_MIXER_FORMAT_S16 | ST_MIXER_FORMAT_STEREO_NI:
            sample_display_prepare_data_minmax_do_stereo(ST_MIXER_FORMAT_S16, 0)
            break;
        case ST_MIXER_FORMAT_U16 | ST_MIXER_FORMAT_STEREO_NI:
            sample_display_prepare_data_minmax_do_stereo(ST_MIXER_FORMAT_U16, 1)
            break;
        case ST_MIXER_FORMAT_S8 | ST_MIXER_FORMAT_STEREO_NI:
            sample_display_prepare_data_minmax_do_stereo(ST_MIXER_FORMAT_S8, 2)
            break;
        case ST_MIXER_FORMAT_U8 | ST_MIXER_FORMAT_STEREO_NI:
            sample_display_prepare_data_minmax_do_stereo(ST_MIXER_FORMAT_U8, 3)
            break;
        case ST_MIXER_FORMAT_S16 | ST_MIXER_FORMAT_STEREO:
            sample_display_prepare_data_minmax_do(ST_MIXER_FORMAT_S16 | ST_MIXER_FORMAT_STEREO, 4)
            break;
        case ST_MIXER_FORMAT_U16 | ST_MIXER_FORMAT_STEREO:
            sample_display_prepare_data_minmax_do(ST_MIXER_FORMAT_U16 | ST_MIXER_FORMAT_STEREO, 5)
            break;
        case ST_MIXER_FORMAT_S8 | ST_MIXER_FORMAT_STEREO:
            sample_display_prepare_data_minmax_do(ST_MIXER_FORMAT_S8 | ST_MIXER_FORMAT_STEREO, 6)
            break;
        case ST_MIXER_FORMAT_U8 | ST_MIXER_FORMAT_STEREO:
            sample_display_prepare_data_minmax_do(ST_MIXER_FORMAT_U8 | ST_MIXER_FORMAT_STEREO, 7)
            break;
        default:
            g_assert_not_reached();
        }

        if (!p->chain || nsegs >= width)
            break;
        ch = offset_lookup(p, ch, offs, &len, &new_bo);
        buffer_offset += new_bo;
        offs -= new_bo;
    }
    return type & ST_MIXER_FORMAT_STEREO_NI ? nsegs << 1 : nsegs;
}

static void
sample_display_draw_data(DIDrawable win,
    const SampleDisplay* s,
    const gboolean color,
    const guint x,
    const guint width)
{
    SampleDisplayPriv* p = s->priv;
    DIGC gc;
    gint nseg;

    if (width == 0)
        return;

    g_return_if_fail(x >= 0);
    g_return_if_fail(x + width <= s->width);

    di_draw_rectangle(win, color ? p->fg_gc : p->bg_gc,
        TRUE, x, 0, width, s->height);

    if (p->display_zero_line) {
        if (p->datatype & ST_MIXER_FORMAT_STEREO_NI) {
            di_draw_line(win, p->zeroline_gc, x, s->height >> 2, x + width - 1, s->height >> 2);
            di_draw_line(win, p->zeroline_gc, x, (s->height >> 2) + (s->height >> 1),
                x + width - 1, (s->height >> 2) + (s->height >> 1));
        } else
            di_draw_line(win, p->zeroline_gc, x, s->height >> 1, x + width - 1, s->height >> 1);
    }

    gc = color ? p->bg_gc : p->fg_gc;

    switch(p->mode) {
    case SAMPLE_DISPLAY_MODE_STROBO:
        nseg = sample_display_prepare_data_strobo(s, x, width);
        break;
    case SAMPLE_DISPLAY_MODE_MINMAX:
        nseg = sample_display_prepare_data_minmax(s, x, width);
        break;
    default:
        nseg = 0; /* Unknown display mode, just draw nothing */
        break;
    }
    di_draw_segments(win, gc, p->segments, nseg);
}

gint
sample_display_offset_to_xpos(SampleDisplay* s,
    const gint offset)
{
    gint64 d = offset - s->win_start;

    if (d < 0)
        return 0;
    if (d >= s->win_length)
        return s->width;

    return d * s->width / s->win_length;
}

static int
sample_display_endoffset_to_xpos(SampleDisplay* s,
    int offset)
{
    if (s->win_length < s->width) {
        return sample_display_offset_to_xpos(s, offset);
    } else {
        gint64 d = offset - s->win_start;
        int l = (1 - s->win_length) / s->width;

        /* you get these tests by setting the complete formula below equal to 0 or s->width, respectively,
	   and then resolving towards d. */
        if (d < l)
            return 0;
        if (d > s->win_length + l)
            return s->width;

        return (d * s->width + s->win_length - 1) / s->win_length;
    }
}

static void
sample_display_do_marker_line(DIDrawable win,
    SampleDisplay* s,
    int endoffset,
    int offset,
    int x_min,
    int x_max,
    DIGC gc)
{
    int x;

    if (offset >= s->win_start && offset <= s->win_start + s->win_length) {
        if (!endoffset)
            x = sample_display_offset_to_xpos(s, offset);
        else
            x = sample_display_endoffset_to_xpos(s, offset);
        if (x + 3 >= x_min && x - 3 < x_max) {
            di_draw_line(win, gc, x, 0, x, s->height);
            di_draw_rectangle(win, gc, TRUE,
                x - 3, 0, 7, 10);
            di_draw_rectangle(win, gc, TRUE,
                x - 3, s->height - 10, 7, 10);
        }
    }
}

static gboolean
sample_display_draw_main(GtkWidget* widget,
    GdkRectangle* area)
{
    SampleDisplay* s = SAMPLE_DISPLAY(widget);
    SampleDisplayPriv* p = s->priv;
    DIDrawable drw = custom_drawing_get_drawable(CUSTOM_DRAWING(widget));
    GdkEventExpose event;
    int x, x2;

    g_return_val_if_fail(area->x >= 0, FALSE);

    if (area->width == 0)
        return FALSE;

    if (area->x + area->width > s->width)
        return FALSE;

    if (!IS_INITIALIZED(s)) {
        di_draw_rectangle(drw, p->bg_gc,
            TRUE, area->x, area->y, area->width, area->height);
        di_draw_line(drw, p->fg_gc,
            area->x, s->height / 2,
            area->x + area->width - 1, s->height / 2);
    } else {
        const int x_min = area->x;
        /* This helps against some discontinuity artifacts */
        const int x_max = MIN(area->x + area->width + 1, s->width);

        if (s->sel_start != -1) {
            /* draw the part to the left of the selection */
            x = sample_display_offset_to_xpos(s, s->sel_start);
            x = MIN(x_max, MAX(x_min, x));
            sample_display_draw_data(drw, s, 0, x_min, x - x_min);

            /* draw the selection */
            x2 = sample_display_endoffset_to_xpos(s, s->sel_end);
            x2 = MIN(x_max, MAX(x_min, x2));
            sample_display_draw_data(drw, s, 1, x, x2 - x);
        } else {
            /* we don't have a selection */
            x2 = x_min;
        }

        /* draw the part to the right of the selection */
        sample_display_draw_data(drw, s, 0, x2, x_max - x2);

        if (p->loop_start != -1) {
            sample_display_do_marker_line(drw, s, 0, p->loop_start, x_min, x_max, p->loop_gc);
            sample_display_do_marker_line(drw, s, 1, p->loop_end, x_min, x_max, p->loop_gc);
        }
    }

    event.area = *area;
    /* We need to redo explicitly the custom drawing stuff */
    return (*GTK_WIDGET_CLASS(sample_display_parent_class)->expose_event)(widget, &event);
}

static void
sample_display_draw_update(GtkWidget* widget,
    GdkRectangle* area)
{
    SampleDisplay* s = SAMPLE_DISPLAY(widget);
    SampleDisplayPriv* p = s->priv;
    GdkRectangle area2 = { 0, 0, 0, s->height };
    int x;
    const int x_min = area->x;
    const int x_max = area->x + area->width;
    gboolean special_draw = FALSE;

    if (s->sel_start != p->old_ss || s->sel_end != p->old_se) {
        if (s->sel_start == -1 || p->old_ss == -1) {
            sample_display_draw_main(widget, area);
        } else {
            if (s->sel_start < p->old_ss) {
                /* repaint left additional side */
                x = sample_display_offset_to_xpos(s, s->sel_start);
                area2.x = MIN(x_max, MAX(x_min, x));
                x = sample_display_offset_to_xpos(s, p->old_ss);
                area2.width = MIN(x_max, MAX(x_min, x)) - area2.x;
            } else {
                /* repaint left removed side */
                x = sample_display_offset_to_xpos(s, p->old_ss);
                area2.x = MIN(x_max, MAX(x_min, x));
                x = sample_display_offset_to_xpos(s, s->sel_start);
                area2.width = MIN(x_max, MAX(x_min, x)) - area2.x;
            }
            sample_display_draw_main(widget, &area2);

            if (s->sel_end < p->old_se) {
                /* repaint right removed side */
                x = sample_display_endoffset_to_xpos(s, s->sel_end);
                area2.x = MIN(x_max, MAX(x_min, x));
                x = sample_display_endoffset_to_xpos(s, p->old_se);
                area2.width = MIN(x_max, MAX(x_min, x)) - area2.x;
            } else {
                /* repaint right additional side */
                x = sample_display_endoffset_to_xpos(s, p->old_se);
                area2.x = MIN(x_max, MAX(x_min, x));
                x = sample_display_endoffset_to_xpos(s, s->sel_end);
                area2.width = MIN(x_max, MAX(x_min, x)) - area2.x;
            }
            sample_display_draw_main(widget, &area2);
        }

        p->old_ss = s->sel_start;
        p->old_se = s->sel_end;
        special_draw = TRUE;
    }

    if (!special_draw) {
        sample_display_draw_main(widget, area);
    }
}

static gint
sample_display_expose(GtkWidget* widget,
    GdkEventExpose* event)
{
    return sample_display_draw_main(widget, &event->area);
}

static gint
sample_display_idle_draw_function(SampleDisplay* s)
{
    GdkRectangle area = { 0, 0, s->width, s->height };

    if (gtk_widget_get_mapped(GTK_WIDGET(s))) {
        sample_display_draw_update(GTK_WIDGET(s), &area);
    }

    s->priv->idle_handler = 0;
    return FALSE;
}

static void
sample_display_handle_motion(SampleDisplay* s,
    int x,
    int y,
    int just_clicked)
{
    SampleDisplayPriv* p = s->priv;
    int ol, or ;
    int ss = s->sel_start, se = s->sel_end;
    int ls = p->loop_start, le = p->loop_end;

    if (!p->selecting)
        return;

    if (x < 0)
        x = 0;
    else if (x >= s->width)
        x = s->width - 1;

    ol = XPOS_TO_OFFSET(x);
    if (s->win_length < s->width) {
        or = XPOS_TO_OFFSET(x) + 1;
    } else {
        or = XPOS_TO_OFFSET(x + 1);
    }

    g_return_if_fail(ol >= 0 && ol < p->datalen);
    g_return_if_fail(or > 0 && or <= p->datalen);
    g_return_if_fail(ol < or);

    switch (p->selecting) {
    case SELECTING_SELECTION_START:
        if (just_clicked) {
            if (ss != -1 && ol < se) {
                ss = ol;
            } else {
                ss = ol;
                se = ol + 1;
            }
        } else {
            if (ol < se) {
                ss = ol;
            } else {
                ss = se - 1;
                se = or ;
                p->selecting = SELECTING_SELECTION_END;
            }
        }
        break;
    case SELECTING_SELECTION_END:
        if (just_clicked) {
            if (ss != -1 && or > ss) {
                se = or ;
            } else {
                ss = or -1;
                se = or ;
            }
        } else {
            if (or > ss) {
                se = or ;
            } else {
                se = ss + 1;
                ss = ol;
                p->selecting = SELECTING_SELECTION_START;
            }
        }
        break;
    case SELECTING_LOOP_START:
        if (ol < p->loop_end)
            ls = ol;
        else
            ls = le - 1;
        break;
    case SELECTING_LOOP_END:
        if (or > p->loop_start)
            le = or ;
        else
            le = ls + 1;
        break;
    default:
        g_assert_not_reached();
        break;
    }

    if (s->sel_start != ss || s->sel_end != se) {
        s->sel_start = ss;
        s->sel_end = se;
        sample_display_idle_draw(s);
        g_signal_emit(G_OBJECT(s), sample_display_signals[SIG_SELECTION_CHANGED], 0, ss, se);
    }

    if (p->loop_start != ls || p->loop_end != le) {
        p->loop_start = ls;
        p->loop_end = le;
        sample_display_idle_draw(s);
        g_signal_emit(G_OBJECT(s), sample_display_signals[SIG_LOOP_CHANGED], 0, ls, le);
    }
}

// Handle middle mousebutton display window panning
static void
sample_display_handle_motion_2(SampleDisplay* s,
    int x,
    int y)
{
    SampleDisplayPriv* p = s->priv;
    int new_win_start = p->selecting_wins0 + (p->selecting_x0 - x) * s->win_length / s->width;

    new_win_start = CLAMP(new_win_start, 0, p->datalen - s->win_length);

    if (new_win_start != s->win_start) {
        sample_display_set_window_full(s, new_win_start, -1, TRUE);
    }
}

static gint
sample_display_button_press(GtkWidget* widget,
    GdkEventButton* event)
{
    SampleDisplay* s;
    SampleDisplayPriv* p;
    int x, y;
    GdkModifierType state;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(IS_SAMPLE_DISPLAY(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    s = SAMPLE_DISPLAY(widget);
    p = s->priv;

    if (!p->edit)
        return FALSE;

    if (!IS_INITIALIZED(s))
        return TRUE;

    if (p->selecting && event->button != p->button) {
        p->selecting = 0;
    } else {
        p->button = event->button;
        gdk_window_get_pointer(event->window, &x, &y, &state);
        if (!(state & GDK_SHIFT_MASK)) {
            if (p->button == 1) {
                p->selecting = SELECTING_SELECTION_START;
            } else if (p->button == 2) {
                p->selecting = SELECTING_PAN_WINDOW;
                gdk_window_get_pointer(event->window, &p->selecting_x0, NULL, NULL);
                p->selecting_wins0 = s->win_start;
            } else if (p->button == 3) {
                p->selecting = SELECTING_SELECTION_END;
            }
        } else {
            if (p->loop_start != -1) {
                if (p->button == 1) {
                    p->selecting = SELECTING_LOOP_START;
                } else if (p->button == 3) {
                    p->selecting = SELECTING_LOOP_END;
                }
            }
        }
        if (!p->selecting)
            return TRUE;
        if (p->selecting != SELECTING_PAN_WINDOW)
            sample_display_handle_motion(s, x, y, 1);
    }

    return TRUE;
}

static gint
sample_display_button_release(GtkWidget* widget,
    GdkEventButton* event)
{
    SampleDisplay* s;
    SampleDisplayPriv* p;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(IS_SAMPLE_DISPLAY(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    s = SAMPLE_DISPLAY(widget);
    p = s->priv;

    if (!p->edit)
        return FALSE;

    if (p->selecting && event->button == p->button) {
        p->selecting = 0;
    }

    return TRUE;
}

static gint
sample_display_motion_notify(GtkWidget* widget,
    GdkEventMotion* event)
{
    SampleDisplay* s;
    SampleDisplayPriv* p;
    int x, y;
    GdkModifierType state;

    s = SAMPLE_DISPLAY(widget);
    p = s->priv;

    if (!p->edit)
        return FALSE;

    if (!IS_INITIALIZED(s) || !p->selecting)
        return TRUE;

    if (event->is_hint) {
        gdk_window_get_pointer(event->window, &x, &y, &state);
    } else {
        x = event->x;
        y = event->y;
        state = event->state;
    }

    if (((state & GDK_BUTTON1_MASK) && p->button == 1) || ((state & GDK_BUTTON3_MASK) && p->button == 3)) {
        sample_display_handle_motion(SAMPLE_DISPLAY(widget), x, y, 0);
    } else if ((state & GDK_BUTTON2_MASK) && p->button == 2) {
        sample_display_handle_motion_2(SAMPLE_DISPLAY(widget), x, y);
    } else {
        p->selecting = 0;
    }

    return TRUE;
}

static void
sample_display_class_init(SampleDisplayClass* class)
{
    GObjectClass* object_class;
    GtkWidgetClass* widget_class;

    object_class = G_OBJECT_CLASS(class);
    widget_class = GTK_WIDGET_CLASS(class);

    widget_class->realize = sample_display_realize;
    widget_class->size_allocate = sample_display_size_allocate;
    widget_class->expose_event = sample_display_expose;
    widget_class->size_request = sample_display_size_request;
    widget_class->button_press_event = sample_display_button_press;
    widget_class->button_release_event = sample_display_button_release;
    widget_class->motion_notify_event = sample_display_motion_notify;

    sample_display_signals[SIG_SELECTION_CHANGED] = g_signal_new("selection_changed",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(SampleDisplayClass, selection_changed),
        NULL, NULL,
        __marshal_VOID__INT_INT,
        G_TYPE_NONE, 2,
        G_TYPE_INT, G_TYPE_INT);
    sample_display_signals[SIG_LOOP_CHANGED] = g_signal_new("loop_changed",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(SampleDisplayClass, loop_changed),
        NULL, NULL,
        __marshal_VOID__INT_INT,
        G_TYPE_NONE, 2,
        G_TYPE_INT, G_TYPE_INT);
    sample_display_signals[SIG_WINDOW_CHANGED] = g_signal_new("window_changed",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(SampleDisplayClass, window_changed),
        NULL, NULL,
        __marshal_VOID__INT_INT,
        G_TYPE_NONE, 2,
        G_TYPE_INT, G_TYPE_INT);
    sample_display_signals[SIG_POS_CHANGED] =
        g_signal_new ("position_changed",
        G_TYPE_FROM_CLASS (object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(SampleDisplayClass, position_changed),
        NULL, NULL,
        g_cclosure_marshal_VOID__INT,
        G_TYPE_NONE, 1,
        G_TYPE_INT);

    class->selection_changed = NULL;
    class->loop_changed = NULL;
    class->window_changed = NULL;
    class->position_changed = NULL;
}

static void
sample_display_init(SampleDisplay* s)
{
    s->priv = g_new0(SampleDisplayPriv, 1);
    di_widget_configure(GTK_WIDGET(s));
}

GtkWidget*
sample_display_new(const gboolean edit, const SampleDisplayMode mode)
{
    SampleDisplay* s = SAMPLE_DISPLAY(g_object_new(sample_display_get_type(), NULL));

    s->priv->edit = edit;
    s->priv->mode = mode;
    return GTK_WIDGET(s);
}
