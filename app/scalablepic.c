
/*
 * The Real SoundTracker - automatically scalable widget containing
 * an image
 *
 * Copyright (C) 2002, 2019 Yury Aliaev
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

#include <math.h>

#include "scalablepic.h"

static void scalable_pic_class_init(ScalablePicClass* klass);
static void scalable_pic_realize(GtkWidget* widget);
static void scalable_pic_size_allocate(GtkWidget* widget, GtkAllocation* allocation);
static void scalable_pic_send_configure(ScalablePic* sp);
static void scalable_pic_draw(GtkWidget* widget, GdkRectangle* area);
static gint scalable_pic_expose(GtkWidget* widget, GdkEventExpose* event);
static void scalable_pic_size_request(GtkWidget* widget, GtkRequisition* requisition);
static void scalable_pic_destroy(GtkObject* object);

G_DEFINE_TYPE(ScalablePic, scalable_pic, custom_drawing_get_type())

GtkWidget* scalable_pic_new(GdkPixbuf* pic, gboolean expand)
{
    ScalablePic* sp;

    GtkWidget* widget = GTK_WIDGET(g_object_new(scalable_pic_get_type(), NULL));
    sp = SCALABLE_PIC(widget);

    sp->expand = expand;
    sp->pic = pic;
    if (pic != NULL) {
        sp->copy = NULL;
        sp->pic_width = gdk_pixbuf_get_width(sp->pic);
        sp->pic_height = gdk_pixbuf_get_height(sp->pic);
        if (expand) {
            sp->maxwidth = rint(1.414 * (gfloat)(sp->pic_width));
            sp->maxheight = rint(1.414 * (gfloat)(sp->pic_height));
        } else {
            sp->maxwidth = sp->pic_width;
            sp->maxheight = sp->pic_height;
        }
    }

    di_widget_configure(widget);
    return widget;
}

void
scalable_pic_set_bg(ScalablePic* sp, DIGC gc)
{
    sp->bg_gc = gc;
}

static void scalable_pic_class_init(ScalablePicClass* klass)
{
    GtkWidgetClass* widget_class;
    GtkObjectClass* object_class;

    widget_class = (GtkWidgetClass*)klass;
    object_class = (GtkObjectClass*)klass;

    widget_class->realize = scalable_pic_realize;
    widget_class->expose_event = scalable_pic_expose;
    widget_class->size_request = scalable_pic_size_request;
    widget_class->size_allocate = scalable_pic_size_allocate;

    object_class->destroy = scalable_pic_destroy;
}

static void scalable_pic_init(ScalablePic* sp)
{
}

static void scalable_pic_realize(GtkWidget* widget)
{
    GdkWindowAttr attributes;
    guint attributes_mask;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(IS_SCALABLE_PIC(widget));

    gtk_widget_set_realized(widget, TRUE);

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);
    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.event_mask |= GDK_EXPOSURE_MASK;

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
        &attributes, attributes_mask);

    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    scalable_pic_send_configure(SCALABLE_PIC(widget));
    (*GTK_WIDGET_CLASS(scalable_pic_parent_class)->realize)(widget);
}

static void scalable_pic_destroy(GtkObject* object)
{
    ScalablePic* sp;
    GObjectClass* klass;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_SCALABLE_PIC(object));

    sp = SCALABLE_PIC(object);
    if (sp->copy != NULL)
        g_object_unref(sp->copy);

    klass = g_type_class_peek_static(GTK_TYPE_WIDGET);
    if (GTK_OBJECT_CLASS(klass)->destroy)
        (*GTK_OBJECT_CLASS(klass)->destroy)(object);
}

static void scalable_pic_draw(GtkWidget* widget, GdkRectangle* area)
{
    gboolean need_resize = FALSE;

    ScalablePic* sp;
    cairo_t* cr;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(IS_SCALABLE_PIC(widget));

    sp = SCALABLE_PIC(widget);

    if (sp->pic == NULL)
        return;

    cr = gdk_cairo_create(custom_drawing_get_gdk_drawable(CUSTOM_DRAWING(widget)));
    if (widget->allocation.width != sp->former_width
            || widget->allocation.height != sp->former_height
            || sp->copy == NULL) {
        gdouble ratio_x, ratio_y;

        if (sp->expand && widget->allocation.width > sp->maxwidth) {
            sp->maxwidth = widget->allocation.width;
            need_resize = TRUE;
        }
        if (sp->expand && widget->allocation.height > sp->maxheight) {
            sp->maxheight = widget->allocation.height;
            need_resize = TRUE;
        }

        if (need_resize || (sp->copy == NULL)) {
            if (sp->copy)
                g_object_unref(sp->copy);
            /* In case of the non-expandable image this happens only one time */
            sp->copy = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(sp->pic),
                gdk_pixbuf_get_has_alpha(sp->pic),
                gdk_pixbuf_get_bits_per_sample(sp->pic),
                sp->maxwidth, sp->maxheight);
        }

        ratio_x = (double)widget->allocation.width / (double)sp->pic_width;
        ratio_y = (double)widget->allocation.height / (double)sp->pic_height;
        if (sp->expand)
            gdk_pixbuf_scale(sp->pic, sp->copy, 0, 0,
                widget->allocation.width, widget->allocation.height,
                0.0, 0.0, ratio_x, ratio_y, GDK_INTERP_BILINEAR);
        else {
            sp->ratio_min = MIN(ratio_x, ratio_y);

            if (sp->ratio_min < 1.0)
                gdk_pixbuf_scale(sp->pic, sp->copy, 0, 0,
                    sp->pic_width, sp->pic_height,
                    0.0, 0.0, sp->ratio_min, sp->ratio_min, GDK_INTERP_BILINEAR);
            else { /* No expanding, just copy */
                if (sp->copy)
                    g_object_unref(sp->copy);
                sp->copy = gdk_pixbuf_copy(sp->pic);
            }
        }

        sp->former_width = widget->allocation.width;
        sp->former_height = widget->allocation.height;
    }

    cairo_reset_clip(cr);
    if (sp->expand) {
        cairo_rectangle(cr, area->x, area->y, area->width, area->height);
        cairo_clip(cr);
        gdk_cairo_set_source_pixbuf(cr, sp->copy, 0.0, 0.0);
        cairo_paint(cr);
    } else {/* All this stuff is needed to center the scaled image and determine the actual area
             to be drawn */
        gdouble x0, y0, x_i, x_f, w_i, y_i, y_f, h_i, heff, weff;

        if (sp->ratio_min < 1.0) {
            weff = (gdouble)sp->pic_width * sp->ratio_min;
            heff = (gdouble)sp->pic_height * sp->ratio_min;
        } else {
            weff = sp->pic_width;
            heff = sp->pic_height;
        }
        x0 = MAX(0, (widget->allocation.width - weff) / 2);
        y0 = MAX(0, (widget->allocation.height - heff) / 2);
        x_i = MAX(x0, area->x);
        x_f = MIN(x0 + sp->pic_width, area->x + area->width);
        w_i = x_f - x_i;
        y_i = MAX(y0, area->y);
        y_f = MIN(y0 + sp->pic_height, area->y + area->height);
        h_i = y_f - y_i;

        di_draw_rectangle(custom_drawing_get_drawable(CUSTOM_DRAWING(widget)),
            sp->bg_gc, TRUE, area->x, area->y, area->width, area->height);
        if (w_i > 0 && h_i > 0) {
            cairo_rectangle(cr, x_i, y_i, w_i, h_i);
            cairo_clip(cr);
            gdk_cairo_set_source_pixbuf(cr, sp->copy,
                ((gdouble)widget->allocation.width - weff) * 0.5,
                ((gdouble)widget->allocation.height - heff) * 0.5);
            cairo_paint(cr);
        }
    }
    cairo_destroy(cr);
}

