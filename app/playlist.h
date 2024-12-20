
/*
 * The Real SoundTracker - gtk+ Playlist widget (header)
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

#ifndef _PLAYLIST_H
#define _PLAYLIST_H

#include <gtk/gtkeventbox.h>

#include "xm.h"

#define PLAYLIST(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, playlist_get_type(), Playlist))
#define PLAYLIST_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass)(klass, playlist_get_type(), PlaylistClass))
#define IS_PLAYLIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, playlist_get_type()))

typedef struct _Playlist Playlist;
typedef struct _PlaylistClass PlaylistClass;
typedef struct _PlaylistPriv PlaylistPriv;

struct _PlaylistClass {
    GtkEventBoxClass parent_class;

    void (*current_position_changed)(Playlist* p, const gint pos);
    void (*entry_changed)(Playlist* p, const gint pat);
    void (*song_length_changed)(Playlist* p, const gint len);
    void (*insert_pattern)(Playlist* p, const gint pos);
    void (*add_copy)(Playlist* p);
};

GType playlist_get_type(void) G_GNUC_CONST;
GtkWidget* playlist_new(GtkWidget* mainwindow);

GtkAdjustment* playlist_get_songpat_adjustment(Playlist* p);

void playlist_freeze(Playlist* p);
void playlist_thaw(Playlist* p);
void playlist_enable(Playlist* p, const gboolean enable);

void playlist_freeze_signals(Playlist* p);
void playlist_thaw_signals(Playlist* p);

void playlist_set_xm(Playlist* p, XM* xm);
gint playlist_get_nth_pattern(Playlist* p, const gint pos);
void playlist_set_pattern_length(Playlist* p, const gint pat, const gint len);

void playlist_set_position(Playlist* p, const gint pos);
gint playlist_get_position(Playlist* p);

void playlist_insert_pattern(Playlist* p,
    const gint pos,
    const gint pat);
void playlist_add_pattern(Playlist* p, const gint copy_from);

#endif /* _PLAYLIST_H */
