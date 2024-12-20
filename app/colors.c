
/*
 * The Real SoundTracker - Color scheme configuration routines
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

#include <glib/gi18n.h>

#include "colors.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "preferences.h"
#include "st-subs.h"

typedef struct {
    const guint r, g, b; /* Default values */
    const gchar *meaning, *tip; /* meaning == NULL also indicates that this color is not configurable */
    GSList *gcs, *widgets;
    gboolean changed;
    GdkColor color, new_color; /* Actual and desired values */
} STColor;

/* This arraly must be consistent with the COLORS_* enum in colors.h */
static STColor colors[] = {
    /* Configurable */
    {10, 20, 30, N_("Background"), N_("Background for tracker and sample dislays"), NULL, NULL, FALSE, {0}, {0}},
    {100, 100, 100, N_("Cursor background"), N_("Background for the tracker cursor line"), NULL, NULL, FALSE, {0}, {0}},
    {70, 70, 70, N_("Major lines"), N_("Major lines highlighting"), NULL, NULL, FALSE, {0}, {0}},
    {50, 50, 50, N_("Minor lines"), N_("Minor lines highlighting"), NULL, NULL, FALSE, {0}, {0}},
    {50, 60, 70, N_("Selection"), N_("Tracker selection"), NULL, NULL, FALSE, {0}, {0}},
    {230, 230, 230, N_("Notes, waveforms"), N_("Tracker text (notes, effects) and waveforms"), NULL, NULL, FALSE, {0}, {0}},
    {170, 170, 200, N_("Delimiters"), N_("Tracker delimiters"), NULL, NULL, FALSE, {0}, {0}},
    {230, 200, 0, N_("Channel numbers, loops"), N_("Channel numbers and loops in the sample editor"), NULL, NULL, FALSE, {0}, {0}},
    {250, 100, 50, N_("Cursor idle"), N_("Tracker cursor in idle mode"), NULL, NULL, FALSE, {0}, {0}},
    {250, 200, 50, N_("Cursor editing"), N_("Tracker cursor in editing mode"), NULL, NULL, FALSE, {0}, {0}},
    {120, 130, 150, N_("Cursor bg in selection"), N_("Background for the tracker cursor in the selection"), NULL, NULL, FALSE, {0}, {0}},
    {230, 0, 0, N_("Mixer position"), N_("Mixer position indicator"), NULL, NULL, FALSE, {0}, {0}},
    {60, 60, 140, N_("Zero line"), N_("Sample display zero line"), NULL, NULL, FALSE, {0}, {0}},
    /* Special */
    {0, 0, 0, N_("White keys"), N_("White clavier keys, text on black keys"), NULL, NULL, FALSE, {0}, {0}},
    {0, 0, 0, N_("Black keys"), N_("Black clavier keys, text on white keys"), NULL, NULL, FALSE, {0}, {0}},
    {0, 0, 0, N_("White keys pressed"), N_("White clavier keys in pressed state"), NULL, NULL, FALSE, {0}, {0}},
    {0, 0, 0, N_("Black keys pressed"), N_("Black clavier keys in pressed state"), NULL, NULL, FALSE, {0}, {0}},
    {0, 0, 0, N_("White text pressed"), N_("Text on white clavier keys in pressed state"), NULL, NULL, FALSE, {0}, {0}},
    {0, 0, 0, N_("Black text pressed"), N_("Text on black clavier keys in pressed state"), NULL, NULL, FALSE, {0}, {0}},
    /* Fixed */
    {255, 0, 0, NULL, NULL, NULL, NULL, FALSE, {0}, {0}}, /* Red */
    {0, 0, 0, NULL, NULL, NULL, NULL, FALSE, {0}, {0}}, /* Black */
    /* Must be consistent with the background color of the "unmuted.png" image */
    {0, 0, 1, NULL, NULL, NULL, NULL, FALSE, {0}, {0}}
};

#define COLORS ARRAY_SIZE(colors)

static GtkWidget* col_samples[COLORS] = {NULL};
static GtkWidget* colorsel = NULL;
static GdkColormap* cm = NULL;
static guint active_col_item = 0;

static void
colors_update_color(const guint i)
{
    GSList* l;
#ifdef BACKEND_X11
    XGCValues vls;
#endif

    for (l = colors[i].gcs; l; l = l->next) {
#ifdef BACKEND_X11
        GC* gc = l->data;

        vls.foreground = colors[i].color.pixel;
        XChangeGC(display, *gc, GCForeground, &vls);
#else
        GdkGC* gc = l->data;

        gdk_gc_set_foreground(gc, &colors[i].color);
#endif
    }
}

