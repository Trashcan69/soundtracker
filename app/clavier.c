/* clavier.c - GTK+ "Clavier" Widget -- based on clavier-0.1.3
 * Copyright (C) 1998 Simon Kågedal
 * Copyright (C) 1999-2019 Michael Krause
 * Copyright (C) 2020-2021 Yury Aliaev
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License in the file COPYING for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * created 1998-04-18 Simon Kågedal
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "clavier.h"
#include "colors.h"
#include "marshal.h"

/* Signals */
enum {
    CLAVIERKEY_PRESS,
    CLAVIERKEY_RELEASE,
    CLAVIERKEY_ENTER,
    CLAVIERKEY_LEAVE,
    LAST_SIGNAL
};

struct _ClavierKeyInfo {
    gboolean is_black; /* is it a black key */

    gint upper_right_x;
    gint lower_right_x; /* not valid if is_black==TRUE */
    gint width; /* = */
};

typedef struct _ClavierKeyInfo ClavierKeyInfo;

typedef struct {
    DILayout *layout;
    DIGC white_gc, black_gc, white_p_gc, black_p_gc, white_tp_gc, black_tp_gc;
    int fonth, fontw;
    const gchar* font;

    gint8* keylabels; /* Pointer to bytes, not characters... */
    gboolean pressed_map[128], prev_map[128];

    gint key_start; /* key number start. like MIDI, middle C = 60 */
    gint key_end; /* key number end. */
    gfloat black_key_height; /* percentage of the whole height */
    gfloat black_key_width;
    gint prev_width;

    gboolean is_pressed; /* is a key pressed? */
    gint key_pressed; /* which key? */
    gint button; /* by which mouse button? */

    int key_entered; /* the key the mouse is over currently (or -1) */

    /* stuff that is calculated */

    gfloat keywidth; /* X */

    ClavierKeyInfo* key_info;
    gint key_info_size;
} ClavierPriv;

struct _Clavier {
    CustomDrawing widget;

    ClavierPriv* priv;
};

static guint clavier_signals[LAST_SIGNAL] = { 0 };

typedef void (*ClavierSignal1)(GtkObject* object,
    gint arg1,
    gpointer data);

/* Clavier Methods */
static void clavier_class_init(ClavierClass* class);
static void clavier_init(Clavier* clavier);

/* GtkObject Methods */
static void clavier_destroy(GtkObject* object);

/* GtkWidget Methods */
static void clavier_realize(GtkWidget* widget);
static gboolean clavier_expose(GtkWidget* widget, GdkEventExpose* event);
static void clavier_size_allocate(GtkWidget* widget, GtkAllocation* allocation);
static gboolean clavier_button_press(GtkWidget* widget, GdkEventButton* event);
static gboolean clavier_button_release(GtkWidget* widget, GdkEventButton* event);
static gboolean clavier_motion_notify(GtkWidget* widget, GdkEventMotion* event);
static gboolean clavier_leave_notify(GtkWidget* widget, GdkEventCrossing* event);

G_DEFINE_TYPE(Clavier, clavier, custom_drawing_get_type())

