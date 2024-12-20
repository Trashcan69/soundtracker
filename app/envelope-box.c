
/*
 * The Real SoundTracker - GTK+ envelope editor box
 *
 * Copyright (C) 1998-2019 Michael Krause
 * Copyright (C) 2019-2023 Yury Aliaev
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

#include <math.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef USE_CANVAS
#include <goocanvas.h>
#endif

#include "envelope-box.h"
#include "extspinbutton.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "gui.h"
#include "history.h"
#include "st-subs.h"
#include "xm.h"

#define NUM_OBJS 10 /* Number of spin buttons */

#define spin_length objs[0]
#define spin_pos objs[1]
#define spin_offset objs[2]
#define spin_value objs[3]
#define spin_sustain objs[4]
#define spin_loop_start objs[5]
#define spin_loop_end objs[6]
#define enable objs[7]
#define sustain objs[8]
#define loop objs[9]

struct _EnvelopeBoxPrivate {
    STEnvelope* current;

    GtkWidget* objs[NUM_OBJS];
    gint sig_tags[NUM_OBJS];

    gfloat scale_x, scale_y;
    gint off_x, off_y;

#ifdef USE_CANVAS
    GooCanvas* canvas;
    GooCanvasItem* group;
    GooCanvasItem *points[ST_MAX_ENVELOPE_POINTS], *cur_point;
    GooCanvasItem *lines[ST_MAX_ENVELOPE_POINTS - 1], *sustain_line, *loop_start_line, *loop_end_line;
    GtkAdjustment *hadj, *vadj;

    int canvas_max_x;
    gdouble zoomfactor_base;
    gdouble zoomfactor_mult;

    gdouble dragging_item_from_x, dragging_item_from_y; /* world coordinates */
    gdouble dragfromx, dragfromy, origin_x, origin_y; /* screen pixel coordinates */
    gdouble zooming_canvas_from_val;

    int dragpoint;
    gboolean dragging_canvas;
    gboolean zooming_canvas;
    guint prev_current;
#endif
};

enum {
    SIG_COPY_ENV,
    SIG_PASTE_ENV,
    LAST_SIGNAL
};

static guint envelope_box_signals[LAST_SIGNAL] = { 0 };

static STEnvelope dummy_envelope = {
    { { 0, 0 } },
    1,
    0,
    0,
    0,
    0
};

static GtkWidget *scale_dialog = NULL, *x_spin, *y_spin, *x_off, *y_off;

static gboolean
envelope_box_clip_point_movement(EnvelopeBoxPrivate* e,
    int p,
    int* dx,
    int* dy)
{
    gboolean corrected = FALSE;
    int bound;
    int curx = e->current->points[p].pos;
    int cury = e->current->points[p].val;

    if (dx != NULL) {
        if (p == 0 && *dx != 0) {
            *dx = 0;
            corrected = TRUE;
        } else if (*dx < 0) {
            if (p > 0)
                bound = e->current->points[p - 1].pos + 1;
            else
                bound = 0;
            if (*dx < bound - curx) {
                *dx = bound - curx;
                corrected = TRUE;
            }
        } else {
            if (p < e->current->num_points - 1)
                bound = e->current->points[p + 1].pos - 1;
            else
                bound = 65535;
            if (*dx > bound - curx) {
                *dx = bound - curx;
                corrected = TRUE;
            }
        }
    }

    if (dy != NULL) {
        if (*dy < 0) {
            bound = 0;
            if (*dy < bound - cury) {
                *dy = bound - cury;
                corrected = TRUE;
            }
        } else {
            bound = 64;
            if (*dy > bound - cury) {
                *dy = bound - cury;
                corrected = TRUE;
            }
        }
    }

    return corrected;
}

static void
envelope_box_move_point(EnvelopeBox* e,
    int n,
    int dpos,
    int dval);

#ifdef USE_CANVAS

#define POINT_ACTIVE "#ffcc00"
#define POINT_CURRENT "#ff7777"
#define POINT_NORMAL "#cc0000"

static void
envelope_box_initialize_point_dragging(EnvelopeBoxPrivate* e,
    GooCanvasItem* eventitem,
    GdkEventButton* event,
    GooCanvasItem* point);

static void
envelope_box_stop_point_dragging(EnvelopeBoxPrivate* e,
    GdkEventButton* event);

static void
envelope_box_canvas_set_max_x(EnvelopeBoxPrivate* e,
    int x);

static gboolean
envelope_box_point_enter(GooCanvasItem* item, GooCanvasItem* target_item,
    GdkEventCrossing* event,
    gpointer data)
{
    g_object_set(G_OBJECT(item), "fill-color", POINT_ACTIVE, NULL);
    return FALSE;
}

static gboolean
envelope_box_point_leave(GooCanvasItem* item, GooCanvasItem* target_item,
    GdkEventCrossing* event,
    EnvelopeBoxPrivate* e)
{
    g_object_set(G_OBJECT(item), "fill-color", item == e->cur_point ? POINT_CURRENT : POINT_NORMAL, NULL);
    return FALSE;
}

static gboolean
envelope_box_point_press(GooCanvasItem* item, GooCanvasItem* target_item,
    GdkEventButton* event,
    EnvelopeBoxPrivate* e)
{
    guint i;

    switch (event->button) {
    case 1:
        envelope_box_initialize_point_dragging(e, item, event, item);
        return TRUE;
    case 3:
        for (i = 0; i < e->current->num_points; i++)
            if (e->points[i] == item) {
                gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_pos), i);
                return TRUE;
            }

        return FALSE;
    default:
        break;
    }
    return FALSE;
}

static gboolean
envelope_box_point_release(GooCanvasItem* item, GooCanvasItem* target_item,
    GdkEventButton* event,
    EnvelopeBoxPrivate* e)
{
    if (event->button == 1) {
        envelope_box_stop_point_dragging(e, event);
        return TRUE;
    }
    return FALSE;
}

static gboolean
envelope_box_point_motion(GooCanvasItem* item, GooCanvasItem* target_item,
    GdkEventMotion* event,
    EnvelopeBox* eb)
{
    EnvelopeBoxPrivate* e = eb->priv;

    if (e->dragpoint != -1 && event->state & GDK_BUTTON1_MASK) {
        /* Snap movement to integer grid */
        int dx, dy;
        gdouble ex = event->x, ey = event->y;

        goo_canvas_convert_from_item_space(e->canvas, item, &ex, &ey);
        dx = ex - e->dragfromx;
        dy = e->dragfromy - ey;

        if (dx || dy) {
            envelope_box_clip_point_movement(e, e->dragpoint, &dx, &dy);

            if (event->state & GDK_CONTROL_MASK) {
                if (fabs(ex - e->origin_x) > fabs(ey - e->origin_y))
                    dy = e->dragfromy - e->origin_y;
                else
                    dx = e->origin_x - e->dragfromx;
            }

            envelope_box_move_point(eb, e->dragpoint, dx, dy);

            e->dragfromx += dx;
            e->dragfromy -= dy;

            /* Expand scrolling region horizontally, if necessary */
            if (e->dragpoint == e->current->num_points - 1 && e->current->points[e->dragpoint].pos > e->canvas_max_x) {
                envelope_box_canvas_set_max_x(e, e->current->points[e->dragpoint].pos);
            }
        }
    }
    return TRUE;
}