static void
colors_special_set_default(GtkWidget* widget, const guint i)
{
    GtkStyle* style = gtk_widget_get_style(widget);

    switch (i) {
    case COLOR_KEY_WHITE:
        colors[i].color = style->bg[GTK_STATE_NORMAL];
        break;
    case COLOR_KEY_BLACK:
        colors[i].color = style->text[GTK_STATE_NORMAL];
        break;
    case COLOR_KEY_WHITE_PRESSED:
        colors[i].color = style->text_aa[GTK_STATE_NORMAL];
        break;
    case COLOR_KEY_BLACK_PRESSED:
        colors[i].color = style->fg[GTK_STATE_INSENSITIVE];
        break;
    case COLOR_TEXT_WHITE_PRESSED:
        colors[i].color = style->base[GTK_STATE_NORMAL];
        break;
    case COLOR_TEXT_BLACK_PRESSED:
        colors[i].color = style->bg[GTK_STATE_NORMAL];
        break;
    default:
        g_assert_not_reached();
    }
    colors_update_color(i);
}

void
colors_init(GtkWidget* widget, const gint action)
{
    guint i;
#ifdef BACKEND_X11
    XGCValues vls;
    DIDrawable drw  = di_get_drawable(widget->window);
#endif

    if (!cm)
        cm = gdk_colormap_get_system();

    for (i = 0; i < COLORS; i++) {
        GSList* l;

        if (action == COLORS_GET_PREFS || action == COLORS_RESET) {
            if (i >= COLOR_SPECIAL && i < COLOR_FIXED)
                colors_special_set_default(widget, i);
            else {
                colors[i].color.red = colors[i].r << 8;
                colors[i].color.green = colors[i].g << 8;
                colors[i].color.blue = colors[i].b << 8;
            }

            if (action == COLORS_GET_PREFS && colors[i].meaning
                && (i < COLOR_SPECIAL || !gui_settings.clavier_colors_gtk))
                    colors[i].color = prefs_get_color("settings", colors[i].meaning, colors[i].color);
        } else { /* COLORS_APPLY */
            if (colors[i].changed) {
                colors[i].color = colors[i].new_color;
                colors[i].changed = FALSE;
            }
        }

        gdk_colormap_alloc_color(cm, &colors[i].color, FALSE, TRUE);

        if (!colors[i].gcs) {
#ifdef BACKEND_X11
            GC* gc = g_new(GC, 1);
            *gc = XCreateGC(display, drw, 0, &vls);
#else
            GdkGC* gc = gdk_gc_new(widget->window);
#endif
            colors[i].gcs = g_slist_prepend(NULL, gc);
        }
        colors_update_color(i);

        for (l = colors[i].widgets; l; l = l->next) {
            GtkWidget* w = l->data;

            if (w)
                gtk_widget_queue_draw(w);
        }
    }
}

void
colors_save(const gchar* section)
{
    guint i;

    for (i = 0; i < COLORS && colors[i].meaning; i++)
        prefs_put_color(section, colors[i].meaning, colors[i].color);
}

GdkColor*
colors_get_color(const guint index)
{
    g_return_val_if_fail(index < COLORS, NULL);

    return &colors[index].color;
}

DIGC
colors_get_gc(const guint index)
{
    g_return_val_if_fail(index < COLORS, NULL);

    /* Attemt to get something from the uninitialized structure */
    if (!colors[index].gcs)
        g_critical("Access to the uninitialized color's manager!");

#ifdef BACKEND_X11
    return *(GC*)colors[index].gcs->data;
#else
    return colors[index].gcs->data;
#endif
}

DIGC
colors_new_gc(const guint index, GdkWindow* window)
{
#ifdef BACKEND_X11
    XGCValues vls;
    GC* gc = g_new(GC, 1);
#else
    GdkGC* gc;
#endif

    g_return_val_if_fail(index < COLORS, NULL);

#ifdef BACKEND_X11
    vls.foreground = colors[index].color.pixel;
    *gc = XCreateGC(display, di_get_drawable(window), GCForeground, &vls);
#else
    gc = gdk_gc_new(window);
    gdk_gc_set_foreground(gc, &colors[index].color);
#endif
    colors[index].gcs = g_slist_prepend(colors[index].gcs, gc);

#ifdef BACKEND_X11
    return *gc;
#else
    return gc;
#endif
}

