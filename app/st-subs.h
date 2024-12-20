
/*
 * The Real SoundTracker - General support routines (header)
 *
 * Copyright (C) 1998-2019 Michael Krause
 * Copyright (C) 2023 Yury Aliaev
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

#ifndef _ST_SUBS_H
#define _ST_SUBS_H

#include "xm.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

typedef enum {
    ST_FLAGS_NOTE = 1,
    ST_FLAGS_INSTR = 1 << 1,
    ST_FLAGS_VOLUME = 1 << 2,
    ST_FLAGS_VOLEFF = 1 << 3,
    ST_FLAGS_FXTYPE = 1 << 4,
    ST_FLAGS_FXPARAM = 1 << 5,
    ST_FLAGS_UNUSED = 1 << 6
}
STForEachNoteFlags;

/* --- Module functions --- */
void st_free_all_pattern_channels(XM* xm);
int st_init_pattern_channels(XMPattern* p, unsigned length, int num_channels);
int st_num_save_instruments(XM* xm);
int st_num_save_patterns(XM* xm);
void st_clean_song(XM*);
void st_set_num_channels(XM*, int);
void st_for_each_note(XM* xm,
    gboolean (*foreach_func)(XM*, XMNote*, const gint ins, gpointer),
    STForEachNoteFlags flags,
    gpointer user_data);


/* --- Pattern functions --- */
void st_free_pattern_channels(XMPattern* pat);
void st_clear_pattern(XMPattern*);
int st_copy_pattern(XMPattern* dst,
    XMPattern* src);
void st_pattern_delete_track(XMPattern*, int);
void st_pattern_insert_track(XMPattern*, int);
void st_pattern_delete_line(XMPattern*, int);
void st_pattern_insert_line(XMPattern*, int);
gboolean st_is_empty_pattern(XMPattern* p);
int st_find_first_unused_and_empty_pattern(XM* xm);
gboolean st_is_pattern_used_in_song(XM* xm,
    int patnum);
void st_set_alloc_length(XMPattern*, const gint, const gboolean);

static inline void st_set_pattern_length(XMPattern* pat,
    int l)
{
    if (pat->alloc_length < l)
        st_set_alloc_length(pat, l, TRUE);
    pat->length = l;
}

void st_exchange_patterns(XM* xm,
    int p1,
    int p2);
gboolean st_check_if_odd_are_not_empty(XMPattern* p);
void st_shrink_pattern(XMPattern* p);
void st_expand_pattern(XMPattern* p);

/* --- Track functions --- */
void st_clear_track(XMNote*, int length);
gboolean st_is_empty_track(XMNote* notes,
    int length);

/* --- Instrument functions --- */
int st_instrument_num_samples(STInstrument* i);
int st_instrument_num_save_samples(STInstrument* instr);
gsize st_get_instrument_size(STInstrument* instr);
void st_clean_instrument_full2(STInstrument* i,
    const char* name,
    const gboolean modify_ins_name,
    const gboolean modify_smp_names);
#define st_clean_instrument_full(i, name, modify_name) st_clean_instrument_full2(i, name, modify_name, modify_name)
#define st_clean_instrument(i, name) st_clean_instrument_full(i, name, TRUE)
void st_copy_instrument_full(STInstrument* src,
    STInstrument* dest,
    const gboolean ins_name_overwrite,
    const gboolean smp_name_overwrite);
#define st_copy_instrument(src, dest) st_copy_instrument_full(src, dest, TRUE, TRUE)
gboolean st_instrument_used_in_song(XM* xm, int instr);

/* --- Sample functions --- */
void st_clean_sample_full(STSample* s, const char* utf_name, const char* name, const gboolean clear_name);
#define st_clean_sample(s, utf_name, name) st_clean_sample_full(s, utf_name, name, TRUE)
void st_copy_sample_full(STSample* src_smp, STSample* dest_smp, const gboolean name_overwrite);
#define st_copy_sample(src, dest) st_copy_sample_full(src, dest, TRUE)
void st_set_sample_name(STSample* s, const char* name);
void st_sample_fix_loop(STSample* s);
void st_convert_sample(const void* src,
    void* dst,
    const gint srcformat,
    const gint dstformat,
    const gsize count,
    const gboolean src_be,
    const gboolean dst_be);
void st_sample_8bit_signed_unsigned(gint8* data,
    int count);
void st_sample_16bit_signed_unsigned(gint16* data,
    int count);
/* Convert between interleaved and non-interleaved stereo */
void st_convert_to_nonint(gint16* src,
    gint16* dest,
    const gsize offset,
    const gsize frames);
void st_convert_to_int(gint16* src,
    gint16* dest,
    const gsize offset,
    const gsize frames);

#endif /* _ST_SUBS_H */
