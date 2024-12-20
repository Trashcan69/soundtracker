/* clavier.h - GTK+ "Clavier" Widget
 * Copyright (C) 1998 Simon Kågedal
 * Copyright (C) 1999-2019 Michael Krause
 * Copyright (C) 2021 Yury Aliaev
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
 * created 1998-04-19
 */

#ifndef __CLAVIER_H__
#define __CLAVIER_H__

#include "customdrawing.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define CLAVIER(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, clavier_get_type(), Clavier))
#define CLAVIER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST(klass, clavier_get_type, ClavierClass))
#define IS_CLAVIER(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, clavier_get_type()))

typedef struct _Clavier Clavier;
typedef struct _ClavierClass ClavierClass;

struct _ClavierClass {
    CustomDrawingClass parent_class;

    void (*clavierkey_press)(Clavier* clavier, gint key);
    void (*clavierkey_release)(Clavier* clavier, gint key);
    void (*clavierkey_enter)(Clavier* clavier, gint key);
    void (*clavierkey_leave)(Clavier* clavier, gint key);
};

GType clavier_get_type(void) G_GNUC_CONST;
GtkWidget* clavier_new(const gchar* font);

void clavier_set_range(Clavier* clavier, gint start, gint end);
void clavier_set_key_labels(Clavier*, gint8*);
void clavier_set_font(Clavier* clavier, const gchar* font);

void clavier_press(Clavier* clavier, gint key);
void clavier_release(Clavier* clavier, gint key);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CLAVIER_H__ */
