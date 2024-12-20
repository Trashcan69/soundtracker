
/*
 * The Real SoundTracker - main window oscilloscope group
 *
 * Copyright (C) 1998-2019 Michael Krause
 * Copyright (C) 2020-2022 Yury Aliaev
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <glib/gprintf.h>

#include "audio.h"
#include "colors.h"
#include "draw-interlayer.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "sample-display.h"
#include "scalablepic.h"
#include "scope-group.h"
#include "xm.h"

typedef enum {
    POSITION_TOP,
    POSITION_LEFT = POSITION_TOP,
    POSITION_BOTTOM,
    POSITION_RIGHT = POSITION_BOTTOM
} ElementPosition;

typedef struct {
    gchar* text;
    ElementPosition pos_x, pos_y;
    gint x, y;
} TextElement;

typedef struct {
    DIGC mask_gc, mask_bg_gc;
    DIDrawable drawable;
    PangoLayout* layout;
    GdkPixmap* pixmap;
    GdkColor *num_color, *bg_color;
    /* Size of the drawing area */
    gint width, height;
    /* Size and position of the pixmap */
    GdkRectangle rect;
    GSList* elements;
    gboolean firsttime, modified;
    unsigned int old_pixel;
} TextLayer;

struct _ScopeGroupPriv {
    GtkWidget* table;
    SampleDisplay* scopes[XM_NUM_CHAN];
    GtkWidget* scopebuttons[XM_NUM_CHAN];
    GtkWidget *mutedpic[XM_NUM_CHAN], *unmutedpic[XM_NUM_CHAN];
    TextLayer number_m[XM_NUM_CHAN], number_u[XM_NUM_CHAN], number_c[XM_NUM_CHAN];
    TextElement chan_info[XM_NUM_CHAN];
    gint inst[XM_NUM_CHAN], smpl[XM_NUM_CHAN];
    gchar i_s[XM_NUM_CHAN][8]; /* 3 (instr) + ":" + 3 (smpl) + \0 channel info */
    int numchan;
    int scopes_on;
    int update_freq;
    gint32 on_mask;

    DIGC bg_gc;
};

G_DEFINE_TYPE(ScopeGroup, scope_group, GTK_TYPE_HBOX)

/* Displacement between the channel number and the left top corner:
   x_displ = XDISP + width / FRACTION, the same is for y coordinate */
static guint XDISP = 1, YDISP = 1, FRACTION = 200;

static void
scope_group_set_channel_state(ScopeGroupPriv *s,
    const guint n,
    const gboolean on)
{
    if (s->scopes_on) {
        gtk_widget_hide(on ? s->mutedpic[n] : GTK_WIDGET(s->scopes[n]));
        gtk_widget_show(on ? GTK_WIDGET(s->scopes[n]) : s->mutedpic[n]);
    } else {
        gtk_widget_hide(on ? s->mutedpic[n] : s->unmutedpic[n]);
        gtk_widget_show(on ? s->unmutedpic[n] : s->mutedpic[n]);
    }
}

void
scope_group_toggle_channel(ScopeGroup *s,
    const guint n)
{
    const guint mask = 1 << n;
    gboolean on = ((s->priv->on_mask & mask) != 0);

    player_mute_channels[n] = on;
    s->priv->on_mask ^= mask;

    scope_group_set_channel_state(s->priv, n, !on);
}

void
scope_group_solo_channel(ScopeGroup *sg,
    const guint n)
{
    gint i;

    for (i = 0; i < sg->priv->numchan; i++) {
        gboolean on = ((sg->priv->on_mask & (1 << i)) != 0);

        if ((i == n) != on)
            scope_group_set_channel_state(sg->priv, i, i == n);
        player_mute_channels[i] = (i != n);
    }

    sg->priv->on_mask = 1 << n;
}

void
scope_group_all_channels_on(ScopeGroup *sg)
{
    gint i;

    for (i = 0; i < sg->priv->numchan; i++)
        scope_group_set_channel_state(sg->priv, i, TRUE);
    sg->priv->on_mask = 0xFFFFFFFF;
    memset(player_mute_channels, 0, sizeof(player_mute_channels));
}

