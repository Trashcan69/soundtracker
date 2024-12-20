
/*
 * The Real SoundTracker - General support routines
 *
 * Copyright (C) 1998-2019 Michael Krause
 * Copyright (C) 2020-2023 Yury Aliaev
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

#include <pthread.h>

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "st-subs.h"
#include "xm.h"

int st_init_pattern_channels(XMPattern* p,
    unsigned length,
    int num_channels)
{
    int i;

    p->length = p->alloc_length = length;
    for (i = 0; i < num_channels; i++) {
        if (!(p->channels[i] = calloc(1, length * sizeof(XMNote)))) {
            st_free_pattern_channels(p);
            return 0;
        }
    }

    return 1;
}

void st_free_pattern_channels(XMPattern* pat)
{
    int i;

    for (i = 0; i < 32; i++) {
        free(pat->channels[i]);
        pat->channels[i] = NULL;
    }
}

void st_free_all_pattern_channels(XM* xm)
{
    int i;

    for (i = 0; i < 256; i++)
        st_free_pattern_channels(&xm->patterns[i]);
}

static XMNote*
st_dup_track(XMNote* n,
    int length)
{
    XMNote* r;

    if (!n)
        return NULL;

    r = malloc(length * sizeof(XMNote));
    if (r) {
        memcpy(r, n, length * sizeof(XMNote));
    }

    return r;
}

int st_copy_pattern(XMPattern* dst,
    XMPattern* src)
{
    XMNote* oldchans[32];
    int i;

    for (i = 0; i < 32; i++) {
        oldchans[i] = dst->channels[i];
        if (src->channels[i]) {
            if (!(dst->channels[i] = st_dup_track(src->channels[i], src->length))) {
                // Out of memory, restore previous pattern, return error
                for (; i >= 0; i--) {
                    free(dst->channels[i]);
                    dst->channels[i] = oldchans[i];
                }
                return 0;
            }
        } else {
            dst->channels[i] = NULL;
        }
    }

    for (i = 0; i < 32; i++)
        free(oldchans[i]);

    dst->length = dst->alloc_length = src->length;

    return 1;
}

void st_clear_track(XMNote* n,
    int length)
{
    if (!n)
        return;

    memset(n, 0, length * sizeof(*n));
}

void st_clear_pattern(XMPattern* p)
{
    int i;

    for (i = 0; i < 32; i++) {
        st_clear_track(p->channels[i], p->alloc_length);
    }
}

void st_pattern_delete_track(XMPattern* p,
    int t)
{
    int i;
    XMNote* a;

    a = p->channels[t];
    g_assert(a != NULL);

    for (i = t; i < 31 && p->channels[i + 1] != NULL; i++) {
        p->channels[i] = p->channels[i + 1];
    }

    p->channels[i] = a;
    st_clear_track(a, p->alloc_length);
}

void st_pattern_insert_track(XMPattern* p,
    int t)
{
    int i;
    XMNote* a;

    a = p->channels[31];

    for (i = 31; i > t; i--) {
        p->channels[i] = p->channels[i - 1];
    }

    if (!a) {
        if (!(a = calloc(1, p->alloc_length * sizeof(XMNote)))) {
            g_assert_not_reached();
        }
    }

    p->channels[t] = a;
    st_clear_track(a, p->alloc_length);
}

gboolean
st_instrument_used_in_song(XM* xm,
    int instr)
{
    int i, j, k;
    XMPattern* p;
    XMNote* c;

    for (i = 0; i < xm->song_length; i++) {
        p = &xm->patterns[(int)xm->pattern_order_table[i]];
        for (j = 0; j < xm->num_channels; j++) {
            c = p->channels[j];
            for (k = 0; k < p->length; k++) {
                if (c[k].instrument == instr)
                    return TRUE;
            }
        }
    }

    return FALSE;
}

int st_instrument_num_samples(STInstrument* instr)
{
    int i, n;

    for (i = 0, n = 0; i < XM_NUM_SAMPLES; i++) {
        if (instr->samples[i].sample.length != 0)
            n++;
    }
    return n;
}

int st_instrument_num_save_samples(STInstrument* instr)
{
    int i, n;

    for (i = 0, n = 0; i < XM_NUM_SAMPLES; i++) {
        if (instr->samples[i].sample.length != 0 || instr->samples[i].utf_name[0] != 0)
            n = i + 1;
    }
    return n;
}

gsize
st_get_instrument_size(STInstrument* instr)
{
    gint i;
    gsize s;

    for (i = 0, s = 0; i < XM_NUM_SAMPLES; i++) {
        /* Length in samples which are internally in 2-bytes format */
        gsize samsize = instr->samples[i].sample.length * sizeof(instr->samples[i].sample.data[0]);

        if (instr->samples[i].sample.flags & ST_SAMPLE_STEREO)
            samsize <<= 1;
        s += samsize;
    }

    return s + sizeof(STInstrument);
}

