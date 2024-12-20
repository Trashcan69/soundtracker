
/*
 * The Real SoundTracker - Intermediate layer for drawing backends
 * abstraction
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

#include "draw-interlayer.h"

struct _DILayout {
    PangoLayout* pango;
    cairo_t* cr;
};

#ifdef BACKEND_X11
Display* display;

DIGC
di_gc_new(GdkWindow* window)
{
    XGCValues vls;

    return XCreateGC(display, di_get_drawable(window), 0, &vls);
}

void
di_init_display(GtkWidget* w)
{
    g_assert(gtk_widget_get_realized(w));

    display = gdk_x11_drawable_get_xdisplay(w->window);
}
#endif

void
di_update_mask(GdkWindow* window, GdkPixmap* pm, DIGC mask_gc,
    const gint x0, const gint y0, const gint pm_width, const gint pm_height,
    const gint width, const gint height)
{
    GdkPixbuf *pb, *pba;
    GdkBitmap *mask;

    pb = gdk_pixbuf_get_from_drawable(NULL, pm, gdk_colormap_get_system(),
        0, 0, 0, 0, pm_width, pm_height);
    pba = gdk_pixbuf_add_alpha(pb, TRUE, 0, 0, 0);
    mask = gdk_pixmap_new(window, width, height, 1);
    gdk_pixbuf_render_threshold_alpha(pba, mask, 0, 0, x0, y0, pm_width, pm_height, 254);
#ifdef BACKEND_X11
    XSetClipMask(display, mask_gc, di_get_drawable(mask));
#else
    gdk_gc_set_clip_mask(mask_gc, mask);
#endif

    g_object_unref(pb);
    g_object_unref(pba);
    g_object_unref(mask);
}

DILayout*
di_layout_new(GtkWidget *widget)
{
    PangoContext* context;
    DILayout* layout = g_new(DILayout, 1);

    context = gtk_widget_create_pango_context(widget);
    layout->pango = pango_layout_new(context);
    g_object_unref(context);
    layout->cr = NULL;

    return layout;
}

void
di_layout_set_drawable(DILayout* layout, GdkDrawable* dest)
{
    if (layout->cr)
        cairo_destroy(layout->cr);

    layout->cr = gdk_cairo_create(dest);
}

void
di_draw_text(DILayout* layout, GdkColor* color,
    const char* text, const gint len,  const gint x, const gint y)
{
    cairo_save(layout->cr);
    pango_layout_set_text(layout->pango, text, len);
    gdk_cairo_set_source_color(layout->cr, color);
    cairo_move_to(layout->cr, x, y);
    pango_cairo_update_layout(layout->cr, layout->pango);
    pango_cairo_show_layout(layout->cr, layout->pango);
    cairo_restore(layout->cr);
}

gboolean
di_layout_set_font(DILayout* layout, const gchar* fname)
{
    PangoFontDescription* desc = pango_font_description_from_string(fname);

    if (!desc) {
        /* this never seems to be reached... */
        g_warning("can't get font description from string %s", fname);
        return FALSE;
    }

    pango_layout_set_font_description(layout->pango, desc);
    pango_font_description_free(desc);

    return TRUE;
}

void
di_layout_get_pixel_size(DILayout* layout, const gchar* text,
    const gint len, gint* x, gint* y)
{
    pango_layout_set_text(layout->pango, text, len);
    pango_layout_get_pixel_size(layout->pango, x, y);
}
