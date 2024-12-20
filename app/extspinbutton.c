
/*
 * The Real SoundTracker - GTK+ Spinbutton extensions
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

#include <math.h>

#include "extspinbutton.h"
#include "gui-subs.h"

/* These two defines have been taken from gtkspinbutton.c in gtk+-1.2.1
   Unfortunately there's no cleaner solution */

#define MIN_SPIN_BUTTON_WIDTH 30
#define ARROW_SIZE 11

G_DEFINE_TYPE(ExtSpinButton, extspinbutton, GTK_TYPE_SPIN_BUTTON)

static int
extspinbutton_find_display_digits(GtkAdjustment* adjustment)
{
    int num_digits;

    num_digits = adjustment->lower < 0 || adjustment->upper < 0;
    num_digits += ceil(log10(MAX(1000, MAX(ABS(adjustment->lower), ABS(adjustment->upper)))));

    return num_digits;
}

static void
extspinbutton_style_set(GtkWidget* w)
{
    gint height;

    ExtSpinButton* s = EXTSPINBUTTON(w);

    gui_get_pixel_size(w, "X", &s->width, &height);
}

static void
extspinbutton_size_request(GtkWidget* widget,
    GtkRequisition* requisition)
{
    g_return_if_fail(widget != NULL);
    g_return_if_fail(requisition != NULL);
    g_return_if_fail(GTK_IS_SPIN_BUTTON(widget));

    GTK_WIDGET_CLASS(extspinbutton_parent_class)->size_request(widget, requisition);

    if (EXTSPINBUTTON(widget)->size_hack) {
        if (EXTSPINBUTTON(widget)->width == -1)
            extspinbutton_style_set(widget);
        requisition->width = MAX(MIN_SPIN_BUTTON_WIDTH,
                                 extspinbutton_find_display_digits(GTK_SPIN_BUTTON(widget)->adjustment)
                                     * EXTSPINBUTTON(widget)->width)
            + ARROW_SIZE + 2 * widget->style->xthickness;
    } else {
        // This is the normal size_request() from gtk+-1.2.8
        requisition->width = MIN_SPIN_BUTTON_WIDTH + ARROW_SIZE
            + 2 * widget->style->xthickness;
    }
}

static void
extspinbutton_value_changed(ExtSpinButton* spin)
{
    if (spin->parent_window)
        gtk_window_set_focus(GTK_WINDOW(spin->parent_window), NULL);
}

static void
extspinbutton_map(GtkWidget* w)
{
    ExtSpinButton* s;
    GtkWidget* p;

    g_assert(IS_EXTSPINBUTTON(w));
    s = EXTSPINBUTTON(w);
    if (s->unset_focus)
        for(p = gtk_widget_get_parent(w); p; p = gtk_widget_get_parent(p))
            if (GTK_IS_WINDOW(p)) {
                s->parent_window = p;
                break;
            }

    (*GTK_WIDGET_CLASS(extspinbutton_parent_class)->map)(w);
}

GtkWidget*
extspinbutton_new(GtkAdjustment* adjustment,
    gfloat climb_rate,
    guint digits,
    gboolean unset_focus)
{
    ExtSpinButton* s;

    s = g_object_new(extspinbutton_get_type(), NULL);
    s->size_hack = TRUE;
    s->unset_focus = unset_focus;
    s->parent_window = NULL;
    s->width = -1;
    gtk_spin_button_configure(GTK_SPIN_BUTTON(s), adjustment, climb_rate, digits);

    g_signal_connect(s, "style-set",
        G_CALLBACK(extspinbutton_style_set), NULL);
    if (unset_focus)
        g_signal_connect(s, "value-changed",
            G_CALLBACK(extspinbutton_value_changed), NULL);

    return GTK_WIDGET(s);
}

void extspinbutton_disable_size_hack(ExtSpinButton* b)
{
    b->size_hack = FALSE;
}

static void
extspinbutton_init(ExtSpinButton* s)
{
}

static void
extspinbutton_class_init(ExtSpinButtonClass* class)
{
    GtkWidgetClass* widget_class;

    widget_class = (GtkWidgetClass*)class;
    widget_class->size_request = extspinbutton_size_request;
    widget_class->map = extspinbutton_map;
}
