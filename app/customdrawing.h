
/*
 * The Real SoundTracker - Base class for widgets using custom drawing
 * procedures (header)
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

#ifndef __CUSTOM_DRAWING_H
#define __CUSTOM_DRAWING_H

#include <gtk/gtkwidget.h>

#include "draw-interlayer.h"

#define CUSTOM_DRAWING(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST(obj, custom_drawing_get_type(), CustomDrawing))
#define CUSTOM_DRAWING_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass, custom_drawing_get_type(), CustomDrawingClass))
#define IS_CUSTOM_DRAWING(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE(obj, custom_drawing_get_type()))

typedef struct _CustomDrawing {
    GtkWidget widget;

    void (*drawing_func)(GtkWidget*, GdkRectangle*, gpointer);
    void (*realize_func)(GtkWidget*, gpointer);
    void (*style_set_func)(GtkWidget*, gpointer);

    gpointer drawing_data;
#ifdef DOUBLE_BUFFERING_NEEDED
    GdkPixmap *pixmap;
    DIGC gc;
    DIDrawable drawable;
#endif
} CustomDrawing;

typedef struct _CustomDrawingClass {
    GtkWidgetClass parent_class;
} CustomDrawingClass;

GType custom_drawing_get_type(void) G_GNUC_CONST;
void custom_drawing_register_drawing_func(CustomDrawing* cd,
    void (*drawing_func)(GtkWidget* widget, GdkRectangle* area, gpointer drawing_data),
    gpointer drawing_data);
void custom_drawing_register_realize_func(CustomDrawing* cd,
    void (*drawing_func)(GtkWidget* widget, gpointer drawing_data),
    gpointer drawing_data);
void custom_drawing_register_style_set_func(CustomDrawing* cd,
    void (*drawing_func)(GtkWidget* widget, gpointer drawing_data),
    gpointer drawing_data);

static inline DIDrawable custom_drawing_get_drawable(CustomDrawing* cd)
{
#ifdef DOUBLE_BUFFERING_NEEDED
    return cd->drawable;
#else
    return di_get_drawable(GTK_WIDGET(cd)->window);
#endif
}

static inline GdkDrawable* custom_drawing_get_gdk_drawable(CustomDrawing* cd)
{
#ifdef DOUBLE_BUFFERING_NEEDED
    return cd->pixmap;
#else
    return GTK_WIDGET(cd)->window;
#endif
}

#endif /* __CUSTOM_DRAWING_H */