static void
clavier_class_init(ClavierClass* class)
{
    GObjectClass* object_class;
    GtkObjectClass* gtkobject_class;
    GtkWidgetClass* widget_class;

    object_class = (GObjectClass*)class;
    gtkobject_class = (GtkObjectClass*)class;
    widget_class = (GtkWidgetClass*)class;

    clavier_signals[CLAVIERKEY_PRESS] = g_signal_new("clavierkey_press", G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(ClavierClass, clavierkey_press),
        NULL, NULL,
        __marshal_VOID__INT_INT, G_TYPE_NONE, 2,
        G_TYPE_INT, G_TYPE_INT);

    clavier_signals[CLAVIERKEY_RELEASE] = g_signal_new("clavierkey_release", G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(ClavierClass, clavierkey_release),
        NULL, NULL,
        __marshal_VOID__INT_INT, G_TYPE_NONE, 2,
        G_TYPE_INT, G_TYPE_INT);

    clavier_signals[CLAVIERKEY_ENTER] = g_signal_new("clavierkey_enter", G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(ClavierClass, clavierkey_enter),
        NULL, NULL,
        g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1,
        G_TYPE_INT);

    clavier_signals[CLAVIERKEY_LEAVE] = g_signal_new("clavierkey_leave", G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)G_SIGNAL_RUN_FIRST,
        G_STRUCT_OFFSET(ClavierClass, clavierkey_leave),
        NULL, NULL,
        g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1,
        G_TYPE_INT);

    gtkobject_class->destroy = clavier_destroy;

    widget_class->realize = clavier_realize;
    widget_class->expose_event = clavier_expose;
    widget_class->size_allocate = clavier_size_allocate;

    widget_class->button_press_event = clavier_button_press;
    widget_class->button_release_event = clavier_button_release;
    widget_class->motion_notify_event = clavier_motion_notify;
    widget_class->leave_notify_event = clavier_leave_notify;
}

static void
clavier_init(Clavier* clavier)
{
    clavier->priv = g_new(ClavierPriv, 1);
    clavier->priv->keylabels = NULL;

    clavier->priv->key_start = 36;
    clavier->priv->key_end = 96;
    clavier->priv->black_key_height = 0.6;
    clavier->priv->black_key_width = 0.55;
    clavier->priv->prev_width = 0;

    clavier->priv->is_pressed = FALSE;
    clavier->priv->key_pressed = 0;

    clavier->priv->key_info = NULL;
    clavier->priv->key_info_size = 0;

    di_widget_configure(GTK_WIDGET(clavier));
}

static void
clavier_destroy(GtkObject* object)
{
    Clavier* clavier;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_CLAVIER(object));

    clavier = CLAVIER(object);

    /* eventually free memory allocated for key info
   */

    if (clavier->priv->key_info) {
        g_free(clavier->priv->key_info);
    }
    g_free(clavier->priv);

    if (GTK_OBJECT_CLASS(clavier_parent_class)->destroy)
        (*GTK_OBJECT_CLASS(clavier_parent_class)->destroy)(object);
}

/* checks if the given key is a black one
 */

static gboolean
is_black_key(gint key)
{
    switch (key % 12) {
    case 1:
    case 3:
    case 6:
    case 8:
    case 10:
        return (TRUE);

    default:
        return (FALSE);
    }
}

/* calculates how many white and black keys there are in a certain
 * interval on the keyboard 
 */

static void
calc_keys(gint start, gint end, gint* whites, gint* blacks)
{
    gint i;

    *blacks = 0;
    *whites = 0;

    if (end >= start) {
        for (i = start; i <= end; i++) {
            if (is_black_key(i))
                (*blacks)++;
        }

        *whites = end - start + 1 - *blacks;
    }
}

/* this draws a key like on a real keyboard, y'know, all white keys have
 * the same width and the black keys are a bit narrower.
 */

static void
calc_key_normal(ClavierPriv* clavier, GtkWidget* widget, gint key, ClavierKeyInfo* key_info)
{
    gint blacks, whites, width;

    width = widget->allocation.width;
    calc_keys(clavier->key_start, key - 1, &whites, &blacks);

    switch (key % 12) {
    case 1:
    case 3:
    case 6:
    case 8:
    case 10:

        /* black key */

        key_info->is_black = TRUE;
        key_info->upper_right_x = ((gfloat)whites + (clavier->black_key_width * 0.5)) * clavier->keywidth;

        break;

    case 0:
    case 2:
    case 5:
    case 7:
    case 9:

        /* the (whites):th white key from left or bottom, with short
       * right side
       */

        key_info->is_black = FALSE;
        key_info->upper_right_x = (whites + 1) * clavier->keywidth - (clavier->black_key_width * 0.5 * clavier->keywidth);
        key_info->lower_right_x = (whites + 1) * clavier->keywidth;

        break;

    case 4:
    case 11:

        /* the (whites):th white key from left or bottom, with long
       * right side
       */

        key_info->is_black = FALSE;
        key_info->upper_right_x = key_info->lower_right_x = (whites + 1) * clavier->keywidth;

        break;
    }

    /* we need to do some fixing for the rightmost key */

    if (key == clavier->key_end) {
        key_info->upper_right_x = key_info->lower_right_x = width - 1;
    }
}