static void
envelope_box_canvas_size_allocate(GooCanvas* c,
    void* dummy,
    EnvelopeBoxPrivate* e)
{
    double newval = (double)GTK_WIDGET(c)->allocation.height / (64 + 10);
    if (newval != e->zoomfactor_base) {
        goo_canvas_set_scale(c, newval * e->zoomfactor_mult);
        e->zoomfactor_base = newval;
    }
}

static void
envelope_box_canvas_set_max_x(EnvelopeBoxPrivate* e,
    int x)
{
    e->canvas_max_x = x;
    goo_canvas_set_bounds(e->canvas, -2 - 10 - 10, -4, x + 2 + 10, 66);
}

static void
envelope_box_canvas_paint_grid(EnvelopeBoxPrivate* e)
{
    GooCanvasItem* item;
    GooCanvasGroup* group;
    int lines[] = {
        0,
        0,
        0,
        64,
        -6,
        0,
        0,
        0,
        -4,
        16,
        0,
        16,
        -6,
        32,
        0,
        32,
        -4,
        48,
        0,
        48,
        -6,
        64,
        0,
        64,
        2,
        0,
        16384,
        0,
        2,
        64,
        16384,
        64,
    };
    int i;

    group = GOO_CANVAS_GROUP(goo_canvas_group_new(goo_canvas_get_root_item(e->canvas),
        "x", 0.0,
        "y", 0.0,
        NULL));

    for (i = 0; i < sizeof(lines) / 4 / sizeof(int); i++) {
        item = goo_canvas_polyline_new_line(GOO_CANVAS_ITEM(group),
            lines[4 * i + 0],
            lines[4 * i + 1],
            lines[4 * i + 2],
            lines[4 * i + 3],
            "stroke-color", "#000088",
            "line-width", 0.5,
            NULL);
        goo_canvas_item_lower(item, NULL);
    }
}

static void
envelope_box_canvas_add_point(EnvelopeBox* eb,
    int n, gboolean current)
{
    EnvelopeBoxPrivate* e = eb->priv;

    // Create new point
    e->points[n] = goo_canvas_rect_new(e->group,
        (double)e->current->points[n].pos - 1.5,
        (double)(64 - e->current->points[n].val) - 1.5,
        3.0, 3.0,
        "fill-color", current ? POINT_CURRENT : POINT_NORMAL,
        "stroke-color", "#ff0000",
        "line-width", 0.0,
        NULL);
    g_signal_connect(e->points[n], "enter-notify-event",
        G_CALLBACK(envelope_box_point_enter),
        e);
    g_signal_connect(e->points[n], "leave-notify-event",
        G_CALLBACK(envelope_box_point_leave),
        e);
    g_signal_connect(e->points[n], "button-press-event",
        G_CALLBACK(envelope_box_point_press),
        e);
    g_signal_connect(e->points[n], "button-release-event",
        G_CALLBACK(envelope_box_point_release),
        e);
    g_signal_connect(e->points[n], "motion-notify-event",
        G_CALLBACK(envelope_box_point_motion),
        eb);

    // Adjust / Create line connecting to the previous point
    if (n > 0) {
        if (e->lines[n - 1]) {
            GooCanvasPoints* points;

            g_object_get(G_OBJECT(e->lines[n - 1]), "points", &points, NULL);
            points->coords[2] = (double)e->current->points[n].pos;
            points->coords[3] = (double)(64 - e->current->points[n].val);
            g_object_set(G_OBJECT(e->lines[n - 1]), "points", points, NULL);
        } else {
            e->lines[n - 1] = goo_canvas_polyline_new_line(e->group,
                (double)e->current->points[n - 1].pos,
                (double)(64 - e->current->points[n - 1].val),
                (double)e->current->points[n].pos,
                (double)(64 - e->current->points[n].val),
                "stroke-color", "black",
                "line-width", 1.0,
                NULL);
            goo_canvas_item_lower(e->lines[n - 1], NULL);
        }
    }

    // Adjust / Create line connecting to the next point
    if (n < e->current->num_points - 1) {
        if (e->lines[n]) {
            printf("muh.\n");
        } else {
            e->lines[n] = goo_canvas_polyline_new_line(e->group,
                (double)e->current->points[n].pos,
                (double)(64 - e->current->points[n].val),
                (double)e->current->points[n + 1].pos,
                (double)(64 - e->current->points[n + 1].val),
                "stroke-color", "black",
                "line-width", 1.0,
                NULL);
            goo_canvas_item_lower(e->lines[n], NULL);
        }
    }
}
#endif

static void
envelope_undo(const gint ins, const gint smp, const gboolean redo,
    gpointer arg, gpointer data)
{
    const gsize e_size = sizeof(STEnvelope);
    EnvelopeBox* e = data;
    STEnvelope* tmp = alloca(e_size);

    g_assert(IS_ENVELOPE_BOX(data));

    memcpy(tmp, arg, e_size);
    memcpy(arg, e->priv->current, e_size);
    memcpy(e->priv->current, tmp, e_size);
    envelope_box_set_envelope(e, e->priv->current);
}

void
envelope_box_log_action(EnvelopeBox* e,
    const gchar* name,
    const gint extra_flags)
{
    STEnvelope* arg = g_new(STEnvelope, 1);

    memcpy(arg, e->priv->current, sizeof(STEnvelope));
    history_log_action(HISTORY_ACTION_POINTER, _(name),
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | extra_flags, envelope_undo, e,
        sizeof(STEnvelope), arg);
}

static int
envelope_box_insert_point(EnvelopeBox* eb,
    int before,
    int pos,
    int val)
{
    EnvelopeBoxPrivate* e = eb->priv;

    /* Check if there's enough room */
    if (e->current->num_points == ST_MAX_ENVELOPE_POINTS)
        return FALSE;
    if (!(before >= 1 && before <= e->current->num_points))
        return FALSE;
    if (pos <= e->current->points[before - 1].pos)
        return FALSE;
    if (before < e->current->num_points && pos >= e->current->points[before].pos)
        return FALSE;

#ifdef USE_CANVAS
    /* Unhighlight previous current point */
    g_object_set(G_OBJECT(e->cur_point), "fill-color", POINT_NORMAL, NULL);
#endif

    envelope_box_log_action(eb, N_("Envelope point inserting"), 0);
    // Update envelope structure
    memmove(&e->current->points[before + 1], &e->current->points[before],
        (ST_MAX_ENVELOPE_POINTS - 1 - before) * sizeof(e->current->points[0]));
    e->current->points[before].pos = pos;
    e->current->points[before].val = val;
    e->current->num_points++;

    // Update GUI
    history_skip = TRUE;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_length), e->current->num_points);
    if (before <= e->current->sustain_point)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_sustain), e->current->sustain_point + 1);
    gui_block_signals(e->objs, e->sig_tags, TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_pos), before);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_offset), e->current->points[before].pos);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_value), e->current->points[before].val);
    gui_block_signals(e->objs, e->sig_tags, FALSE);
    history_skip = FALSE;

#ifdef USE_CANVAS
    // Update Canvas
    memmove(&e->points[before + 1], &e->points[before], (ST_MAX_ENVELOPE_POINTS - 1 - before) * sizeof(e->points[0]));
    memmove(&e->lines[before + 1], &e->lines[before], (ST_MAX_ENVELOPE_POINTS - 2 - before) * sizeof(e->lines[0]));
    e->lines[before] = NULL;
    envelope_box_canvas_add_point(eb, before, TRUE);
    /* New current point, already painted highlit */
    e->cur_point = e->points[before];
    e->prev_current = before;
