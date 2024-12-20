
/*
 * The Real SoundTracker - Cubically interpolating mixing routines
 *                         with IT style filter support
 *
 * Copyright (C) 2001-2019 Michael Krause
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

#ifndef _ST_MIXERASM_H
#define _ST_MIXERASM_H

#include <glib.h>

/* The numbers at the end of the lines are structure field offsets on
   32-bit machines: */

typedef struct kb_x86_mixer_data {
    float volleft; // left volume (1.0=normal)            0
    float volright; // right volume (1.0=normal)           4
    float volrampl; // left volramp (dvol/sample)          8
    float volrampr; // right volramp (dvol/sample)         12
    gint16* positioni; // pointer to sample data              16
    guint32 positionf; // fractional part of pointer          20
    guint32 stereo_off; // offset of the second channel  24
    gint32 freqi; // integer part of delta               28
    guint32 freqf; // fractional part of delta            32
    float* mixbuffer; // pointer to destination buffer       36
    guint32 numsamples; // number of samples to render         40
    float lastl; // declick value                       44
    float ffreq; // filter frequency (0<=x<=1)          48
    float freso; // filter resonance (0<=x<1)           52
    float fl1, fl1r; // filter lp buffers                    56, 60
    float fb1, fb1r; // filter bp buffers                    64, 68
    gint16* scopebuf; //                                     72
    guint32 flags; // which mixer to use                  76
} kb_x86_mixer_data;

#define KB_X86_MIXER_FLAGS_BACKWARD (1 << 2)
#define KB_X86_MIXER_FLAGS_FILTERED (1 << 3)
#define KB_X86_MIXER_FLAGS_SCOPES (1 << 4)
#define KB_X86_MIXER_FLAGS_VOLRAMP (1 << 5)
#define KB_X86_MIXER_FLAGS_VIRTUAL (1 << 6)
#define KB_X86_MIXER_FLAGS_STEREO (1 << 7)

void kbasm_mix(kb_x86_mixer_data* data);

extern float kb_x86_ct0[256];
extern float kb_x86_ct1[256];
extern float kb_x86_ct2[256];
extern float kb_x86_ct3[256];

#endif /* _ST_MIXERASM_H */