static void
calc_key_info(ClavierPriv* clavier, GtkWidget* widget)
{
    gint keys = clavier->key_end - clavier->key_start + 1;
    gint i, prev = 0, pprev = 0;

    if (!clavier->key_info) {
        clavier->key_info = g_malloc(sizeof(ClavierKeyInfo) * keys);
    } else if (clavier->key_info_size != keys) {
        clavier->key_info = g_realloc(clavier->key_info, sizeof(ClavierKeyInfo) * keys);
    }

    for (i = 0; i < keys; i++) {
        ClavierKeyInfo* ki = &clavier->key_info[i];

        calc_key_normal(clavier, widget,
            clavier->key_start + i, ki);

        if (!ki->is_black) {
            ki->width = ki->lower_right_x - prev;
            prev = ki->lower_right_x;
        } else
            ki->width = ki->upper_right_x - pprev;
        pprev = ki->upper_right_x;

        /* fix so draw_key draws lower rectangle correctly */

        if (i > 0 && (ki->is_black)) {
            clavier->key_info[i].lower_right_x = clavier->key_info[i - 1].lower_right_x;
        }
    }
}

static void
clavier_draw_label(ClavierPriv* clavier, GtkWidget* widget,
    int keynum, gboolean pressed)
{
    if (clavier->keylabels && clavier->layout) {
        ClavierKeyInfo* this = &(clavier->key_info[keynum]);
        gchar buf[4];
        gint8 label = clavier->keylabels[keynum];
        gboolean is_black = clavier->key_info[keynum].is_black;
        gint len;
        const gint black_pos =
            widget->allocation.height * clavier->black_key_height -
            clavier->fonth;
        const gint white_pos =
            MIN(widget->allocation.height * clavier->black_key_height + clavier->fonth,
            widget->allocation.height - clavier->fonth);

        if (label > 0xf) {
            len = sprintf(buf, "%x", label >> 4);
            di_draw_text(clavier->layout,
                colors_get_color(is_black ? (pressed ? COLOR_TEXT_BLACK_PRESSED : COLOR_TEXT_BLACK)
                                          : (pressed ? COLOR_TEXT_WHITE_PRESSED : COLOR_TEXT_WHITE)),
                buf, len, (is_black ? this->upper_right_x : this->lower_right_x)
                    - ((this->width + clavier->fontw) >> 1),
                (is_black ? black_pos : white_pos) - clavier->fonth);
        }
        len = sprintf(buf, "%x", label & 0xf);
        di_draw_text(clavier->layout,
            colors_get_color(is_black ? (pressed ? COLOR_TEXT_BLACK_PRESSED : COLOR_TEXT_BLACK)
                                      : (pressed ? COLOR_TEXT_WHITE_PRESSED : COLOR_TEXT_WHITE)),
            buf, len, (is_black ? this->upper_right_x : this->lower_right_x)
                - ((this->width + clavier->fontw) >> 1),
            is_black ? black_pos : white_pos);
    }
}

static void clavier_size_allocate(GtkWidget* widget,
    GtkAllocation* allocation)
{
    g_return_if_fail(widget != NULL);
    g_return_if_fail(IS_CLAVIER(widget));
    g_return_if_fail(allocation != NULL);

    widget->allocation.x = allocation->x;
    widget->allocation.y = allocation->y;
    widget->allocation.width = allocation->width;
    widget->allocation.height = allocation->height;

    if (gtk_widget_get_realized(widget)) {
        gdk_window_move_resize(widget->window, allocation->x, allocation->y,
            allocation->width, allocation->height);
    }

  (*GTK_WIDGET_CLASS(clavier_parent_class)->size_allocate)(widget, allocation);
}