#endif

    return TRUE;
}

static void
envelope_box_delete_point(EnvelopeBox* eb,
    int n)
{
    int nn;
    EnvelopeBoxPrivate* e = eb->priv;

    if (!(n >= 1 && n < e->current->num_points))
        return;

    envelope_box_log_action(eb, N_("Envelope point removing"), 0);

    // Update envelope structure
    memmove(&e->current->points[n], &e->current->points[n + 1],
        (ST_MAX_ENVELOPE_POINTS - 1 - n) * sizeof(e->current->points[0]));
    e->current->num_points--;
    nn = MIN(n, e->current->num_points - 1);

    // Update GUI
    history_skip = TRUE;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_length), e->current->num_points);
    if (n <= e->current->sustain_point)
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_sustain), e->current->sustain_point - 1);
    gui_block_signals(e->objs, e->sig_tags, TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_pos), nn);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_offset), e->current->points[nn].pos);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_value), e->current->points[nn].val);
    gui_block_signals(e->objs, e->sig_tags, FALSE);
    history_skip = FALSE;

#ifdef USE_CANVAS
    // Update Canvas
    goo_canvas_item_remove(e->points[n]);
    goo_canvas_item_remove(e->lines[n - 1]);
    memmove(&e->points[n], &e->points[n + 1], (ST_MAX_ENVELOPE_POINTS - 1 - n) * sizeof(e->points[0]));
    memmove(&e->lines[n - 1], &e->lines[n], (ST_MAX_ENVELOPE_POINTS - 1 - n) * sizeof(e->lines[0]));
    e->lines[e->current->num_points - 1] = NULL;
    if (e->lines[n - 1]) {
        GooCanvasPoints* points;

        g_object_get(G_OBJECT(e->lines[n - 1]), "points", &points, NULL);
        points->coords[0] = (double)e->current->points[n - 1].pos;
        points->coords[1] = (double)(64 - e->current->points[n - 1].val);
        g_object_set(G_OBJECT(e->lines[n - 1]), "points", points, NULL);
    }
    envelope_box_canvas_set_max_x(e, e->current->points[e->current->num_points - 1].pos);

    /* New current point highlighting */
    if (n < e->current->num_points) { /* If the current point is last, it will be highlit automatically */
        e->cur_point = e->points[n];
        e->prev_current = n;
        g_object_set(G_OBJECT(e->cur_point), "fill-color", POINT_CURRENT, NULL);
    }
#endif
}

#ifdef USE_CANVAS
static void
envelope_box_canvas_point_out_of_sight(EnvelopeBoxPrivate* e,
    STEnvelopePoint point, gint* dragx, gint* dragy)
{
    double xposwindow = point.pos, y = point.val;
    double bottom = gtk_adjustment_get_upper(e->vadj) - gtk_adjustment_get_page_size(e->vadj)
        - gtk_adjustment_get_value(e->vadj) + 2.0 * (e->zoomfactor_base * e->zoomfactor_mult); /* :-P */

    goo_canvas_convert_to_pixels(e->canvas, &xposwindow, &y);
    xposwindow -= gtk_adjustment_get_value(e->hadj);

    if (xposwindow < 0)
        *dragx = -1;
    if (xposwindow >= GTK_WIDGET(e->canvas)->allocation.width)
        *dragx = 1;

    if (y < bottom)
        *dragy = -1;
    if (y >= bottom + GTK_WIDGET(e->canvas)->allocation.height)
        *dragy = 1;
}

/* We assume here that the movement is valid! */
static void
loop_sustain_move(GooCanvasItem* line, guint pos)
{
    GooCanvasPoints* points;

    g_object_get(G_OBJECT(line), "points", &points, NULL);
    points->coords[0] = points->coords[2] = pos;
    g_object_set(G_OBJECT(line), "points", points, NULL);
}

static GooCanvasItem*
loop_sustain_new(EnvelopeBoxPrivate* e, guint pos, gboolean is_sustain)
{
    GooCanvasItem* line;

    if (is_sustain) {
        GooCanvasLineDash* dash = goo_canvas_line_dash_new(2, 4.0, 4.0);

        line = goo_canvas_polyline_new_line(e->group, pos, 0.0, pos, 64.0,
            "stroke-color", "#00cc00",
            "line-width", 1.5,
            "line-dash", dash,
            NULL);
        goo_canvas_line_dash_unref(dash);
    } else {
        line = goo_canvas_polyline_new_line(e->group, pos, 0.0, pos, 64.0,
            "stroke-color", "#7700ff",
            "line-width", 1.5,
            NULL);
    }
    if (is_sustain && e->loop_start_line)
        goo_canvas_item_raise(line, e->loop_start_line);
    else if (!is_sustain && e->sustain_line)
        goo_canvas_item_lower(line, e->sustain_line);
    else
        goo_canvas_item_lower(line, NULL);

    return line;
}
#endif

static void
envelope_box_move_point(EnvelopeBox* eb,
    int n,
    int dpos,
    int dval)
{
#ifdef USE_CANVAS
    GooCanvasPoints* points;
#endif
    EnvelopeBoxPrivate* e = eb->priv;

    if (!history_test_collate(HISTORY_ACTION_POINTER,
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | HISTORY_EXTRA_FLAGS(n), eb))
        envelope_box_log_action(eb, N_("Envelope point moving"), HISTORY_EXTRA_FLAGS(n));

    // Update envelope structure
    e->current->points[n].pos += dpos;
    e->current->points[n].val += dval;

    // Update GUI
    gui_block_signals(e->objs, e->sig_tags, TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_offset), e->current->points[n].pos);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_value), e->current->points[n].val);
    gui_block_signals(e->objs, e->sig_tags, FALSE);

#ifdef USE_CANVAS
    // Update Canvas
    goo_canvas_item_translate(e->points[n], dpos, -dval);
    if (n < e->current->num_points - 1) {
        g_object_get(G_OBJECT(e->lines[n]), "points", &points, NULL);

        points->coords[0] += dpos;
        points->coords[1] -= dval;
        g_object_set(G_OBJECT(e->lines[n]), "points", points, NULL);
    }
    if (n > 0) {
        g_object_get(G_OBJECT(e->lines[n - 1]), "points", &points, NULL);

        points->coords[2] += dpos;
        points->coords[3] -= dval;
        g_object_set(G_OBJECT(e->lines[n - 1]), "points", points, NULL);

        if (n == e->current->sustain_point && e->current->flags & EF_SUSTAIN)
            loop_sustain_move(e->sustain_line, e->current->points[n].pos);

        if (n == e->current->loop_start && e->current->flags & EF_LOOP)
            loop_sustain_move(e->loop_start_line, e->current->points[n].pos);

        if (n == e->current->loop_end && e->current->flags & EF_LOOP)
            loop_sustain_move(e->loop_end_line, e->current->points[n].pos);
    }
#endif
}

#ifdef USE_CANVAS

/* This function returns world coordinates for a click on an item, if
   it is given, or else (if item == NULL), assumes the click was on
   the root canvas (event->button.x/y contain screen pixel coords in
   that case). A little confusing, I admit. */

static void
envelope_box_get_world_coords(GooCanvasItem* item,
    GdkEventButton* event,
    EnvelopeBoxPrivate* e,
    double* worldx,
    double* worldy)
{
    *worldx = event->x;
    *worldy = event->y;
    if (item == NULL) {
        goo_canvas_convert_from_pixels(e->canvas, worldx, worldy);
    } else
        goo_canvas_convert_from_item_space(e->canvas, item, worldx, worldy);
}