int st_num_save_instruments(XM* xm)
{
    int i, n;

    for (i = 0, n = 0; i < XM_NUM_INSTRUMENTS; i++) {
        if (st_instrument_num_save_samples(&xm->instruments[i]) != 0 || xm->instruments[i].utf_name[0] != 0)
            n = i + 1;
    }
    return n;
}

int st_num_save_patterns(XM* xm)
{
    int i, n;

    for (i = 0, n = 0; i < 256; i++) {
        if (!st_is_empty_pattern(&xm->patterns[i]))
            n = i;
    }

    return n + 1;
}

void st_copy_instrument_full(STInstrument* src,
    STInstrument* dest,
    const gboolean ins_name_overwrite,
    const gboolean smp_name_overwrite)
{
    int i;

    st_clean_instrument_full2(dest, NULL, ins_name_overwrite, smp_name_overwrite);

    if (ins_name_overwrite)
        memcpy(dest, src, offsetof(STInstrument, samples));
    else
        /* Looks a little bit dirty. We want to skip the beginning of the instrument
           where all name-related stuff is located */
        memcpy(&dest->vol_env, &src->vol_env,
            offsetof(STInstrument, samples) - offsetof(STInstrument, vol_env));
    for (i = 0; i < XM_NUM_SAMPLES; i++)
        st_copy_sample_full(&src->samples[i], &dest->samples[i], smp_name_overwrite);
}

void st_clean_instrument_full2(STInstrument* instr,
    const char* name,
    const gboolean modify_ins_name,
    const gboolean modify_smp_name)
{
    int i;

    for (i = 0; i < XM_NUM_SAMPLES; i++)
        st_clean_sample_full(&instr->samples[i], NULL, NULL, modify_smp_name);

    if (modify_ins_name) {
        if (!name) {
            memset(instr->name, 0, sizeof(instr->name));
            memset(instr->utf_name, 0, sizeof(instr->utf_name));
            instr->needs_conversion = FALSE;
        } else {
            g_utf8_strncpy(instr->utf_name, name, sizeof(instr->name) - 1);
            instr->needs_conversion = TRUE;
        }
    }
    memset(instr->samplemap, 0, sizeof(instr->samplemap));
    memset(&instr->vol_env, 0, sizeof(instr->vol_env));
    memset(&instr->pan_env, 0, sizeof(instr->pan_env));
    instr->vol_env.num_points = 1;
    instr->vol_env.points[0].val = 64;
    instr->pan_env.num_points = 1;
    instr->pan_env.points[0].val = 32;
}

void st_set_sample_name(STSample* s,
    const char* name)
{
    g_utf8_strncpy(s->utf_name, name, sizeof(s->name) - 1);
    s->needs_conversion = TRUE;
}

void st_clean_sample_full(STSample* s,
    const char* utf_name, const char* name,
    const gboolean clear_name)
{
    free(s->sample.data);
    s->sample.data = NULL;
    s->sample.flags = ST_SAMPLE_16_BIT;
    s->sample.length = 0;
    s->sample.loopend = 1;
    s->sample.loopstart = 0;
    s->volume = 0;
    s->finetune = 0;
    s->panning = 0;
    s->relnote = 0;

    if (clear_name) {
        if (utf_name) {
            g_utf8_strncpy(s->utf_name, utf_name, sizeof(s->name) - 1);
            if (!name)
                s->needs_conversion = TRUE;
            else {
                strncpy(s->name, name, sizeof(s->name) - 1);
                s->needs_conversion = FALSE;
            }
        } else {
            memset(s->utf_name, 0, sizeof(s->utf_name));
            memset(s->name, 0, sizeof(s->name));
            s->needs_conversion = FALSE;
        }
    }
}

