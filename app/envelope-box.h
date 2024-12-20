
/*
 * The Real SoundTracker - GTK+ envelope editor box (header)
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

#ifndef _ENVELOPE_BOX_H
#define _ENVELOPE_BOX_H

#include <gtk/gtk.h>

#include "xm.h"

#define ENVELOPE_BOX(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, envelope_box_get_type(), EnvelopeBox))
#define ENVELOPE_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST(klass, envelope_box_get_type(), EnvelopeBoxClass))
#define IS_ENVELOPE_BOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, envelope_box_get_type()))

typedef struct _EnvelopeBox EnvelopeBox;
typedef struct _EnvelopeBoxClass EnvelopeBoxClass;
typedef struct _EnvelopeBoxPrivate EnvelopeBoxPrivate;

struct _EnvelopeBox {
    GtkVBox vbox;

    EnvelopeBoxPrivate* priv;
};

struct _EnvelopeBoxClass {
    GtkVBoxClass parent_class;
    void (*copy_env)(EnvelopeBox*, STEnvelope*);
    void (*paste_env)(EnvelopeBox*, STEnvelope*);
};

GType envelope_box_get_type(void) G_GNUC_CONST;
GtkWidget* envelope_box_new(const gchar* label);

void envelope_box_set_envelope(EnvelopeBox* e, STEnvelope* env);
void envelope_box_log_action(EnvelopeBox* e,
    const gchar* name,
    const gint extra_flags);

#endif /* _ENVELOPE_BOX_H */
