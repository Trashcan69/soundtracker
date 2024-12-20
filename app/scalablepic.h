
/*
 * The Real SoundTracker - automatically scalable widget containing
 * an image (header)
 *
 * Copyright (C) 2002 Yury Aliaev
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

#ifndef __SCALABLE_PIC_H
#define __SCALABLE_PIC_H

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "customdrawing.h"

#define SCALABLE_PIC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST(obj, scalable_pic_get_type(), ScalablePic))
#define SCALABLE_PIC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST(klass, scalable_pic_get_type(), ScalablePicClass))
#define IS_SCALABLE_PIC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE(obj, scalable_pic_get_type()))

typedef struct _ScalablePic {
    CustomDrawing widget;
    GdkPixbuf *pic, *copy;
    DIGC bg_gc;
    gint maxwidth, maxheight, former_width, former_height, pic_width, pic_height;
    gboolean expand;
    gdouble ratio_min;
} ScalablePic;

typedef struct _ScalablePicClass {
    CustomDrawingClass parent_class;
} ScalablePicClass;

/* create a widget containing GdkPixbuf image */
GType scalable_pic_get_type(void) G_GNUC_CONST;
GtkWidget* scalable_pic_new(GdkPixbuf* pic, gboolean expand);
void scalable_pic_set_bg(ScalablePic* w, DIGC gc);

#endif /* __SCALABLE_PIC_H */
