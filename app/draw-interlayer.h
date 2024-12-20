
/*
 * The Real SoundTracker - Intermediate layer for drawing backends
 * abstraction (header)
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

#ifndef __DRAW_H
#define __DRAW_H

#include <gtk/gtk.h>

#include <config.h>

#ifdef BACKEND_X11

#   include <gdk/gdkx.h>
#   include <X11/Xlib.h>

#define DOUBLE_BUFFERING_NEEDED 1

typedef XID DIDrawable;
typedef GC DIGC;
typedef XSegment DISegment;

extern Display* display;

static inline void di_widget_configure(GtkWidget* widget)
{
    gtk_widget_set_double_buffered(widget, FALSE);
}

static inline void di_gc_set_exposures(DIGC gc, const gboolean value)
{
    XGCValues vls;

    vls.graphics_exposures = value;
    XChangeGC(display, gc, GCGraphicsExposures, &vls);
}

static inline void di_draw_rectangle(DIDrawable drawable, DIGC gc,
    gboolean filled, gint x, gint y, gint width, gint height)
{
    (filled ? XFillRectangle : XDrawRectangle)(display, drawable, gc, x, y, width, height);
}

static inline void di_draw_arc(DIDrawable drawable, DIGC gc,
    gboolean filled, gint x, gint y, gint width, gint height, gint angle1, gint angle2)
{
    (filled ? XFillArc : XDrawArc)(display, drawable, gc, x, y, width, height, angle1, angle2);
}

static inline DIDrawable di_get_drawable(GdkDrawable* win)
{
    return gdk_x11_drawable_get_xid(win);
}

DIGC di_gc_new(GdkWindow* window);
void di_init_display(GtkWidget* w);

#   define di_draw_line(drawable, gc, x1, y1, x2, y2)\
    XDrawLine(display, drawable, gc, x1, y1, x2, y2)
#   define di_draw_drawable(dest, gc, src, x1, y1, x2, y2, width, height)\
    XCopyArea(display, src, dest, gc, x1, y1, width, height, x2, y2)
#   define di_draw_segments(dest, gc, segs, nsegs)\
    XDrawSegments(display, dest, gc, segs, nsegs)

#else /* BACKEND_X11 */

typedef GdkDrawable *DIDrawable;
typedef GdkGC *DIGC;
typedef GdkSegment DISegment;

#   define di_init_display(x) {}
#   define di_widget_configure(x) {}
#   define di_gc_set_exposures(gc, value) gdk_gc_set_exposures(gc, value)

#   define di_draw_rectangle(drawable, gc, filled, x, y, width, height)\
    gdk_draw_rectangle(drawable, gc, filled, x, y, width, height)
#   define di_draw_line(drawable, gc, x1, y1, x2, y2)\
    gdk_draw_line(drawable, gc, x1, y1, x2, y2)
#   define di_draw_arc(drawable, gc, filled, x, y, width, height, angle1, angle2)\
    gdk_draw_arc(drawable, gc, filled, x, y, width, height, angle1, angle2)
#   define di_draw_drawable(dest, gc, src, x1, y1, x2, y2, width, height)\
    gdk_draw_drawable(dest, gc, src, x1, y1, x2, y2, width, height)
#   define di_draw_segments(dest, gc, segs, nsegs)\
    gdk_draw_segments(dest, gc, segs, nsegs)

#   define di_get_drawable(x) x
#   define di_gc_new(window) gdk_gc_new(window)

#endif /* !BACKEND_X11 */

typedef struct _DILayout DILayout;

void di_update_mask(GdkWindow* window, GdkPixmap* pixmap, DIGC mask_gc,
    const gint x, const gint y, const gint width, const gint height,
    const gint window_width, const gint window_height);
DILayout* di_layout_new(GtkWidget *widget);
void di_layout_set_drawable(DILayout* layout, GdkDrawable* dest);
void di_draw_text(DILayout* layout, GdkColor* color,
    const char* text, const gint len,  const gint x, const gint y);
gboolean di_layout_set_font(DILayout* layout, const gchar* fname);
void di_layout_get_pixel_size(DILayout* layout, const gchar* text,
    const gint len, gint* x, gint* y);

#endif /* __DRAW_H */