static void
envelope_box_initialize_point_dragging(EnvelopeBoxPrivate* e,
    GooCanvasItem* eventitem,
    GdkEventButton* event,
    GooCanvasItem* point)
{
    GdkCursor* cursor;
    int i;
    double x, y;

    envelope_box_get_world_coords(eventitem, event, e, &x, &y);

    cursor = gdk_cursor_new(GDK_FLEUR);
    goo_canvas_pointer_grab(e->canvas, point,
        GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
        cursor,
        event->time);
    gdk_cursor_unref(cursor);

    e->origin_x = e->dragfromx = x;
    e->origin_y = e->dragfromy = y;

    e->dragpoint = -1;
    for (i = 0; i < 12; i++) {
        if (e->points[i] == point) {
            e->dragpoint = i;
            break;
        }
    }
    g_assert(e->dragpoint != -1);

    e->dragging_item_from_x = e->current->points[e->dragpoint].pos;
    e->dragging_item_from_y = e->current->points[e->dragpoint].val;

    gui_block_signals(e->objs, e->sig_tags, TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_pos), e->dragpoint);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_offset), e->current->points[e->dragpoint].pos);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_value), e->current->points[e->dragpoint].val);
    gui_block_signals(e->objs, e->sig_tags, FALSE);

    g_object_set(G_OBJECT(e->cur_point), "fill-color", POINT_NORMAL, NULL);
    e->prev_current = e->dragpoint;
    e->cur_point = point;
}

static void
envelope_box_stop_point_dragging(EnvelopeBoxPrivate* e,
    GdkEventButton* event)
{
    gint dragx = 0, dragy = 0;
    gdouble dx = 0.0, dy = 0.0, zoom = e->zoomfactor_base * e->zoomfactor_mult;

    if (e->dragpoint == -1)
        return;

    goo_canvas_pointer_ungrab(e->canvas, e->points[e->dragpoint], event->time);

    /* Shrink scrolling region horizontally, if necessary */
    if (e->dragpoint == e->current->num_points - 1 && e->current->points[e->dragpoint].pos < e->canvas_max_x) {
        envelope_box_canvas_set_max_x(e, e->current->points[e->dragpoint].pos);
    }

    /* If new location is out of sight, jump there */
    envelope_box_canvas_point_out_of_sight(e, e->current->points[e->dragpoint], &dragx, &dragy);
    if (dragx < 0)
        dx = e->current->points[e->dragpoint].pos - 10.0 / zoom;
    else if (dragx > 0)
        dx = e->current->points[e->dragpoint].pos - (GTK_WIDGET(e->canvas)->allocation.width - 10.0) / zoom;
    else if (dragy)
        dx = gtk_adjustment_get_value(e->hadj) / zoom - 22.0;

    if (dragy < 0)
        dy = 64.0 - e->current->points[e->dragpoint].val - (GTK_WIDGET(e->canvas)->allocation.height - 10.0) / zoom;
    else if (dragy > 0)
        dy = 64.0 - e->current->points[e->dragpoint].val - 10.0 / zoom;
    else if (dragx)
        dy = gtk_adjustment_get_value(e->vadj) / zoom - 4.0;

    if (dragx || dragy)
        goo_canvas_scroll_to(e->canvas, dx, dy);
}

static gboolean
scrolled_window_press(GtkWidget* widget,
    GdkEventButton* event,
    EnvelopeBoxPrivate* e)
{
    GdkCursor* cursor;

    if (event->button == 2) {
        /* Middle button */
        if (event->state & GDK_SHIFT_MASK) {
            /* Zoom in/out */
            cursor = gdk_cursor_new(GDK_SIZING);
            goo_canvas_pointer_grab(e->canvas, e->group,
                GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                cursor,
                event->time);
            gdk_cursor_unref(cursor);

            e->zooming_canvas = TRUE;
            e->dragfromy = event->y - gtk_adjustment_get_value(e->vadj);
            e->zooming_canvas_from_val = e->zoomfactor_mult;

            return TRUE;
        } else {
            /* Drag canvas */
            cursor = gdk_cursor_new(GDK_FLEUR);
            goo_canvas_pointer_grab(e->canvas, e->group,
                GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
                cursor,
                event->time);
            gdk_cursor_unref(cursor);

            e->dragging_canvas = TRUE;
            e->dragfromx = event->x;
            e->dragfromy = event->y;

            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
scrolled_window_release(GtkWidget* widget,
    GdkEventButton* event,
    EnvelopeBoxPrivate* e)
{
    if (event->button == 2) {
        goo_canvas_pointer_ungrab(e->canvas, e->group, event->time);
        e->dragging_canvas = FALSE;
        e->zooming_canvas = FALSE;
        return TRUE;
    }

    return FALSE;
}

static gboolean
scrolled_window_motion(GtkScrolledWindow* widget,
    GdkEventMotion* event,
    EnvelopeBoxPrivate* e)
{
    gdouble lower, upper, new, step;

    if (e->dragging_canvas) {
        lower = gtk_adjustment_get_lower(e->hadj);
        upper = gtk_adjustment_get_upper(e->hadj) - gtk_adjustment_get_page_size(e->hadj);
        step = event->x - e->dragfromx;
        new = gtk_adjustment_get_value(e->hadj) - step;

        e->dragfromx = event->x - step;
        if (new < lower)
            new = lower;
        if (new > upper)
            new = upper;

        gtk_adjustment_set_value(e->hadj, new);

        lower = gtk_adjustment_get_lower(e->vadj);
        upper = gtk_adjustment_get_upper(e->vadj) - gtk_adjustment_get_page_size(e->vadj);
        step = event->y - e->dragfromy;
        new = gtk_adjustment_get_value(e->vadj) - step;

        e->dragfromy = event->y - step;
        if (new < lower)
            new = lower;
        if (new > upper)
            new = upper;

        gtk_adjustment_set_value(e->vadj, new);

        return TRUE;
    } else if (e->zooming_canvas) {
        gdouble dy = event->y - e->dragfromy - gtk_adjustment_get_value(e->vadj);

        e->zoomfactor_mult = e->zooming_canvas_from_val - (dy / 20);
        if (e->zoomfactor_mult < 1.0) {
            e->zoomfactor_mult = 1.0;
        }
        goo_canvas_set_scale(e->canvas, e->zoomfactor_base * e->zoomfactor_mult);
        return TRUE;
    }

    return FALSE;
}

static gboolean
envelope_box_canvas_event_button_press(GooCanvas* canvas,
    GdkEventButton* event,
    EnvelopeBox* eb)
{
    if (event->button == 1) {
        EnvelopeBoxPrivate* e = eb->priv;

        gdouble x, y;
        GooCanvasItem* item;

        gint i, insert_after = -1, xint;

        /* Find out where to insert new point */
        x = event->x;
        y = event->y;
        goo_canvas_convert_from_pixels(canvas, &x, &y);
        xint = lrint(x);
        item = goo_canvas_get_item_at(canvas, x, y, FALSE);
        /* GooCanvas probably has a bug here. The event is send to canvas first, then
		   to the item. */
        for (i = 0, insert_after = -1; i < e->current->num_points && e->points[i]; i++) {
            if (e->points[i] == item) {
                /* An already existing point has been selected. Will
		       be handled by envelope_box_point_event(). */
                return FALSE;
            }
            if (e->current->points[i].pos < xint) {
                insert_after = i;
            }
        }

        if (insert_after != -1 && y >= 0 && y < 64) {
            /* Insert new point and start dragging */
            envelope_box_insert_point(eb, insert_after + 1, xint, 64 - y);
            envelope_box_initialize_point_dragging(e, NULL, event, e->points[insert_after + 1]);
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
envelope_box_canvas_event_button_release(GooCanvas* canvas,
    GdkEventButton* event,
    EnvelopeBoxPrivate* e)
{
    double x, y;
    GooCanvasItem* item;

    int i;

    if (event->button == 1) {
        x = event->x;
        y = event->y;
        goo_canvas_convert_from_pixels(canvas, &x, &y);
        item = goo_canvas_get_item_at(canvas, x, y, FALSE);

        for (i = 0; i < e->current->num_points && e->points[i]; i++)
            if (e->points[i] == item) {
                /* An already existing point has been selected. Will
		       be handled by envelope_box_point_event(). */
                return FALSE;
            }
        envelope_box_stop_point_dragging(e, event);
        return TRUE;
    }
    return FALSE;
}

void envelope_box_zoom_in(EnvelopeBoxPrivate* e)
{
    if (e->zoomfactor_mult < 20.0) {
        e->zoomfactor_mult *= 1.414;
        goo_canvas_set_scale(e->canvas, e->zoomfactor_base * e->zoomfactor_mult);
    }
}

void envelope_box_zoom_out(EnvelopeBoxPrivate* e)
{
    if (e->zoomfactor_mult > 1.0) {
        e->zoomfactor_mult /= 1.414;
        if (e->zoomfactor_mult < 1.0)
            e->zoomfactor_mult = 1.0;
        goo_canvas_set_scale(e->canvas, e->zoomfactor_base * e->zoomfactor_mult);
    }
}

static gboolean
envelope_box_canvas_event_scroll(GooCanvas* canvas,
    GdkEventScroll* event,
    EnvelopeBoxPrivate* e)
{
    if (event->direction == GDK_SCROLL_UP) {
        if (event->state & GDK_SHIFT_MASK) { /* Horizontal scrolling */
            gtk_adjustment_set_value(e->hadj, gtk_adjustment_get_value(e->hadj) - gtk_adjustment_get_step_increment(e->hadj));
            return TRUE;
        } else if (event->state & GDK_CONTROL_MASK) { /* Zooming */
            envelope_box_zoom_in(e);
            return TRUE;
        }
    } else if (event->direction == GDK_SCROLL_DOWN) {
        if (event->state & GDK_SHIFT_MASK) { /* Horizontal scrolling */
            gdouble value = gtk_adjustment_get_value(e->hadj) + gtk_adjustment_get_step_increment(e->hadj);
            gdouble maxval = gtk_adjustment_get_upper(e->hadj) - gtk_adjustment_get_page_size(e->hadj);

            if (value > maxval)
                value = maxval;
            gtk_adjustment_set_value(e->hadj, value);

            return TRUE;
        } else if (event->state & GDK_CONTROL_MASK) { /* Zooming */
            envelope_box_zoom_out(e);
            return TRUE;
        }
    }

    return FALSE;
}
#endif

static void
set_num_points(EnvelopeBoxPrivate* e, const gint newval)
{
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_pos), 0, newval - 1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_sustain), 0, newval - 1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_loop_start), 0, newval - 1);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_loop_end), 0, newval - 1);
}

