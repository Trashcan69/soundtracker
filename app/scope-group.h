
/*
 * The Real SoundTracker - main window oscilloscope group (header)
 *
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

#ifndef _SCOPE_GROUP_H
#define _SCOPE_GROUP_H

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "sample-display.h"

#define SCOPE_GROUP(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, scope_group_get_type(), ScopeGroup))
#define SCOPE_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST(klass, scope_group_get_type(), ScopeGroupClass))
#define IS_SCOPE_GROUP(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, scope_group_get_type()))

typedef struct _ScopeGroup ScopeGroup;
typedef struct _ScopeGroupClass ScopeGroupClass;

typedef struct _ScopeGroupPriv ScopeGroupPriv;

struct _ScopeGroup {
    GtkHBox hbox;

    ScopeGroupPriv* priv;
};

struct _ScopeGroupClass {
    GtkHBoxClass parent_class;
};

GType scope_group_get_type(void) G_GNUC_CONST;

GtkWidget*
scope_group_new(GdkPixbuf* mutedpic, GdkPixbuf* unmutedpic);

void scope_group_set_num_channels(ScopeGroup* s, int num_channels);
void scope_group_set_mode(ScopeGroup* s, SampleDisplayMode mode);
void scope_group_enable_scopes(ScopeGroup* s, int enable);
void scope_group_set_update_freq(ScopeGroup* s, int freq);

void scope_group_update_channel_status(ScopeGroup* s,
    const gint channel,
    const gint inst,
    const gint smpl);
void scope_group_timeout(ScopeGroup* s, gdouble time1);
void scope_group_stop_updating(ScopeGroup* s);

void scope_group_set_update_freq(ScopeGroup* s, int freq);
void scope_group_toggle_channel(ScopeGroup *s, const guint n);
void scope_group_solo_channel(ScopeGroup *s, const guint n);
void scope_group_all_channels_on(ScopeGroup *g);

#endif /* _SCOPE_GROUP_H */