static void
col_sample_paint(GtkWidget* widget, GdkColor* color)
{
#ifdef BACKEND_X11
    GC gc;
    XGCValues vls;
    XID drw = di_get_drawable(widget->window);
#else
    static GdkGC* gc = NULL;
#endif

#ifdef BACKEND_X11
    vls.foreground = color->pixel;
    gc = XCreateGC(display, drw, GCForeground, &vls);
    XFillRectangle(display, drw, gc, 0, 0, widget->allocation.width, widget->allocation.height);
#else
    if (!gc)
        gc = gdk_gc_new(widget->window);

    gdk_gc_set_foreground(gc, color);
    gdk_draw_rectangle(widget->window, gc, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);
#endif
}

static gboolean
col_sample_expose(GtkWidget* widget, GdkEvent* event, gpointer data)
{
    gint n = GPOINTER_TO_INT(data);

    col_sample_paint(widget, colors[n].changed ? &colors[n].new_color : &colors[n].color);
    return TRUE;
}

static void
colors_dialog_response(GtkWidget* dialog, gint response)
{
    guint i;

    switch (response) {
    case GTK_RESPONSE_APPLY:
        /* Should be mainwindow, but really no difference */
        colors_init(dialog, COLORS_APPLY);
        break;
    case GTK_RESPONSE_REJECT:
        colors_init(dialog, COLORS_RESET);
        for (i = 0; i < COLORS && colors[i].meaning; i++)
            col_sample_paint(col_samples[i], &colors[i].color);
        gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &colors[active_col_item].color);
        break;
    default:
        gtk_widget_hide(dialog); /* Then falling down */
    case GTK_RESPONSE_CANCEL:
        for (i = 0; i < COLORS && colors[i].meaning; i++) {
            col_sample_paint(col_samples[i], &colors[i].color);
            if (colors[i].changed)
                colors[i].changed = FALSE;
        }
        gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &colors[active_col_item].color);
        break;
    }
}

static void
col_item_toggled(GtkToggleButton* butt, gpointer data)
{
    if (gtk_toggle_button_get_active(butt)) {
        gint n = active_col_item = GPOINTER_TO_INT(data);
        gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel),
            colors[n].changed ? &colors[n].new_color : &colors[n].color);
    }
}

static void
color_changed(void)
{
    gtk_color_selection_get_current_color(GTK_COLOR_SELECTION(colorsel), &colors[active_col_item].new_color);
    gdk_colormap_alloc_color(cm, &colors[active_col_item].new_color, FALSE, TRUE);
    col_sample_paint(col_samples[active_col_item], &colors[active_col_item].new_color);
    colors[active_col_item].changed = TRUE;
}

static void
clavier_gtk_toggled(GtkToggleButton* tb, GtkWidget* cl_col[])
{
    guint i;
    gboolean active = gtk_toggle_button_get_active(tb);

    gui_settings.clavier_colors_gtk = active;
    for (i = COLOR_SPECIAL; i < COLOR_FIXED; i++) {
        gtk_widget_set_sensitive(cl_col[i - COLOR_SPECIAL], !active);
        if (active) {
            colors_special_set_default(cl_col[i - COLOR_SPECIAL], i);
            colors[i].changed = FALSE;
            col_sample_paint(col_samples[i], &colors[i].color);
            if (i == active_col_item)
                gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &colors[i].color);
        }
    }
}

static void
colors_special_init_all(GtkWidget* widget)
{
    if (gui_settings.clavier_colors_gtk) {
        static GtkStyle* prev_style = NULL;
        GtkStyle* new_style = gtk_widget_get_style(widget);

        /* Reduce overhead if several widgets use colors from Gtk+ theme */
        if (prev_style != new_style) {
            guint i;

            prev_style = new_style;

            for (i = COLOR_SPECIAL; i < COLOR_FIXED; i++) {
                colors_special_set_default(widget, i);
                if (col_samples[i])
                    col_sample_paint(col_samples[i], &colors[i].color);
                if (colorsel && i == active_col_item)
                    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &colors[i].color);
            }
        }
    }
}

void
colors_add_widget(const guint index, GtkWidget* w)
{
    g_return_if_fail(index < COLORS);

    colors[index].widgets = g_slist_prepend(colors[index].widgets, w);
    /* For colors taken from Gtk+ theme we should reinitialize them
       on theme change */
    if (index >= COLOR_SPECIAL && index < COLOR_FIXED)
        g_signal_connect(w, "style-set",
            G_CALLBACK(colors_special_init_all), NULL);
}