static gboolean
scope_group_button_press(GtkWidget* widget,
    GdkEventButton* event,
    gpointer data)
{
    GtkWidget* w;
    ScopeGroup* sg;
    const gint n = GPOINTER_TO_INT(data);
    static guint prev_time = 0;

    /* A strange Gtk+-2 bug, when one real event produces 2 callbacks */
    if (event->time == prev_time)
        return FALSE;
    prev_time = event->time;

    /* enjoy the hack!!! */
    g_return_val_if_fail((w = widget->parent) != NULL, FALSE);
    sg = SCOPE_GROUP(w->parent); /* button<-table<-scope_group */

    if (event->button == 1) {
        scope_group_toggle_channel(sg, n);
        return TRUE;
    } else if (event->button == 3) {
        /* Turn all scopes on if some were muted, or make solo selected otherwise */
        if (sg->priv->on_mask == 0xFFFFFFFF)
            scope_group_solo_channel(sg, n);
        else
            scope_group_all_channels_on(sg);
        return TRUE;
    }

    return FALSE;
}

void scope_group_set_num_channels(ScopeGroup* sg,
    int num_channels)
{
    int i;
    ScopeGroupPriv *s = sg->priv;

    // Remove superfluous scopes from table
    for (i = num_channels; i < s->numchan; i++) {
        gtk_container_remove(GTK_CONTAINER(s->table), s->scopebuttons[i]);
    }

    // Resize table
    gtk_table_resize(GTK_TABLE(s->table), 2, num_channels / 2);

    // Add new scopes to table
    for (i = s->numchan; i < num_channels; i++) {
        g_object_ref(G_OBJECT(s->scopebuttons[i]));
        gtk_table_attach_defaults(GTK_TABLE(s->table), s->scopebuttons[i], i / 2, i / 2 + 1, i % 2, i % 2 + 1);
    }

    s->numchan = num_channels;
    s->on_mask = 0xFFFFFFFF; /* all channels are on */
    memset(player_mute_channels, 0, sizeof(player_mute_channels));

    // Reset all buttons (enable all channels)
    for (i = 0; i < XM_NUM_CHAN; i++)
        scope_group_set_channel_state(s, i, TRUE);
}

void scope_group_enable_scopes(ScopeGroup* sg,
    int enable)
{
    int i;
    ScopeGroupPriv *s = sg->priv;

    s->scopes_on = enable;

    for (i = 0; i < XM_NUM_CHAN; i++) {
        if (s->on_mask & (1 << i)) {
            gtk_widget_hide(enable ? s->unmutedpic[i] : GTK_WIDGET(s->scopes[i]));
            gtk_widget_show(enable ? GTK_WIDGET(s->scopes[i]) : s->unmutedpic[i]);
        }
    }
}

void
scope_group_update_channel_status(ScopeGroup* sg,
    const gint i,
    const gint newinst,
    const gint newsmpl)
{
    ScopeGroupPriv* s = sg->priv;

    if (s->inst[i] != newinst || s->smpl[i] != newsmpl) {
        s->inst[i] = newinst;
        s->smpl[i] = newsmpl;
        if (newinst != -1)
            g_snprintf(s->chan_info[i].text, sizeof(s->i_s[0]), "%i:%i", newinst, newsmpl);
        else
            s->chan_info[i].text[0] = '\0';
        s->number_c[i].modified = TRUE;
    }
}

void
scope_group_timeout(ScopeGroup* sg, gdouble time1)
{
    ScopeGroupPriv* s = sg->priv;
    double time2;
    int i, l;
    static gint16* buf = NULL;
    static int bufsize = 0;
    int o1, o2;

    if (!s->scopes_on || !scopebuf_ready || !current_driver)
        return;

    time2 = time1 + (double)1 / s->update_freq;

    for (i = 0; i < 2; i++) {
        if (time1 < scopebuf_start.time || time2 < scopebuf_start.time) {
            // requesting too old samples -- scopebuf_length is too low
            goto ende;
        }

        if (time1 >= scopebuf_end.time || time2 >= scopebuf_end.time) {
            /* requesting samples which haven't been even rendered yet.
	       can happen with short driver latencies. */
            time1 -= (double)1 / s->update_freq;
            time2 -= (double)1 / s->update_freq;
        } else {
            break;
        }
    }

    if (i == 2) {
        goto ende;
    }

    o1 = (time1 - scopebuf_start.time) * scopebuf_freq + scopebuf_start.offset;
    o2 = (time2 - scopebuf_start.time) * scopebuf_freq + scopebuf_start.offset;

    l = o2 - o1;
    if (bufsize < l) {
        free(buf);
        buf = malloc(2 * l);
        bufsize = l;
    }

    o1 %= scopebuf_length;
    o2 %= scopebuf_length;
    g_assert(o1 >= 0 && o1 <= scopebuf_length);
    g_assert(o2 >= 0 && o2 <= scopebuf_length);

    for (i = 0; i < s->numchan; i++) {
        if (o2 > o1) {
            sample_display_set_data(s->scopes[i], scopebufs[i] + o1, ST_MIXER_FORMAT_S16_LE, l, TRUE);
        } else {
            memcpy(buf, scopebufs[i] + o1, 2 * (scopebuf_length - o1));
            memcpy(buf + scopebuf_length - o1, scopebufs[i], 2 * o2);
            sample_display_set_data(s->scopes[i], buf, ST_MIXER_FORMAT_S16_LE, l, TRUE);
        }
    }

    return;

ende:
    for (i = 0; i < s->numchan; i++) {
        sample_display_set_data(s->scopes[i], NULL, ST_MIXER_FORMAT_S8, 0, FALSE);
    }
}

