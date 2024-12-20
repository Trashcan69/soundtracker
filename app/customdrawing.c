
/*
 * The Real SoundTracker - Base class for widgets using custom drawing
 * procedures
 *
 * Copyright (C) 2019 Yury Aliaev
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

#include "customdrawing.h"

static void custom_drawing_class_init(CustomDrawingClass* klass);
static void custom_drawing_init(CustomDrawing* sp);
static gint custom_drawing_expose(GtkWidget* widget, GdkEventExpose* event);
static void custom_drawing_realize(GtkWidget* widget);
static void custom_drawing_style_set(GtkWidget* widget, GtkStyle* prev);
#ifdef DOUBLE_BUFFERING_NEEDED
static void custom_drawing_size_allocate(GtkWidget* widget, GtkAllocation* allocation);
#endif

G_DEFINE_TYPE(CustomDrawing, custom_drawing, GTK_TYPE_WIDGET)

void
custom_drawing_register_drawing_func(CustomDrawing* cd,
    void (*drawing_func)(GtkWidget*, GdkRectangle*, gpointer),
    gpointer drawing_data)
{
    cd->drawing_func = drawing_func;
    cd->drawing_data = drawing_data;
}

void
custom_drawing_register_realize_func(CustomDrawing* cd,
    void (*realize_func)(GtkWidget*, gpointer),
    gpointer drawing_data)
{
    cd->realize_func = realize_func;
}

void
custom_drawing_register_style_set_func(CustomDrawing* cd,
    void (*style_set_func)(GtkWidget*, gpointer),
    gpointer drawing_data)
{
    cd->style_set_func = style_set_func;
}

static gint
custom_drawing_expose(GtkWidget* widget, GdkEventExpose* event)
{
    CustomDrawing* cd;
#ifdef DOUBLE_BUFFERING_NEEDED
    DIDrawable drw;
#endif

    g_return_val_if_fail(IS_CUSTOM_DRAWING(widget), FALSE);
    cd = CUSTOM_DRAWING(widget);

    if (cd->drawing_func)
        cd->drawing_func(widget, &event->area, cd->drawing_data);

#ifdef DOUBLE_BUFFERING_NEEDED
    drw = di_get_drawable(widget->window);
    if (!cd->gc)
        cd->gc = di_gc_new(widget->window);
    di_draw_drawable(drw, cd->gc, cd->drawable,
        event->area.x, event->area.y, event->area.x, event->area.y,
        event->area.width, event->area.height);
#endif

    return TRUE;
}

static void
custom_drawing_realize(GtkWidget* widget)
{
    CustomDrawing* cd;

    g_return_if_fail(IS_CUSTOM_DRAWING(widget));
    cd = CUSTOM_DRAWING(widget);

#ifdef DOUBLE_BUFFERING_NEEDED
    if (cd->pixmap)
        g_object_unref(cd->pixmap);
    cd->pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
    cd->drawable = di_get_drawable(cd->pixmap);
#endif

    if (cd->realize_func)
        cd->realize_func(widget, cd->drawing_data);
}

static void
custom_drawing_style_set(GtkWidget* widget, GtkStyle* prev)
{
    CustomDrawing* cd;

    g_return_if_fail(IS_CUSTOM_DRAWING(widget));
    cd = CUSTOM_DRAWING(widget);

    if (cd->style_set_func)
        cd->style_set_func(widget, cd->drawing_data);

    if (GTK_WIDGET_CLASS(custom_drawing_parent_class)->style_set)
        (*GTK_WIDGET_CLASS(custom_drawing_parent_class)->style_set)(widget, prev);
}

#ifdef DOUBLE_BUFFERING_NEEDED
static void
custom_drawing_size_allocate(GtkWidget* widget, GtkAllocation* allocation)
{
    CustomDrawing* cd;

    g_return_if_fail(IS_CUSTOM_DRAWING(widget));
    cd = CUSTOM_DRAWING(widget);

    if (cd->pixmap)
        g_object_unref(cd->pixmap);
    if (widget->window) {
        cd->pixmap = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);
        cd->drawable = di_get_drawable(cd->pixmap);
    }
}
#endif

static void
custom_drawing_init(CustomDrawing* cd)
{
    cd->drawing_func = NULL;
    cd->realize_func = NULL;
    cd->style_set_func = NULL;
#ifdef DOUBLE_BUFFERING_NEEDED
    cd->pixmap = NULL;
    cd->gc = NULL;
#endif
}

static void
custom_drawing_class_init(CustomDrawingClass* klass)
{
    GtkWidgetClass* widget_class;

    widget_class = (GtkWidgetClass*)klass;

    widget_class->realize = custom_drawing_realize;
    widget_class->expose_event = custom_drawing_expose;
    widget_class->style_set = custom_drawing_style_set;
#ifdef DOUBLE_BUFFERING_NEEDED
    widget_class->size_allocate = custom_drawing_size_allocate;
#endif
}