#ifdef USE_CANVAS
static void update_sustain_line(EnvelopeBoxPrivate* e)
{
    if (e->current->flags & EF_SUSTAIN && !e->sustain_line)
        e->sustain_line = loop_sustain_new(e, e->current->points[e->current->sustain_point].pos, TRUE);
    if (!(e->current->flags & EF_SUSTAIN) && e->sustain_line) {
        goo_canvas_item_remove(e->sustain_line);
        e->sustain_line = NULL;
    }
}

static void update_loop_lines(EnvelopeBoxPrivate* e)
{
    if (e->current->flags & EF_LOOP) {
        if (!e->loop_start_line)
            e->loop_start_line = loop_sustain_new(e, e->current->points[e->current->loop_start].pos, FALSE);
        if (!e->loop_end_line)
            e->loop_end_line = loop_sustain_new(e, e->current->points[e->current->loop_end].pos, FALSE);
    }
    if (!(e->current->flags & EF_LOOP)) {
        if (e->loop_start_line) {
            goo_canvas_item_remove(e->loop_start_line);
            e->loop_start_line = NULL;
        }
        if (e->loop_end_line) {
            goo_canvas_item_remove(e->loop_end_line);
            e->loop_end_line = NULL;
        }
    }
}
#endif

void envelope_box_set_envelope(EnvelopeBox* eb, STEnvelope* env)
{
    int i;
    EnvelopeBoxPrivate* e;

    g_return_if_fail(eb != NULL);
    e = eb->priv;

    if (env == NULL) {
        env = &dummy_envelope;
    }
    e->current = env;

    // Some preliminary Paranoia...
    g_assert(env->num_points >= 1 && env->num_points <= ST_MAX_ENVELOPE_POINTS);
    for (i = 0; i < env->num_points; i++) {
        int h = env->points[i].val;
        g_assert(h >= 0 && h <= 64);
    }

    // Update GUI
#ifdef USE_CANVAS
    /* Creation and removing lines are handled by toggle button callbacks */
    if (env->flags & EF_SUSTAIN && e->sustain_line)
        loop_sustain_move(e->sustain_line, env->points[env->sustain_point].pos);

    if (env->flags & EF_LOOP && e->loop_start_line) {
        loop_sustain_move(e->loop_start_line, env->points[env->loop_start].pos);
        loop_sustain_move(e->loop_end_line, env->points[env->loop_end].pos);
    }

    // Update Canvas
    for (i = 0; i < ARRAY_SIZE(e->points) && e->points[i]; i++) {
        goo_canvas_item_remove(e->points[i]);
        e->points[i] = NULL;
    }
    for (i = 0; i < ARRAY_SIZE(e->lines) && e->lines[i]; i++) {
        goo_canvas_item_remove(e->lines[i]);
        e->lines[i] = NULL;
    }
    for (i = 0; i < env->num_points; i++) {
        envelope_box_canvas_add_point(eb, i, !i);
    }
    envelope_box_canvas_set_max_x(e, env->points[env->num_points - 1].pos);
    e->prev_current = 0;
    e->cur_point = e->points[0];
#endif

    gui_block_signals(e->objs, e->sig_tags, TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->enable), env->flags & EF_ON);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->sustain), env->flags & EF_SUSTAIN);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(e->loop), env->flags & EF_LOOP);

#ifdef USE_CANVAS
    update_sustain_line(e);
    update_loop_lines(e);
#endif

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_length), env->num_points);

    set_num_points(e, env->num_points);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_pos), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_offset), env->points[0].pos);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_value), env->points[0].val);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_sustain), env->sustain_point);

    gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_loop_start), 0, e->current->loop_end);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_loop_end), e->current->loop_start, e->current->num_points - 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_loop_start), env->loop_start);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_loop_end), env->loop_end);
    gui_block_signals(e->objs, e->sig_tags, FALSE);
}