void scope_group_stop_updating(ScopeGroup* sg)
{
    int i;
    ScopeGroupPriv *s = sg->priv;

    if (!s->scopes_on)
        return;

    for (i = 0; i < s->numchan; i++) {
        if (s->chan_info[i].text) {
            s->chan_info[i].text[0] = '\0';
        }
        s->number_c[i].modified = TRUE;
        sample_display_set_data(s->scopes[i], NULL, ST_MIXER_FORMAT_S8, 0, FALSE);
    }
}

void scope_group_set_update_freq(ScopeGroup* sg,
    int freq)
{
    ScopeGroupPriv *s = sg->priv;

    s->update_freq = freq;
}

static void
draw_text(cairo_t* cr, PangoLayout* layout, gdouble x, gdouble y)
{
    cairo_move_to(cr, x, y);
    pango_cairo_update_layout(cr, layout);
    pango_cairo_show_layout(cr, layout);
}

/* Making a pixmap cache for each channel number */
static void
text_layer_update_pixmap_cache(GtkWidget* widget, TextLayer* no)
{
    if (gtk_widget_get_realized(widget)) {
        cairo_t* cr;
        cairo_font_options_t* cfo;
        GSList* l = no->elements;
        gboolean first = TRUE;

        g_assert(l != NULL);

        for (; l; l = l->next) {
            TextElement* te = l->data;

            if (te->text[0] != '\0') {
                GdkRectangle rect;

                pango_layout_set_text(no->layout, te->text, -1);
                pango_layout_get_pixel_size(no->layout, &rect.width, &rect.height);
                rect.width += 2; /* For outline */
                rect.height += 2;
                te->x = rect.x = (te->pos_x == POSITION_LEFT) ? XDISP + widget->allocation.width / FRACTION :
                    widget->allocation.width - (XDISP + widget->allocation.width / FRACTION) -
                    rect.width;
                te->y = rect.y = (te->pos_y == POSITION_TOP) ? YDISP + widget->allocation.height / FRACTION :
                    widget->allocation.height - (YDISP + widget->allocation.height / FRACTION) -
                    rect.height;

                if (first) {
                    first = FALSE;
                    no->rect = rect;
                } else
                    gdk_rectangle_union(&no->rect, &rect, &no->rect);
            }
        }

        if (no->pixmap)
            g_object_unref(no->pixmap);
        no->pixmap = gdk_pixmap_new(widget->window, no->rect.width, no->rect.height, -1);
        no->drawable = di_get_drawable(no->pixmap);
        cr = gdk_cairo_create(no->pixmap);

        /* Switching off antialiasing improves channel numbers look making them brighter */
        cfo = cairo_font_options_create();
        cairo_get_font_options(cr, cfo);
        cairo_font_options_set_antialias(cfo, CAIRO_ANTIALIAS_NONE);
        cairo_set_font_options(cr, cfo);
        cairo_font_options_destroy(cfo);

        di_draw_rectangle(no->drawable, no->mask_bg_gc,
            TRUE, 0, 0, no->rect.width, no->rect.height);

        for (l = no->elements; l; l = l->next) {
            TextElement* te = l->data;

            if (te->text) {
                gint x = te->x - no->rect.x;
                gint y = te->y - no->rect.y;

                pango_layout_set_text(no->layout, te->text, -1);

                /* Outline */
                gdk_cairo_set_source_color(cr, no->bg_color);
                draw_text(cr, no->layout, x + 1.0, y + 0.0);
                draw_text(cr, no->layout, x + 2.0, y + 0.0);
                draw_text(cr, no->layout, x + 2.0, y + 1.0);
                draw_text(cr, no->layout, x + 2.0, y + 2.0);
                draw_text(cr, no->layout, x + 1.0, y + 2.0);
                draw_text(cr, no->layout, x + 0.0, y + 2.0);
                draw_text(cr, no->layout, x + 0.0, y + 1.0);

                /* Symbols */
                gdk_cairo_set_source_color(cr, no->num_color);
                draw_text(cr, no->layout, x + 1.0, y + 1.0);
            }
        }
        cairo_destroy(cr);
        no->modified = FALSE;
    }
}

