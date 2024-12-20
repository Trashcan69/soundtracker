
/*
 * The Real SoundTracker - Color scheme configuration routines (header)
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

#ifndef __COLORS_H
#define __COLORS_H

#include "draw-interlayer.h"

enum {
    /* Configurable colors must go first */
    COLOR_BG = 0,
    COLOR_BG_CURSOR,
    COLOR_BG_MAJHIGH,
    COLOR_BG_MINHIGH,
    COLOR_BG_SELECTION,
    COLOR_NOTES,
    COLOR_BARS,
    COLOR_CHANNUMS,
    COLOR_CURSOR,
    COLOR_CURSOR_EDIT,
    COLOR_BG_CURSOR_SEL,
    COLOR_MIXERPOS,
    COLOR_ZEROLINE,
    /* Special-purpose configurable colors, all
       general-purpose colors should go before them */
    COLOR_SPECIAL,
    COLOR_KEY_WHITE = COLOR_SPECIAL,
    COLOR_TEXT_BLACK = COLOR_KEY_WHITE,
    COLOR_KEY_BLACK,
    COLOR_TEXT_WHITE = COLOR_KEY_BLACK,
    COLOR_KEY_WHITE_PRESSED,
    COLOR_KEY_BLACK_PRESSED,
    COLOR_TEXT_WHITE_PRESSED,
    COLOR_TEXT_BLACK_PRESSED,
    /* Than go fixed colors */
    COLOR_FIXED,
    COLOR_RED = COLOR_FIXED,
    COLOR_BLACK,
    COLOR_UNMUTED_BG
};

enum {
    COLORS_APPLY,
    COLORS_RESET,
    COLORS_GET_PREFS,
};

void colors_init(GtkWidget* widget, const gint action);
void colors_save(const gchar* section);

/* Gets the default GC */
DIGC colors_get_gc(const guint index);
/* Creates a new GC related to the indexed color */
DIGC colors_new_gc(const guint index, GdkWindow* window);

GdkColor* colors_get_color(const guint index);
void colors_add_widget(const guint index, GtkWidget* widget);
void colors_dialog(GtkWidget* window);

#endif /* __COLORS_H */