void
colors_dialog(GtkWidget* window)
{
    static GtkWidget* dialog = NULL;
    static GtkWidget* clavier_colors[COLOR_FIXED - COLOR_SPECIAL];
    static guint id;

    GtkWidget *thing, *radio = NULL, *hbox, *vbox, *mainhbox, *c_area;
    guint i;

    if (!dialog) {
        gchar* buf;

        buf = g_strdup_printf(_("%s colors configuration"), PACKAGE_NAME);
        dialog = gtk_dialog_new_with_buttons(buf, GTK_WINDOW(window),
            GTK_DIALOG_DESTROY_WITH_PARENT,
            _("Reset"), GTK_RESPONSE_REJECT,
            GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
        g_free(buf);
        gui_dialog_adjust(dialog, GTK_RESPONSE_APPLY);
        gtk_widget_set_tooltip_text(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT),
            _("Reset the color scheme to standard"));
        gtk_widget_set_tooltip_text(gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL),
            _("Reset colors to the latest set values"));
        gui_dialog_connect(dialog, G_CALLBACK(colors_dialog_response));

        mainhbox = gtk_hbox_new(FALSE, 0);
        vbox = gtk_vbox_new(TRUE, 2);
        for (i = 0; i < COLORS && colors[i].meaning; i++) {
            if (i == COLOR_SPECIAL) {
                thing = gtk_check_button_new_with_label(_("Gtk clavier colors"));
                gtk_widget_set_tooltip_text(thing,
                    _("Use colors from the current Gtk+ theme to paint the clavier"));
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(thing), gui_settings.clavier_colors_gtk);
                gtk_box_pack_start(GTK_BOX(vbox), thing, FALSE, FALSE, 0);
                g_signal_connect(thing, "toggled",
                    G_CALLBACK(clavier_gtk_toggled), clavier_colors);
            }
            radio = i ? gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(radio))
                      : gtk_radio_button_new(NULL);
            gtk_box_pack_start(GTK_BOX(vbox), radio, FALSE, FALSE, 0);
            g_signal_connect(radio, "toggled",
                G_CALLBACK(col_item_toggled), GINT_TO_POINTER(i));

            hbox = gtk_hbox_new(FALSE, 4);
            col_samples[i] = gtk_drawing_area_new();
            di_widget_configure(col_samples[i]);
            gtk_box_pack_start(GTK_BOX(hbox), col_samples[i], FALSE, FALSE, 0);
            g_signal_connect(col_samples[i], "expose_event",
                G_CALLBACK(col_sample_expose), GINT_TO_POINTER(i));

            thing = gtk_label_new(_(colors[i].meaning));
            gtk_widget_set_tooltip_text(thing, _(colors[i].tip));
            gtk_box_pack_start(GTK_BOX(hbox), thing, FALSE, FALSE, 0);
            if (i >= COLOR_SPECIAL) {
                gtk_widget_set_sensitive(radio, !gui_settings.clavier_colors_gtk);
                clavier_colors[i - COLOR_SPECIAL] = radio;
            }
            gtk_container_add(GTK_CONTAINER(radio), hbox);
        }
        gtk_box_pack_start(GTK_BOX(mainhbox), vbox, FALSE, FALSE, 0);

        thing = gtk_vseparator_new();
        gtk_box_pack_start(GTK_BOX(mainhbox), thing, FALSE, FALSE, 4);

        colorsel = gtk_color_selection_new();
        id = g_signal_connect(colorsel, "color_changed",
            G_CALLBACK(color_changed), NULL);
        gtk_box_pack_start(GTK_BOX(mainhbox), colorsel, FALSE, FALSE, 0);

        c_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
        gtk_box_pack_start(GTK_BOX(c_area), mainhbox, TRUE, TRUE, 0);

        thing = gtk_hseparator_new();
        gtk_box_pack_start(GTK_BOX(c_area), thing, FALSE, FALSE, 4);
        gtk_widget_show_all(c_area);

        gtk_widget_realize(dialog);

        for (i = 0; i < COLORS && colors[i].meaning; i++)
            gtk_widget_set_size_request(col_samples[i], radio->allocation.height * 2, radio->allocation.height);
    }

    g_signal_handler_block(colorsel, id);
    gtk_color_selection_set_current_color(GTK_COLOR_SELECTION(colorsel), &colors[active_col_item].color);
    g_signal_handler_unblock(colorsel, id);

    gtk_window_present(GTK_WINDOW(dialog));
    for (i = 0; i < COLORS && colors[i].meaning; i++)
        col_sample_paint(col_samples[i], &colors[i].color);
}