static void
text_layer_update_colors(TextLayer* no)
{
    no->num_color = colors_get_color(COLOR_CHANNUMS);
    no->old_pixel = no->num_color->pixel;
    no->bg_color = colors_get_color(COLOR_UNMUTED_BG);
}

static void
text_layer_style_set(GtkWidget* widget, gpointer data)
{
    TextLayer* no=(TextLayer*)data;

    if (no->layout) {
        pango_layout_set_font_description(no->layout, gtk_widget_get_style(widget)->font_desc);
        text_layer_update_colors(no);
        text_layer_update_pixmap_cache(widget, no);
    }
}

static void
text_layer_realize(GtkWidget* widget, gpointer data)
{
    TextLayer *no = (TextLayer*)data;

    if (no->firsttime) {
        no->firsttime = FALSE;
        no->mask_gc = di_gc_new(widget->window);
        no->mask_bg_gc = colors_get_gc(COLOR_BLACK);
    }

    if (!no->layout) {
        PangoContext* context = gtk_widget_create_pango_context(widget);
        no->layout = pango_layout_new(context);
        g_object_unref(context);
    }

    text_layer_style_set(widget, data);
}

static void
text_layer_draw(GtkWidget* widget, GdkRectangle* area, gpointer data)
{
    TextLayer *no = (TextLayer*)data;
    GdkRectangle dest_area;

    if (no->old_pixel != colors_get_color(COLOR_CHANNUMS)->pixel) {
        text_layer_update_colors(no);
        no->modified = TRUE;
    }

    /* Number color is changed, we need to update the pixmap cache */
    if (no->old_pixel != colors_get_color(COLOR_CHANNUMS)->pixel)
        text_layer_style_set(widget, data);

    /* Update number mask if necessary */
    if (widget->allocation.width != no->width ||
        widget->allocation.height != no->height ||
        no->modified) {
        no->width = widget->allocation.width;
        no->height = widget->allocation.height;
        text_layer_update_pixmap_cache(widget, no);
        di_update_mask(widget->window, no->pixmap, no->mask_gc, no->rect.x, no->rect.y,
            no->rect.width, no->rect.height, widget->allocation.width, widget->allocation.height);
    }

    dest_area = no->rect;
    if (gdk_rectangle_intersect(area, &dest_area, &dest_area))
        di_draw_drawable(custom_drawing_get_drawable(CUSTOM_DRAWING(widget)), no->mask_gc,
            no->drawable, dest_area.x - no->rect.x, dest_area.y - no->rect.y,
            dest_area.x, dest_area.y, dest_area.width, dest_area.height);
}

static void
text_layer_init_number(TextLayer* no, const gint number)
{
    TextElement* element;

    no->width = -1;
    no->height = -1;
    no->elements = g_slist_prepend(NULL, element = g_new(TextElement, 1));
    element->text = g_strdup_printf("%02d", number);
    element->pos_x = POSITION_LEFT;
    element->pos_y = POSITION_TOP;
    no->pixmap = NULL;
    no->layout = NULL;
    no->firsttime = TRUE;
    no->modified = TRUE;
    no->old_pixel = 0;
}

void
scope_group_set_mode(ScopeGroup* s, SampleDisplayMode mode)
{
    guint i;

    for (i = 0; i < XM_NUM_CHAN; i++)
        sample_display_set_mode(s->priv->scopes[i], gui_settings.scopes_mode);
}