static void scalable_pic_size_request(GtkWidget* widget,
    GtkRequisition* requisition)
{
    g_return_if_fail(widget != NULL);
    g_return_if_fail(IS_SCALABLE_PIC(widget));

    /* widget then adjust its dimensions */
    requisition->width = 0;
    requisition->height = 0;
}

static void scalable_pic_size_allocate(GtkWidget* widget,
    GtkAllocation* allocation)
{
    g_return_if_fail(widget != NULL);
    g_return_if_fail(IS_SCALABLE_PIC(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation.x = allocation->x;
    widget->allocation.y = allocation->y;
    widget->allocation.width = allocation->width;
    widget->allocation.height = allocation->height;

    if (gtk_widget_get_realized(widget)) {
        gdk_window_move_resize(widget->window, allocation->x, allocation->y,
            allocation->width, allocation->height);
        scalable_pic_send_configure(SCALABLE_PIC(widget));
    }

    (*GTK_WIDGET_CLASS(scalable_pic_parent_class)->size_allocate)(widget, allocation);
}

static gint scalable_pic_expose(GtkWidget* widget, GdkEventExpose* event)
{
    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(IS_SCALABLE_PIC(widget), FALSE);

    scalable_pic_draw(widget, &event->area);

    return (*GTK_WIDGET_CLASS(scalable_pic_parent_class)->expose_event)(widget, event);
}

static void scalable_pic_send_configure(ScalablePic* sp)
{
    GtkWidget* widget;
    GdkEventConfigure event;

    widget = GTK_WIDGET(sp);

    event.type = GDK_CONFIGURE;
    event.window = widget->window;
    event.x = widget->allocation.x;
    event.y = widget->allocation.y;
    event.width = widget->allocation.width;
    event.height = widget->allocation.height;

    gtk_widget_event(widget, (GdkEvent*)&event);
}
