
/*
 * The Real SoundTracker - GTK+ Spinbutton extensions (header)
 *
 * Copyright (C) 1999-2019 Michael Krause
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

#ifndef _EXTSPINBUTTON_H
#define _EXTSPINBUTTON_H

#include <gtk/gtkspinbutton.h>

#define EXTSPINBUTTON(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, extspinbutton_get_type(), ExtSpinButton))
#define EXTSPINBUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST(klass, extspinbutton_get_type(), ExtSpinButtonClass))
#define IS_EXTSPINBUTTON(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, extspinbutton_get_type()))

typedef struct _ExtSpinButton ExtSpinButton;
typedef struct _ExtSpinButtonClass ExtSpinButtonClass;

struct _ExtSpinButton {
    GtkSpinButton spin;

    GtkWidget* parent_window;

    gboolean size_hack, unset_focus;
    gint width;
};

struct _ExtSpinButtonClass {
    GtkSpinButtonClass parent_class;
};

GType extspinbutton_get_type(void) G_GNUC_CONST;
GtkWidget* extspinbutton_new(GtkAdjustment* adjustment,
    gfloat climb_rate,
    guint digits,
    gboolean unset_focus);
void extspinbutton_disable_size_hack(ExtSpinButton* b);

#endif /* _EXTSPINBUTTON_H */