/* main drawing function
 */

/* Draw a key pressed or unpressed
 */

static GdkRectangle
draw_key(ClavierPriv* clavier, GtkWidget* widget, DIDrawable drw, GdkDrawable* gdrw, gint key)
{
    ClavierKeyInfo first = { 0, 0 }, *prev, *this;
    gint keynum;
    gint height;
    GdkRectangle a;
    const gboolean pressed = clavier->pressed_map[key];
    DIGC gc;

    di_layout_set_drawable(clavier->layout, gdrw);
    height = widget->allocation.height;

    keynum = key - clavier->key_start;
    g_assert(keynum <= clavier->key_end);

    this = &(clavier->key_info[keynum]);
    prev = (keynum > 0) ? &(clavier->key_info[keynum - 1]) : &first;

    gc = this->is_black ? (pressed ? clavier->black_p_gc : clavier->black_gc)
        : (pressed ? clavier->white_p_gc : clavier->white_gc);

    /* draw upper part */

    a.x = prev->upper_right_x + 1;
    a.y = 1;
    a.width = this->upper_right_x - prev->upper_right_x - 1;
    a.height = height * clavier->black_key_height;
    if (clavier->key_info[keynum].is_black)
        a.height -= 1;

    di_draw_rectangle(drw, gc, TRUE, a.x, a.y, a.width, a.height);

    if (!clavier->key_info[keynum].is_black) {
        gint y1, y2;
        /* draw lower part */

        a.x = prev->lower_right_x + 1;
        y1 = height * clavier->black_key_height + 1;
        a.width = this->lower_right_x - prev->lower_right_x - 1;
        y2 = height * (1.0 - clavier->black_key_height);
        a.height += y2;

        di_draw_rectangle(drw, gc, TRUE, a.x, y1, a.width, y2);
    }

    clavier_draw_label(clavier, widget, keynum, pressed);
    clavier->prev_map[key] = pressed;

    return a;
}