static void handle_enable_button(GtkToggleButton* t, EnvelopeBoxPrivate* e)
{
    const gboolean is_active = gtk_toggle_button_get_active(t);

    history_log_toggle_button(t,
        _(is_active ? N_("Envelope enabling") : N_("Envelope disabling")),
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS,
        e->current->flags & EF_ON);

    if (is_active)
        e->current->flags |= EF_ON;
    else
        e->current->flags &= ~EF_ON;
}

static void handle_sustain_button(GtkToggleButton* t, EnvelopeBoxPrivate* e)
{
    const gboolean is_active = gtk_toggle_button_get_active(t);

    history_log_toggle_button(t,
        _(is_active ? N_("Sustain enabling") : N_("Sustain disabling")),
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS,
        e->current->flags & EF_SUSTAIN);

    if (is_active)
        e->current->flags |= EF_SUSTAIN;
    else
        e->current->flags &= ~EF_SUSTAIN;
#ifdef USE_CANVAS
    update_sustain_line(e);
#endif
}

static void handle_loop_button(GtkToggleButton* t, EnvelopeBoxPrivate* e)
{
    const gboolean is_active = gtk_toggle_button_get_active(t);

    history_log_toggle_button(t,
        _(is_active ? N_("Envelope loop enabling") : N_("Envelope loop disabling")),
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS,
        e->current->flags & EF_LOOP);

    if (is_active)
        e->current->flags |= EF_LOOP;
    else
        e->current->flags &= ~EF_LOOP;
#ifdef USE_CANVAS
    update_loop_lines(e);
#endif
}

static void handle_spin_sustain(GtkSpinButton* s, EnvelopeBoxPrivate* e)
{
    history_log_spin_button(s,
        _("Sustain point changing"),
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS,
        e->current->sustain_point);

    e->current->sustain_point = gtk_spin_button_get_value_as_int(s);
#ifdef USE_CANVAS
    if (e->current->flags & EF_SUSTAIN)
        loop_sustain_move(e->sustain_line, e->current->points[e->current->sustain_point].pos);
#endif
}

static void handle_spin_loop_start(GtkSpinButton* s, EnvelopeBoxPrivate* e)
{
    history_log_spin_button(s,
        _("Envelope loop start changing"),
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS,
        e->current->loop_start);

    e->current->loop_start = gtk_spin_button_get_value_as_int(s);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_loop_end), e->current->loop_start, e->current->num_points - 1);
#ifdef USE_CANVAS
    if (e->current->flags & EF_LOOP)
        loop_sustain_move(e->loop_start_line, e->current->points[e->current->loop_start].pos);
#endif
}

static void handle_spin_loop_end(GtkSpinButton* s, EnvelopeBoxPrivate* e)
{
    history_log_spin_button(s,
        _("Envelope loop end changing"),
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS,
        e->current->loop_end);

    e->current->loop_end = gtk_spin_button_get_value_as_int(s);
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(e->spin_loop_start), 0, e->current->loop_end);
#ifdef USE_CANVAS
    if (e->current->flags & EF_LOOP)
        loop_sustain_move(e->loop_end_line, e->current->points[e->current->loop_end].pos);
#endif
}

static void
spin_length_changed(GtkSpinButton* spin,
    EnvelopeBox* eb)
{
    int newval = gtk_spin_button_get_value_as_int(spin);
    EnvelopeBoxPrivate* e = eb->priv;

#ifdef USE_CANVAS
    int i;

    if (newval < e->current->num_points)
        for (i = e->current->num_points - 1; i >= newval; i--) {
            goo_canvas_item_remove(e->points[i]);
            e->points[i] = NULL;
            goo_canvas_item_remove(e->lines[i - 1]);
            e->lines[i - 1] = NULL;
        }

    if (newval > e->current->num_points)
        for (i = e->current->num_points; i < newval; i++) {
            if (e->current->points[i].pos <= e->current->points[i - 1].pos)
                e->current->points[i].pos = e->current->points[i - 1].pos + 10;
            envelope_box_canvas_add_point(eb, i, FALSE);
        }
#endif

    /* Length changing can affect other controls (loop, sustain), so it's simplier to
       log the whole envelope */
    if (!history_test_collate(HISTORY_ACTION_POINTER,
        HISTORY_FLAG_LOG_PAGE | HISTORY_FLAG_LOG_INS | HISTORY_EXTRA_FLAGS(256), e))
        envelope_box_log_action(eb, N_("Envelope length changing"), HISTORY_EXTRA_FLAGS(256));

    e->current->num_points = newval;
#ifdef USE_CANVAS
    envelope_box_canvas_set_max_x(e, e->current->points[e->current->num_points - 1].pos);
#endif
    /* We log overall changes of the envelope rather
       than changes of the particular controls */
    history_skip = TRUE;
    set_num_points(e, newval);
    history_skip = FALSE;
}

static void
spin_pos_changed(GtkSpinButton* spin,
    EnvelopeBoxPrivate* e)
{
    int p = gtk_spin_button_get_value_as_int(spin);

    g_assert(p >= 0 && p < e->current->num_points);

    gui_block_signals(e->objs, e->sig_tags, TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_offset), e->current->points[p].pos);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(e->spin_value), e->current->points[p].val);
    gui_block_signals(e->objs, e->sig_tags, FALSE);

#ifdef USE_CANVAS
    if (e->prev_current < e->current->num_points)
        g_object_set(G_OBJECT(e->cur_point), "fill-color", POINT_NORMAL, NULL);
    e->prev_current = p;
    e->cur_point = e->points[p];
    g_object_set(G_OBJECT(e->cur_point), "fill-color", POINT_CURRENT, NULL);
#endif
}

static void
spin_offset_changed(GtkSpinButton* spin,
    EnvelopeBox* eb)
{
    EnvelopeBoxPrivate* e = eb->priv;
    int p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(e->spin_pos));
    int dx;

    g_assert(p >= 0 && p < e->current->num_points);

    dx = gtk_spin_button_get_value_as_int(spin) - e->current->points[p].pos;

    envelope_box_clip_point_movement(e, p, &dx, NULL);
    envelope_box_move_point(eb, p, dx, 0);

#ifdef USE_CANVAS
    // Horizontal adjustment of scrolling region
    if (p == e->current->num_points - 1) {
        envelope_box_canvas_set_max_x(e, e->current->points[p].pos);
    }
#endif
}

static void
spin_value_changed(GtkSpinButton* spin,
    EnvelopeBox* eb)
{
    EnvelopeBoxPrivate* e = eb->priv;
    int p = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(e->spin_pos));
    int dy;

    g_assert(p >= 0 && p < e->current->num_points);

    dy = gtk_spin_button_get_value_as_int(spin) - e->current->points[p].val;

    envelope_box_clip_point_movement(e, p, NULL, &dy);
    envelope_box_move_point(eb, p, 0, dy);
}

static void
insert_clicked(GtkWidget* w,
    EnvelopeBox* eb)
{
    EnvelopeBoxPrivate* e = eb->priv;
    int pos = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(e->spin_pos));

    /* We cannot insert point before the beginning */
    if (!pos)
        return;

    envelope_box_insert_point(eb, pos, (e->current->points[pos - 1].pos + e->current->points[pos].pos) >> 1,
        (e->current->points[pos - 1].val + e->current->points[pos].val) >> 1);
}

static void
delete_clicked(GtkWidget* w,
    EnvelopeBox* e)
{
    envelope_box_delete_point(e, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(e->priv->spin_pos)));
}