void st_copy_sample_full(STSample* src_smp, STSample* dest_smp, const gboolean name_overwrite)
{
    guint32 length;
    GMutex lock = dest_smp->sample.lock;

    if (dest_smp->sample.data)
        g_free(dest_smp->sample.data);
    if (name_overwrite)
        memcpy(dest_smp, src_smp, sizeof(STSample));
    else {
        dest_smp->sample = src_smp->sample;
        dest_smp->volume = src_smp->volume;
        dest_smp->finetune = src_smp->finetune;
        dest_smp->panning = src_smp->panning;
        dest_smp->relnote = src_smp->relnote;
    }
    dest_smp->sample.lock = lock;
    if ((length = sizeof(dest_smp->sample.data[0]) * dest_smp->sample.length)) {
        if (dest_smp->sample.flags & ST_SAMPLE_STEREO)
            length <<= 1;
        dest_smp->sample.data = malloc(length);
        memcpy(dest_smp->sample.data, src_smp->sample.data, length);
    }
}

void st_clean_song(XM* xm)
{
    int i;

    st_free_all_pattern_channels(xm);

    xm->song_length = 1;
    xm->restart_position = 0;
    xm->num_channels = 8;
    xm->tempo = 6;
    xm->bpm = 125;
    xm->flags = 0;
    memset(xm->pattern_order_table, 0, sizeof(xm->pattern_order_table));

    for (i = 0; i < 256; i++)
        st_init_pattern_channels(&xm->patterns[i], 64, xm->num_channels);
}

void st_set_num_channels(XM* xm,
    int n)
{
    int i, j;
    XMPattern* pat;

    for (i = 0; i < XM_NUM_PATTERNS; i++) {
        pat = &xm->patterns[i];
        for (j = 0; j < n; j++) {
            if (!pat->channels[j]) {
                pat->channels[j] = calloc(1, sizeof(XMNote) * pat->alloc_length);
            }
        }
    }

    xm->num_channels = n;
}

void st_set_alloc_length(XMPattern* pat,
    const gint l,
    const gboolean need_copy)
{
    if (l > pat->alloc_length) {
        int i;
        XMNote* n;

        for (i = 0; i < 32 && pat->channels[i] != NULL; i++) {
            n = calloc(l, sizeof(XMNote));
            if (need_copy)
                memcpy(n, pat->channels[i], sizeof(XMNote) * pat->alloc_length);
            free(pat->channels[i]);
            pat->channels[i] = n;
        }
        pat->alloc_length = l;
    }
}

void st_sample_fix_loop(STSample* sts)
{
    st_mixer_sample_info* s = &sts->sample;

    if (s->loopend > s->length)
        s->loopend = s->length;
    else if (!s->loopend)
        s->loopend = 1;
    if (s->loopstart >= s->loopend)
        s->loopstart = s->loopend - 1;
}

int st_find_first_unused_and_empty_pattern(XM* xm)
{
    int i;

    for (i = 0; i < XM_NUM_PATTERNS; i++) {
        if (!st_is_pattern_used_in_song(xm, i) && st_is_empty_pattern(&xm->patterns[i])) {
            return i;
        }
    }

    return -1;
}

gboolean
st_is_empty_pattern(XMPattern* p)
{
    int i;

    for (i = 0; i < 32; i++) {
        if (p->channels[i]) {
            if (!st_is_empty_track(p->channels[i], p->length)) {
                return 0;
            }
        }
    }

    return 1;
}

gboolean
st_is_empty_track(XMNote* notes,
    int length)
{
    for (; length > 0; length--, notes++) {
        if (notes->note != 0 || notes->instrument != 0 || notes->volume != 0
            || notes->fxtype != 0 || notes->fxparam != 0) {
            return 0;
        }
    }

    return 1;
}