GtkWidget*
scope_group_new(GdkPixbuf* mutedpic, GdkPixbuf* unmutedpic)
{
    ScopeGroup* sg;
    ScopeGroupPriv *s;
    gint i;

    sg = g_object_new(scope_group_get_type(), NULL);
    GTK_BOX(sg)->spacing = 2;
    GTK_BOX(sg)->homogeneous = FALSE;

    s = sg->priv = g_new(ScopeGroupPriv, 1);
    s->scopes_on = 0;
    s->update_freq = 40;
    s->numchan = 2;
    s->on_mask = 0xFFFFFFFF;

    s->table = gtk_table_new(2, 1, TRUE);
    gtk_table_set_col_spacings(GTK_TABLE(s->table), 4);
    gtk_table_set_row_spacings(GTK_TABLE(s->table), 4);
    gtk_widget_show(s->table);
    gtk_box_pack_start(GTK_BOX(sg), s->table, TRUE, TRUE, 0);

    for (i = 0; i < XM_NUM_CHAN; i++) {
        GtkWidget *box, *thing;

        box = gtk_vbox_new(FALSE, 0);
        s->scopebuttons[i] = box;
        g_signal_connect(box, "button-press-event",
            G_CALLBACK(scope_group_button_press), GINT_TO_POINTER(i));
        g_object_ref(box);
        gtk_widget_show(box);

        thing = scalable_pic_new(mutedpic, TRUE);
        gtk_widget_add_events(thing, GDK_BUTTON_PRESS_MASK);
        text_layer_init_number(&s->number_m[i], i + 1);
        custom_drawing_register_drawing_func(CUSTOM_DRAWING(thing), text_layer_draw, &s->number_m[i]);
        custom_drawing_register_realize_func(CUSTOM_DRAWING(thing), text_layer_realize, &s->number_m[i]);
        custom_drawing_register_style_set_func(CUSTOM_DRAWING(thing), text_layer_style_set, &s->number_m[i]);
        colors_add_widget(COLOR_CHANNUMS, thing);
        gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
        s->mutedpic[i] = thing;

        thing = scalable_pic_new(unmutedpic, FALSE);
        gtk_widget_add_events(thing, GDK_BUTTON_PRESS_MASK);
        text_layer_init_number(&s->number_u[i], i + 1);
        custom_drawing_register_drawing_func(CUSTOM_DRAWING(thing), text_layer_draw, &s->number_u[i]);
        custom_drawing_register_realize_func(CUSTOM_DRAWING(thing), text_layer_realize, &s->number_u[i]);
        custom_drawing_register_style_set_func(CUSTOM_DRAWING(thing), text_layer_style_set, &s->number_u[i]);
        colors_add_widget(COLOR_CHANNUMS, thing);
        gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
        s->unmutedpic[i] = thing;

        thing = sample_display_new(FALSE, gui_settings.scopes_mode);
        gtk_widget_add_events(thing, GDK_BUTTON_PRESS_MASK);
        text_layer_init_number(&s->number_c[i], i + 1);
        s->chan_info[i].text = s->i_s[i];
        s->i_s[i][0] = '\0';
        s->chan_info[i].pos_x = POSITION_LEFT;
        s->chan_info[i].pos_y = POSITION_BOTTOM;
        s->number_c[i].elements = g_slist_prepend(s->number_c[i].elements, &s->chan_info[i]);
        custom_drawing_register_drawing_func(CUSTOM_DRAWING(thing), text_layer_draw, &s->number_c[i]);
        custom_drawing_register_realize_func(CUSTOM_DRAWING(thing), text_layer_realize, &s->number_c[i]);
        custom_drawing_register_style_set_func(CUSTOM_DRAWING(thing), text_layer_style_set, &s->number_c[i]);
        colors_add_widget(COLOR_CHANNUMS, thing);
        gtk_box_pack_start(GTK_BOX(box), thing, TRUE, TRUE, 0);
        s->scopes[i] = SAMPLE_DISPLAY(thing);

        s->inst[i] = -1;
        s->smpl[i] = -1;
    }

    gtk_table_attach_defaults(GTK_TABLE(s->table), s->scopebuttons[0], 0, 1, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(s->table), s->scopebuttons[1], 0, 1, 1, 2);

    scope_group_set_update_freq(sg, gui_settings.scopes_update_freq);

    return GTK_WIDGET(sg);
}

static void
scope_group_realize(GtkWidget *widget)
{
    static gboolean firsttime = TRUE;
    ScopeGroup *s;
    guint i;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(GTK_IS_HBOX(widget));

    GTK_WIDGET_CLASS(scope_group_parent_class)->realize(widget);
    s = SCOPE_GROUP(widget);

    if (firsttime) {
        firsttime = FALSE;
        s->priv->bg_gc = colors_get_gc(COLOR_UNMUTED_BG);

        for (i = 0; i < XM_NUM_CHAN; i++) {
            scalable_pic_set_bg(SCALABLE_PIC(s->priv->unmutedpic[i]), s->priv->bg_gc);
        }
    }
}

static void
scope_group_class_init(ScopeGroupClass *class)
{
    GtkWidgetClass *widget_class = (GtkWidgetClass*)class;

    widget_class->realize = scope_group_realize;
}

static void
scope_group_init (ScopeGroup *self)
{
}