#ifdef USE_CANVAS
void envelope_box_zoom_normal(EnvelopeBoxPrivate* e)
{
    e->zoomfactor_mult = 1.0;
    goo_canvas_set_scale(e->canvas, e->zoomfactor_base * e->zoomfactor_mult);
}
#endif

static void handle_copy(EnvelopeBox* e)
{
    g_signal_emit(G_OBJECT(e), envelope_box_signals[SIG_COPY_ENV], 0, e->priv->current);
}

static void handle_paste(EnvelopeBox* e)
{
    g_signal_emit(G_OBJECT(e), envelope_box_signals[SIG_PASTE_ENV], 0, e->priv->current);
}

static void scale_env(GtkWidget* w, EnvelopeBox** eb)
{
    guint i;
    EnvelopeBoxPrivate* e = (*eb)->priv;
    gboolean overrange = FALSE;

    e->scale_x = gtk_spin_button_get_value(GTK_SPIN_BUTTON(x_spin));
    e->scale_y = gtk_spin_button_get_value(GTK_SPIN_BUTTON(y_spin));
    e->off_x = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(x_off));
    e->off_y = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(y_off));

    envelope_box_log_action(*eb, N_("Envelope transformation"), 0);

    for (i = 0; i < ST_MAX_ENVELOPE_POINTS; i++) {
        guint16 oldval, ival;
        gfloat newval;

        oldval = e->current->points[i].pos;
        newval = (gfloat)oldval * e->scale_x;
        if (i)
            newval += e->off_x;
        if (newval > 65535.0) {
            newval = 65535.0;
            overrange = TRUE;
        }
        if (newval < 0.0) {
            ival = 0;
            overrange = TRUE;
        } else if (!(ival = lrintf(newval)) && oldval) {
            ival = 1;
            overrange = TRUE;
        }
        /* Prevent possible points collation */
        if (i && ival <= e->current->points[i - 1].pos) {
            if (e->current->points[i - 1].pos < 65535)
                ival = e->current->points[i - 1].pos + 1;
            else
                ival = 65535;
        }
        e->current->points[i].pos = ival;

        oldval = e->current->points[i].val;
        newval = (gfloat)oldval * e->scale_y + e->off_y;
        if (newval > 64.0) {
            newval = 64.0;
            overrange = TRUE;
        }
        if (newval < 0.0) {
            ival = 0;
            overrange = TRUE;
        } else if (!(ival = lrintf(newval)) && oldval) {
            ival = 1;
            overrange = TRUE;
        }
        e->current->points[i].val = ival;
    }
    /* There could also be a situation when some points at the end have the
       maximum possible position after scaling */
    for (i = e->current->num_points - 2; i > 0 &&
        e->current->points[i].pos >= e->current->points[i + 1].pos; i--)
        e->current->points[i].pos = e->current->points[i + 1].pos - 1;

    envelope_box_set_envelope(*eb, e->current);

    if (overrange) {
        static GtkWidget* dialog = NULL;

        gui_warning_dialog(&dialog, _("Coordinates of some points had either too low or too hight values. "
            "The envelope is distorted after transformation."), FALSE);
    }
}

static void inverse_env(EnvelopeBox* eb)
{
    guint i;
    EnvelopeBoxPrivate* e = eb->priv;

    envelope_box_log_action(eb, N_("Envelope inverting"), 0);
    for (i = 0; i < ST_MAX_ENVELOPE_POINTS; i++)
        e->current->points[i].val = 64 - e->current->points[i].val;
    envelope_box_set_envelope(eb, e->current);
}

static void handle_scale(EnvelopeBox* e)
{
    gint resp;

    if (!scale_dialog) {
        GtkWidget *vbox, *thing;

        scale_dialog = gtk_dialog_new_with_buttons(_("Envelope Transformation"), GTK_WINDOW(mainwindow),
            GTK_DIALOG_MODAL, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);

        vbox = gtk_dialog_get_content_area(GTK_DIALOG(scale_dialog));
        gtk_box_set_spacing(GTK_BOX(vbox), 4);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 4);

        gtk_box_pack_start(GTK_BOX(vbox),
            gui_labelled_spin_button_new_full(_("X Scale"), 1.0, 0.1, 10.0, 0.01, 1.0, 2,
            &x_spin, "activate", scale_env, &e, FALSE, NULL) ,TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(vbox),
            gui_labelled_spin_button_new_full(_("X Offset"), 0.0, -63.0, 63.0, 1.0, 5.0, 0,
            &x_off, "activate", scale_env, &e, FALSE, NULL) ,TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(vbox),
            gui_labelled_spin_button_new_full(_("Y Scale"), 1.0, 0.1, 10.0, 0.01, 1.0, 2,
            &y_spin, "activate", scale_env, &e, FALSE, NULL) ,TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(vbox),
            gui_labelled_spin_button_new_full(_("Y Offset"), 0.0, -63.0, 63.0, 1.0, 5.0, 0,
            &y_off, "activate", scale_env, &e, FALSE, NULL) ,TRUE, TRUE, 0);

        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
        gtk_widget_show_all(scale_dialog);
    } else {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(x_spin), e->priv->scale_x);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(y_spin), e->priv->scale_y);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(x_off), e->priv->off_x);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(y_off), e->priv->off_y);
    }

    gtk_window_set_position(GTK_WINDOW(scale_dialog), GTK_WIN_POS_MOUSE);
    gtk_widget_grab_focus(x_spin);
    resp = gtk_dialog_run(GTK_DIALOG(scale_dialog));
    gtk_widget_hide(scale_dialog);

    if (resp == GTK_RESPONSE_OK)
        scale_env(NULL, &e);
}