gboolean
st_is_pattern_used_in_song(XM* xm,
    int patnum)
{
    int i;

    for (i = 0; i < xm->song_length; i++)
        if (xm->pattern_order_table[i] == patnum)
            return 1;

    return 0;
}

void st_exchange_patterns(XM* xm,
    int p1,
    int p2)
{
    XMPattern tmp;
    int i;

    memcpy(&tmp, &xm->patterns[p1], sizeof(XMPattern));
    memcpy(&xm->patterns[p1], &xm->patterns[p2], sizeof(XMPattern));
    memcpy(&xm->patterns[p2], &tmp, sizeof(XMPattern));

    for (i = 0; i < xm->song_length; i++) {
        if (xm->pattern_order_table[i] == p1)
            xm->pattern_order_table[i] = p2;
        else if (xm->pattern_order_table[i] == p2)
            xm->pattern_order_table[i] = p1;
    }
}

void
st_pattern_delete_line(XMPattern* p, int l)
{
    int i, j;

    if (l == p->length - 1) /* Don't remove the last line */
        return;
    for (i = 0; i < 32 && p->channels[i]; i++) {
        for (j = l; j < p->alloc_length - 1; j++)
            p->channels[i][j] = p->channels[i][j + 1];
        memset(&p->channels[i][p->alloc_length - 1], 0, sizeof(XMNote));
    }
}

void
st_pattern_insert_line(XMPattern* p, int l)
{
    int i, j;

    if (p->alloc_length < 256)
        st_set_alloc_length(p, p->alloc_length + 1, TRUE);
    if (l < 255) { /* 255 -- last line, do nothing */
        for (i = 0; i < 32 && p->channels[i]; i++) {
            for (j = p->alloc_length - 1; j > l; j--)
                p->channels[i][j] = p->channels[i][j - 1];
            memset(&p->channels[i][l], 0, sizeof(XMNote));
        }
    }
}

gboolean
st_check_if_odd_are_not_empty(XMPattern* p)
{
    int i, j;

    for (i = 0; i < 32 && p->channels[i]; i++)
        for (j = 1; j < p->length; j += 2)
            if ((p->channels[i][j].note && p->channels[i][j].instrument) || (p->channels[i][j].volume > 15) || p->channels[i][j].fxtype || p->channels[i][j].fxparam)
                return TRUE;

    return FALSE;
}

void st_shrink_pattern(XMPattern* p)
{
    int i, j, length = p->length;

    for (i = 0; i < 32 && p->channels[i]; i++) {
        for (j = 1; j <= (length - 1) / 2; j++)
            memcpy(&p->channels[i][j], &p->channels[i][2 * j], sizeof(XMNote));
        for (; j < p->alloc_length; j++) { /* clear the rest of the pattern */
            p->channels[i][j].note = 0;
            p->channels[i][j].instrument = 0;
            p->channels[i][j].volume = 0;
            p->channels[i][j].fxtype = 0;
            p->channels[i][j].fxparam = 0;
        }
    }

    st_set_pattern_length(p, (length - 1) / 2 + 1);
}

void st_expand_pattern(XMPattern* p)
{
    int i, j, length = MIN(p->length * 2, 256);

    st_set_pattern_length(p, length);

    for (i = 0; i < 32 && p->channels[i]; i++) {
        for (j = length / 2 - 1; j >= 0; j--) {
            /* copy to even positions and clear odd */
            memcpy(&p->channels[i][2 * j], &p->channels[i][j], sizeof(XMNote));
            p->channels[i][2 * j + 1].note = 0;
            p->channels[i][2 * j + 1].instrument = 0;
            p->channels[i][2 * j + 1].volume = 0;
            p->channels[i][2 * j + 1].fxtype = 0;
            p->channels[i][2 * j + 1].fxparam = 0;
        }
    }
}

