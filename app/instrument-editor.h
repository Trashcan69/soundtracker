
/*
 * The Real SoundTracker - instrument editor (header)
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

#ifndef _INSTRUMENT_EDITOR_H
#define _INSTRUMENT_EDITOR_H

#include <gtk/gtk.h>

#include "xm.h"

void instrument_page_create(GtkNotebook* nb);
gboolean instrument_editor_handle_keys(int shift,
    int ctrl,
    int alt,
    guint32 keyval,
    gboolean pressed);
void instrument_editor_indicate_key(const gint channel, const gint key);

void instrument_editor_set_instrument(STInstrument*, const gint ins);
STInstrument* instrument_editor_get_instrument(void);
void instrument_editor_update(gboolean full); /* instrument data has changed, redraw */
gboolean instrument_editor_clear_current_instrument(void);
void instrument_editor_update_clavier(const gchar* font);

void instrument_editor_cut_instrument(STInstrument* instr);
void instrument_editor_copy_instrument(STInstrument* instr);
void instrument_editor_paste_instrument(STInstrument* instr);
void instrument_editor_xcopy_instruments(STInstrument* src_instr,
    STInstrument* dst_instr,
    gboolean xchg);

gboolean instrument_editor_check_and_log_instrument(STInstrument* instr,
    const gchar* title,
    const gint flags,
    gsize i_size);

#endif /* _INSTRUMENT_EDITOR_H */