GtkWidget* envelope_box_new(const gchar* label)
{
    EnvelopeBox* eb;
    EnvelopeBoxPrivate* e;
    GtkWidget *box2, *thing, *box3, *box4;
#ifdef USE_CANVAS
    GtkWidget *canvas, *menu;
    GtkBuilder* builder;

    struct menu_callback cb[] = {
        { "env_zoom_in", envelope_box_zoom_in, NULL },
        { "env_zoom_out", envelope_box_zoom_out, NULL },
        { "env_normal", envelope_box_zoom_normal, NULL },
        { NULL }
    };
#else
    GtkWidget* frame;
#endif

    eb = g_object_new(envelope_box_get_type(), NULL);
    GTK_BOX(eb)->spacing = 2;
    GTK_BOX(eb)->homogeneous = FALSE;
    e = eb->priv = g_new(EnvelopeBoxPrivate, 1);

    e->scale_x = e->scale_y = 1.0;
    e->off_x = e->off_y = 0;

    box2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(eb), box2, FALSE, FALSE, 0);

    e->enable = thing = gtk_check_button_new_with_label(label);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    gtk_widget_show(thing);
    e->sig_tags[7] = g_signal_connect(thing, "toggled",
        G_CALLBACK(handle_enable_button), e);

    thing = gui_button(GTK_STOCK_COPY, G_CALLBACK(handle_copy), eb, box2, TRUE, FALSE);
    gtk_widget_set_tooltip_text(thing, _("Copy envelope"));
    thing = gui_button(GTK_STOCK_PASTE, G_CALLBACK(handle_paste), eb, box2, TRUE, FALSE);
    gtk_widget_set_tooltip_text(thing, _("Paste envelope"));
    gtk_widget_show_all(box2);

    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), gui_get_pixmap(GUI_PIXMAP_ENV_SCALE));
    gtk_widget_set_tooltip_text(thing, _("Scale envelope"));
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(handle_scale), eb);
    gtk_widget_show_all(box2);

    thing = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(thing), gui_get_pixmap(GUI_PIXMAP_ENV_INVERSE));
    gtk_widget_set_tooltip_text(thing, _("Inverse envelope by Y"));
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, FALSE, 0);
    g_signal_connect_swapped(thing, "clicked",
        G_CALLBACK(inverse_env), eb);
    gtk_widget_show_all(box2);

    box2 = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(eb), box2, FALSE, TRUE, 0);
    gtk_widget_show(box2);

    /* Numerical list editing fields */
    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);

    e->sig_tags[0] = gui_put_labelled_spin_button(box3,
        _("Length"), 1, 12, &e->spin_length, spin_length_changed, eb, TRUE);
    e->sig_tags[1] = gui_put_labelled_spin_button(box3,
        _("Current"), 0, 11, &e->spin_pos, spin_pos_changed, e, TRUE);
    e->sig_tags[2] = gui_put_labelled_spin_button(box3,
        _("Offset"), 0, 65535, &e->spin_offset, spin_offset_changed, eb, TRUE);
    e->sig_tags[3] = gui_put_labelled_spin_button(box3,
        _("Value"), 0, 64, &e->spin_value, spin_value_changed, eb, TRUE);
    gtk_widget_show_all(box3);

    box4 = gtk_hbox_new(TRUE, 4);
    gtk_box_pack_start(GTK_BOX(box3), box4, FALSE, TRUE, 0);
    gtk_widget_show(box4);

    thing = gtk_button_new_with_label(_("Insert"));
    gtk_box_pack_start(GTK_BOX(box4), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(insert_clicked), eb);

    thing = gtk_button_new_with_label(_("Delete"));
    gtk_box_pack_start(GTK_BOX(box4), thing, TRUE, TRUE, 0);
    gtk_widget_show(thing);
    g_signal_connect(thing, "clicked",
        G_CALLBACK(delete_clicked), eb);

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    // Here comes the graphical stuff
#ifdef USE_CANVAS
    canvas = goo_canvas_new();
    e->canvas = GOO_CANVAS(canvas);
    g_object_set(G_OBJECT(canvas), "background-color", "#ffffff", NULL); /* GooCanvas allows to set background simply! */

    memset(e->points, 0, sizeof(e->points));
    memset(e->lines, 0, sizeof(e->lines));
    e->sustain_line = e->loop_start_line = e->loop_end_line = NULL;
    e->zoomfactor_base = 0.0;
    e->zoomfactor_mult = 1.0;
    e->dragpoint = -1;
    e->prev_current = 0;
    e->dragging_canvas = e->zooming_canvas = FALSE;

    envelope_box_canvas_paint_grid(e);

    e->group = goo_canvas_group_new(goo_canvas_get_root_item(e->canvas),
        "x", 0.0,
        "y", 0.0,
        NULL);

    g_signal_connect_after(canvas, "button-press-event",
        G_CALLBACK(envelope_box_canvas_event_button_press),
        eb);
    g_signal_connect_after(canvas, "button-release-event",
        G_CALLBACK(envelope_box_canvas_event_button_release),
        e);
    g_signal_connect(canvas, "scroll-event",
        G_CALLBACK(envelope_box_canvas_event_scroll),
        e);

    g_signal_connect_after(canvas, "size_allocate",
        G_CALLBACK(envelope_box_canvas_size_allocate), e);

    thing = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(thing), GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(thing), GTK_SHADOW_ETCHED_IN);
    g_signal_connect(thing, "button-press-event", G_CALLBACK(scrolled_window_press), e);
    g_signal_connect(thing, "button-release-event", G_CALLBACK(scrolled_window_release), e);
    g_signal_connect(thing, "motion-notify-event", G_CALLBACK(scrolled_window_motion), e);

    e->hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(thing));
    e->vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(thing));

    gtk_box_pack_start(GTK_BOX(box2), thing, TRUE, TRUE, 0);
    gtk_widget_set_size_request(canvas, 30, 64);
    gtk_container_add(GTK_CONTAINER(thing), canvas);
    gtk_widget_show_all(thing);

    cb[0].data = cb[1].data = cb[2].data = e;
#define MENU_FILE DATADIR "/" PACKAGE "/envelope-box.ui"
    builder = gui_builder_from_file(MENU_FILE, cb);
    menu = gui_get_widget(builder, "env_editor_zoom_menu", MENU_FILE);
    gui_popup_menu_attach(menu, thing, NULL);
    g_object_unref(builder);

#else /* !USE_CANVAS */

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_widget_show(frame);

    gtk_box_pack_start(GTK_BOX(box2), frame, TRUE, TRUE, 0);

    thing = gtk_label_new(_("Graphical\nEnvelope\nEditor\nonly with\nGooCanvas"));
    gtk_widget_show(thing);
    gtk_container_add(GTK_CONTAINER(frame), thing);

#endif /* defined(USE_CANVAS) */

    thing = gtk_vseparator_new();
    gtk_box_pack_start(GTK_BOX(box2), thing, FALSE, TRUE, 0);
    gtk_widget_show(thing);

    /* Sustain / Loop widgets */
    box3 = gtk_vbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box2), box3, FALSE, TRUE, 0);

    e->sustain = thing = gtk_check_button_new_with_label(_("Sustain"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    e->sig_tags[8] = g_signal_connect(thing, "toggled",
        G_CALLBACK(handle_sustain_button), e);

    e->sig_tags[4] = gui_put_labelled_spin_button(box3,
        _("Point"), 0, 11, &e->spin_sustain, handle_spin_sustain, e, TRUE);

    e->loop = thing = gtk_check_button_new_with_label(_("Loop"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), 0);
    gtk_box_pack_start(GTK_BOX(box3), thing, FALSE, TRUE, 0);
    e->sig_tags[9] = g_signal_connect(thing, "toggled",
        G_CALLBACK(handle_loop_button), e);

    e->sig_tags[5] = gui_put_labelled_spin_button(box3,
        _("Start"), 0, 11, &e->spin_loop_start, handle_spin_loop_start, e, TRUE);
    e->sig_tags[6] = gui_put_labelled_spin_button(box3,
        _("End"), 0, 11, &e->spin_loop_end, handle_spin_loop_end, e, TRUE);
    gtk_widget_show_all(box3);

    return GTK_WIDGET(eb);
}

static void
envelope_box_init (EnvelopeBox *self)
{
}

static void
envelope_box_class_init(EnvelopeBoxClass* class)
{
    GObjectClass* object_class = (GObjectClass*)class;

    envelope_box_signals[SIG_COPY_ENV] = g_signal_new("copy_env",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET(EnvelopeBoxClass, copy_env),
        NULL, NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE, 1,
        G_TYPE_POINTER);

    envelope_box_signals[SIG_PASTE_ENV] = g_signal_new("paste_env",
        G_TYPE_FROM_CLASS(object_class),
        (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        G_STRUCT_OFFSET(EnvelopeBoxClass, paste_env),
        NULL, NULL,
        g_cclosure_marshal_VOID__POINTER,
        G_TYPE_NONE, 1,
        G_TYPE_POINTER);
}

G_DEFINE_TYPE(EnvelopeBox, envelope_box, GTK_TYPE_VBOX)