void st_convert_sample(const void* src,
    void* dst,
    const gint srcformat,
    const gint dstformat,
    const gsize count,
    const gboolean src_be,
    const gboolean dst_be)
{
    const gint8 *src8 = src;
    gint8 *dst8 = dst;
    gint swidth = (srcformat & 0xff) >> 3, dwidth = dstformat >> 3;
    gint diff, i, j;

    g_assert(!((srcformat & 7) | (dstformat & 7)));

    if (srcformat == dstformat && src_be == dst_be) {
        memcpy(dst, src, count * swidth);
    } else if (dwidth > swidth) {
        diff = dwidth - swidth;
        for (i = 0; i < count; i++) {
            j = 0;
            if (!dst_be)
                for (; j < diff; j++)
                    *dst8++ = 0;

            if (src_be == dst_be)
                for (; j < dwidth; j++)
                    *dst8++ = *src8++;
            else {
                for (j = 1; j <= swidth; j++)
                    *dst8++ = src8[swidth - j];
                src8 += swidth;
            }

            if (dst_be)
                for (j = 0; j < diff; j++)
                    *dst8++ = 0;
        }
    } else {
        diff = swidth - dwidth;
        if (!src_be)
            src8 += diff;

        /* Special case of 24-bit integer stored into 32-bit frame.
           9th bit marks "true" (packed) 24-bit format */
        if (swidth == 3 && !(srcformat & 256)) {
            diff++;
            if (src_be)
                src8++;
        }
        if (src_be == dst_be)
            for (i = 0; i < count; i++, src8 += diff) {
                for (j = 0; j < dwidth; j++)
                    *dst8++ = *src8++;
            }
        else
            for (i = 0; i < count; i++, src8 += diff) {
                for (j = 1; j <= dwidth; j++)
                    dst8[dwidth - j] = *src8++;
                dst8 += dwidth;
            }
    }
}

void st_sample_8bit_signed_unsigned(gint8* data,
    int count)
{
    while (count) {
        *data++ += 128;
        count--;
    }
}

void st_sample_16bit_signed_unsigned(gint16* data,
    int count)
{
    while (count) {
        *data++ += 32768;
        count--;
    }
}

void st_convert_to_nonint(gint16* src,
    gint16* dest,
    const gsize offset,
    const gsize frames)
{
    gsize i;

    for (i = 0; i < frames; i++) {
        dest[i] = src[i << 1];
        dest[i + offset] = src[(i << 1) + 1];
    }
}

void st_convert_to_int(gint16* src,
    gint16* dest,
    const gsize offset,
    const gsize frames)
{
    gsize i;

    for (i = 0; i < frames; i++) {
        dest[i << 1] = src[i];
        dest[(i << 1) + 1] = src[i + offset];
    }
}

void
st_for_each_note(XM* xm,
    gboolean (*foreach_func)(XM*, XMNote*, const gint, gpointer),
    STForEachNoteFlags flags,
    gpointer user_data)
{
    gint i, j, k;
    guchar cur_ins[XM_NUM_CHAN] = {0};
    XMPattern* p;
    XMNote* c;
    gboolean go_on = TRUE;
    gint maxi = (flags & ST_FLAGS_UNUSED) ?
        sizeof(xm->patterns) / sizeof(xm->patterns)[0] : xm->song_length;

    for (i = 0; i < maxi; i++) {
        p = &xm->patterns[(flags & ST_FLAGS_UNUSED) ? i : (gint)xm->pattern_order_table[i]];
        for (j = 0; j < xm->num_channels; j++) {
            c = p->channels[j];
            for (k = 0; k < p->length; k++) {
                guchar curins = c[k].instrument;

                /* A little bit tricky, we should remember the last
                   specified instrument for the channel */
                if (curins)
                    cur_ins[j] = curins;
                if (((flags & ST_FLAGS_NOTE) && c[k].note) ||
                    ((flags & ST_FLAGS_INSTR) && curins) ||
                    ((flags & ST_FLAGS_VOLUME) && c[k].volume >= XM_NOTE_VOLUME_MIN
                        && c[k].volume <= XM_NOTE_VOLUME_MAX) ||
                    ((flags & ST_FLAGS_VOLEFF) && c[k].volume > XM_NOTE_VOLUME_MAX) ||
                    ((flags & ST_FLAGS_FXTYPE) && c[k].fxtype) ||
                    ((flags & ST_FLAGS_FXPARAM) && c[k].fxparam))
                    go_on = foreach_func(xm, &c[k], cur_ins[j], user_data);
                if (!go_on)
                    return;
            }
        }
    }
}