static gint
clavier_expose(GtkWidget* widget, GdkEventExpose* event)
{
    ClavierPriv* clavier;
    int i;
    gint keys, whitekeys, blackkeys;
    gint x, y, width, height;
    DIDrawable drw = custom_drawing_get_drawable(CUSTOM_DRAWING(widget));
    GdkDrawable* gdrw = custom_drawing_get_gdk_drawable(CUSTOM_DRAWING(widget));

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(IS_CLAVIER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    clavier = CLAVIER(widget)->priv;
    di_layout_set_drawable(clavier->layout,
        custom_drawing_get_gdk_drawable(CUSTOM_DRAWING(widget)));

    x = event->area.x;
    y = event->area.y;
    width = event->area.width;
    height = event->area.height;

    keys = clavier->key_end - clavier->key_start + 1;

    if (width != 1 || height != 1 || x || y) {
        if (height)
            di_draw_rectangle(drw, clavier->black_gc, TRUE, x, MAX(y, 1),
                width, MIN(height, widget->allocation.height - 1));
        if (!y)
            di_draw_line(drw, clavier->white_gc, x, 0, width, 0);
    }

    /* To speed up a little bit */

    if (clavier->prev_width != widget->allocation.width) {
        clavier->prev_width = widget->allocation.width;
        calc_keys(clavier->key_start, clavier->key_end, &whitekeys, &blackkeys);
        clavier->keywidth = (gfloat)widget->allocation.width / (gfloat)whitekeys;
        calc_key_info(clavier, widget);
    }

    if (width == 1 && height == 1 &&
        (!event->area.x) && (!event->area.y)) {/* Special case, drawing only keys changed */
        for (i = 0; i < keys; i++)
            if (clavier->pressed_map[i] != clavier->prev_map[i]) {
                GdkRectangle a = draw_key(clavier, widget, drw, gdrw, i + clavier->key_start);

                if (!i)
                    event->area = a;
                else
                    gdk_rectangle_union(&event->area, &a, &event->area);
            }
    } else //TODO draw keys only within area
        for (i = 0; i < keys; i++)
            draw_key(clavier, widget, drw, gdrw, i + clavier->key_start);

    return (*GTK_WIDGET_CLASS(clavier_parent_class)->expose_event)(widget, event);
}

/* See which key is drawn at x, y
 */

static gint
which_key(ClavierPriv* clavier, GtkWidget* widget, gint x, gint y)
{
    gint i, keys, height;

    height = widget->allocation.height;

    keys = clavier->key_end - clavier->key_start + 1;

    if (y > (height * clavier->black_key_height)) {
        /* check lower part */

        for (i = 0; i < keys; i++) {
            if (x < clavier->key_info[i].lower_right_x) {
                return clavier->key_start + i;
            }
        }
    } else {
        /* check upper part */

        for (i = 0; i < keys; i++) {
            if (x < clavier->key_info[i].upper_right_x) {
                return (clavier->key_start + i);
            }
        }
    }

    /* so it must be the rightmost key */

    return (clavier->key_end);
}

static void
press_key(Clavier* clavier, gint key, gint button)
{
    clavier->priv->is_pressed = TRUE;
    clavier->priv->key_pressed = key;
    clavier->priv->button = button;

    g_signal_emit(G_OBJECT(clavier), clavier_signals[CLAVIERKEY_PRESS], 0, key, button);

    /*  printf("press: %i\n", key); */
}

static void
release_key(Clavier* clavier, gint button)
{
    clavier->priv->is_pressed = FALSE;

    g_signal_emit(G_OBJECT(clavier), clavier_signals[CLAVIERKEY_RELEASE], 0,
        clavier->priv->key_pressed, button);

    /*  printf ("release: %i\n", clavier->key_pressed); */
}

/* events
 */

static gint
clavier_button_press(GtkWidget* widget, GdkEventButton* event)
{
    Clavier* clavier;
    gint key;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(IS_CLAVIER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    clavier = CLAVIER(widget);

    key = which_key(clavier->priv, widget, (gint)event->x, (gint)event->y);

    press_key(clavier, key, event->button);

    /* add pointer grab - hmm, doesn't seem to work the way i'd like :( */

    gtk_grab_add(widget);

    return FALSE;
}

static gint
clavier_button_release(GtkWidget* widget, GdkEventButton* event)
{
    Clavier* clavier;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(IS_CLAVIER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    clavier = CLAVIER(widget);

    release_key(clavier, event->button);

    gtk_grab_remove(widget);

    return FALSE;
}

static gint
clavier_motion_notify(GtkWidget* widget, GdkEventMotion* event)
{
    Clavier* clavier;
    ClavierPriv* priv;
    gint key;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(IS_CLAVIER(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    clavier = CLAVIER(widget);
    priv = clavier->priv;

    key = which_key(priv, widget, (gint)event->x, (gint)event->y);

    if (key != priv->key_entered) {
        if (priv->key_entered != -1) {
            g_signal_emit(G_OBJECT(clavier),
                clavier_signals[CLAVIERKEY_LEAVE], 0, priv->key_entered);
        }
        priv->key_entered = key;
        g_signal_emit(G_OBJECT(clavier),
            clavier_signals[CLAVIERKEY_ENTER], 0, key);
    }

    if (priv->is_pressed) {
        if (key != priv->key_pressed) {
            release_key(clavier, priv->button);
            press_key(clavier, key, priv->button);
        }
    }

    return FALSE;
}

static gint
clavier_leave_notify(GtkWidget* widget,
    GdkEventCrossing* event)
{
    Clavier* clavier = CLAVIER(widget);

    if (clavier->priv->key_entered != -1) {
        g_signal_emit(G_OBJECT(clavier),
            clavier_signals[CLAVIERKEY_LEAVE], 0, clavier->priv->key_entered);
        clavier->priv->key_entered = -1;
    }

    return FALSE;
}

GtkWidget*
clavier_new(const gchar* font)
{
    Clavier* clavier;

    clavier = g_object_new(clavier_get_type(), NULL);
    clavier->priv->font = font;
    clavier->priv->layout = NULL;
    memset(clavier->priv->pressed_map, 0, sizeof(clavier->priv->pressed_map));
    memset(clavier->priv->prev_map, 0, sizeof(clavier->priv->prev_map));

    return GTK_WIDGET(clavier);
}

static inline void
clavier_update_layout(ClavierPriv* clavier)
{
    if (!clavier->layout)
        return;
    di_layout_set_font(clavier->layout, clavier->font);
    /* let's just hope this is a non-proportional font */
    di_layout_get_pixel_size(clavier->layout, "0", -1, &clavier->fontw, &clavier->fonth);
}

static void
clavier_realize(GtkWidget* widget)
{
    GdkWindowAttr attributes;
    ClavierPriv* clavier;
    gint attributes_mask;

    g_return_if_fail(widget != NULL);
    g_return_if_fail(IS_CLAVIER(widget));

    clavier = CLAVIER(widget)->priv;
    gtk_widget_set_realized(widget, TRUE);

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);
    attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK;

    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

    widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
        &attributes, attributes_mask);
    gdk_window_set_user_data(widget->window, widget);

    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    clavier->white_gc = colors_get_gc(COLOR_KEY_WHITE);
    colors_add_widget(COLOR_KEY_WHITE, widget);
    clavier->black_gc = colors_get_gc(COLOR_KEY_BLACK);
    clavier->white_p_gc = colors_get_gc(COLOR_KEY_WHITE_PRESSED);
    clavier->black_p_gc = colors_get_gc(COLOR_KEY_BLACK_PRESSED);
    clavier->white_tp_gc = colors_get_gc(COLOR_TEXT_WHITE_PRESSED);
    clavier->black_tp_gc = colors_get_gc(COLOR_TEXT_BLACK_PRESSED);

    clavier->layout = di_layout_new(widget);
    clavier_update_layout(clavier);
    (*GTK_WIDGET_CLASS(clavier_parent_class)->realize)(widget);
}

/* these routines should most often be mapped to the press and release
 * signals, but not all applications would want it that way
 */

void clavier_press(Clavier* clavier, gint key)
{
    /* TODO: should keep track of what keys are pressed so that the
   * whole widget can be correctly redrawn
   */

    if (key < clavier->priv->key_start || key > clavier->priv->key_end)
        return;

    clavier->priv->pressed_map[key] = TRUE;
    /* width == height == 1 indicates a special case
       of partial drawing */
    gtk_widget_queue_draw_area(GTK_WIDGET(clavier), 0, 0, 1, 1);
}

void clavier_release(Clavier* clavier, gint key)
{
    if (key == -1) /* All keys */
        memset(clavier->priv->pressed_map, 0, sizeof(clavier->priv->pressed_map));
    else {
        if (key < clavier->priv->key_start || key > clavier->priv->key_end)
            return;

        clavier->priv->pressed_map[key] = FALSE;
    }
    gtk_widget_queue_draw_area(GTK_WIDGET(clavier), 0, 0, 1, 1);
}

/* attribute setting 
 */

void clavier_set_range(Clavier* clavier, gint start, gint end)
{
    g_return_if_fail(start >= 0 && start <= 127);
    g_return_if_fail(end >= start && end <= 127);

    clavier->priv->key_start = start;
    clavier->priv->key_end = end;
}

void clavier_set_key_labels(Clavier* clavier,
    gint8* labels)
{
    clavier->priv->keylabels = labels;
    gtk_widget_queue_draw(GTK_WIDGET(clavier));
}

void clavier_set_font(Clavier* clavier,
    const gchar* font)
{
    clavier->priv->font = font;
    clavier_update_layout(clavier->priv);
    gtk_widget_queue_draw(GTK_WIDGET(clavier));
}
