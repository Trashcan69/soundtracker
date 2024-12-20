
/*
 * The Real SoundTracker - Cubically interpolating mixing routines
 *                         with IT style filter support
 *
 * Non-optimized C, translated from Tammo's original i386 assembly code.
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

#include <config.h>

#include "kbfloat-core.h"

#define CUBICMIXER_COMMON_HEAD           \
    gint16* positioni = data->positioni; \
    guint32 positionf = data->positionf; \
    float* mixbuffer = data->mixbuffer;  \
    float fl1 = data->fl1;               \
    float fb1 = data->fb1;               \
    float voll = data->volleft;          \
    float volr = data->volright;         \
    unsigned n = data->numsamples;

#define CUBICMIXER_COMMON_HEAD_S         \
    gint16* positionir;                  \
    float fl1r = data->fl1r;             \
    float fb1r = data->fb1r;

#define CUBICMIXER_COMMON_LOOP_START \
    while (n--) {                    \
        float s0, s0r;               \
        guint32 positionf_new;

#define CUBICMIXER_LOOP_FORWARD                       \
    s0 = positioni[0] * kb_x86_ct0[positionf >> 24];  \
    s0 += positioni[1] * kb_x86_ct1[positionf >> 24]; \
    s0 += positioni[2] * kb_x86_ct2[positionf >> 24]; \
    s0 += positioni[3] * kb_x86_ct3[positionf >> 24];

#define CUBICMIXER_LOOP_BACKWARD                        \
    s0 = positioni[0] * kb_x86_ct0[-positionf >> 24];   \
    s0 += positioni[-1] * kb_x86_ct1[-positionf >> 24]; \
    s0 += positioni[-2] * kb_x86_ct2[-positionf >> 24]; \
    s0 += positioni[-3] * kb_x86_ct3[-positionf >> 24];

#define CUBICMIXER_LOOP_FORWARD_R                       \
    positionir = positioni + data->stereo_off;          \
    s0r = positionir[0] * kb_x86_ct0[positionf >> 24];  \
    s0r += positionir[1] * kb_x86_ct1[positionf >> 24]; \
    s0r += positionir[2] * kb_x86_ct2[positionf >> 24]; \
    s0r += positionir[3] * kb_x86_ct3[positionf >> 24];

#define CUBICMIXER_LOOP_BACKWARD_R                        \
    positionir = positioni + data->stereo_off;            \
    s0r = positionir[0] * kb_x86_ct0[-positionf >> 24];   \
    s0r += positionir[-1] * kb_x86_ct1[-positionf >> 24]; \
    s0r += positionir[-2] * kb_x86_ct2[-positionf >> 24]; \
    s0r += positionir[-3] * kb_x86_ct3[-positionf >> 24];

#define CUBICMIXER_ADVANCE_POINTER           \
    positionf_new = positionf + data->freqf; \
    if (positionf_new < positionf) {         \
        positioni++;                         \
    }                                        \
    positionf = positionf_new;               \
    positioni += data->freqi;

#define CUBICMIXER_FILTER                               \
    fb1 = data->freso * fb1 + data->ffreq * (s0 - fl1); \
    fl1 += data->ffreq * fb1;                           \
    s0 = fl1;

#define CUBICMIXER_FILTER_R                                 \
    fb1r = data->freso * fb1r + data->ffreq * (s0r - fl1r); \
    fl1r += data->ffreq * fb1r;                             \
    s0r = fl1r;

#define CUBICMIXER_WRITE_OUT  \
    s0r = s0 * volr;          \
    s0 *= voll;               \
    *mixbuffer++ = s0;        \
    *mixbuffer++ = s0r;

#define CUBICMIXER_WRITE_OUT_VIRTUAL \
    s0r = s0 * volr;          \
    s0 *= voll;               \
    *mixbuffer++ += s0;       \
    *mixbuffer++ += s0r;

#define CUBICMIXER_WRITE_OUT_S  \
    s0r *= volr;                \
    s0 *= voll;                 \
    *mixbuffer++ = s0;          \
    *mixbuffer++ = s0r;

#define CUBICMIXER_WRITE_OUT_VIRTUAL_S \
    s0r *= volr;              \
    s0 *= voll;               \
    *mixbuffer++ += s0;       \
    *mixbuffer++ += s0r;

#define CUBICMIXER_SCOPES \
    *scopebuf++ = (gint16)(s0 + s0r);

#define CUBICMIXER_VOLRAMP  \
    voll += data->volrampl; \
    volr += data->volrampr;

#define CUBICMIXER_COMMON_FOOT   \
    data->volleft = voll;        \
    data->volright = volr;       \
    data->positioni = positioni; \
    data->positionf = positionf; \
    data->mixbuffer = mixbuffer; \
    data->fl1 = fl1;             \
    data->fb1 = fb1;

#define CUBICMIXER_COMMON_FOOT_S \
    data->fl1r = fl1r;           \
    data->fb1r = fb1r;

/* --- 0 --- */
static void
kbfloat_mix_cubic_noscopes_unfiltered_forward_noramp(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_backward_noramp(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_forward_noramp(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                        CUBICMIXER_WRITE_OUT
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_backward_noramp(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                        CUBICMIXER_WRITE_OUT
}

CUBICMIXER_COMMON_FOOT
}

/* --- 4 --- */
static void
kbfloat_mix_cubic_scopes_unfiltered_forward_noramp(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_unfiltered_backward_noramp(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_forward_noramp(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_backward_noramp(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

/* --- 8 --- */
static void
kbfloat_mix_cubic_noscopes_unfiltered_forward(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT
                        CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_backward(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT
                        CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_forward(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                        CUBICMIXER_WRITE_OUT
                            CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_backward(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                        CUBICMIXER_WRITE_OUT
                            CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

/* --- 12 --- */
static void
kbfloat_mix_cubic_scopes_unfiltered_forward(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_unfiltered_backward(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_forward(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_backward(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_forward_noramp_virtual(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_VIRTUAL
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_backward_noramp_virtual(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_VIRTUAL
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_forward_noramp_virtual(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                        CUBICMIXER_WRITE_OUT_VIRTUAL
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_backward_noramp_virtual(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                        CUBICMIXER_WRITE_OUT_VIRTUAL
}

CUBICMIXER_COMMON_FOOT
}

/* --- 4 --- */
static void
kbfloat_mix_cubic_scopes_unfiltered_forward_noramp_virtual(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_VIRTUAL
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_unfiltered_backward_noramp_virtual(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_VIRTUAL
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_forward_noramp_virtual(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT_VIRTUAL
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_backward_noramp_virtual(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT_VIRTUAL
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

/* --- 8 --- */
static void
kbfloat_mix_cubic_noscopes_unfiltered_forward_virtual(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_VIRTUAL
                        CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_backward_virtual(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_VIRTUAL
                        CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_forward_virtual(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                        CUBICMIXER_WRITE_OUT_VIRTUAL
                            CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_backward_virtual(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                        CUBICMIXER_WRITE_OUT_VIRTUAL
                            CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

/* --- 12 --- */
static void
kbfloat_mix_cubic_scopes_unfiltered_forward_virtual(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_VIRTUAL
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_unfiltered_backward_virtual(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_VIRTUAL
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_forward_virtual(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT_VIRTUAL
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_backward_virtual(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_WRITE_OUT_VIRTUAL
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

/* --- 0 --- */
static void
kbfloat_mix_cubic_noscopes_unfiltered_forward_noramp_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
            CUBICMIXER_LOOP_FORWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_S
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_backward_noramp_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
            CUBICMIXER_LOOP_BACKWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_S
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_forward_noramp_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
            CUBICMIXER_LOOP_FORWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                    CUBICMIXER_FILTER_R
                        CUBICMIXER_WRITE_OUT_S
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

static void
kbfloat_mix_cubic_noscopes_filtered_backward_noramp_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
            CUBICMIXER_LOOP_BACKWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                    CUBICMIXER_FILTER_R
                        CUBICMIXER_WRITE_OUT_S
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

/* --- 4 --- */
static void
kbfloat_mix_cubic_scopes_unfiltered_forward_noramp_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_LOOP_FORWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_S
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_unfiltered_backward_noramp_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_LOOP_BACKWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_S
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_forward_noramp_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_LOOP_FORWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_FILTER_R
    CUBICMIXER_WRITE_OUT_S
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

static void
kbfloat_mix_cubic_scopes_filtered_backward_noramp_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_LOOP_BACKWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_FILTER_R
    CUBICMIXER_WRITE_OUT_S
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

/* --- 8 --- */
static void
kbfloat_mix_cubic_noscopes_unfiltered_forward_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
            CUBICMIXER_LOOP_FORWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_S
                        CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_backward_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
            CUBICMIXER_LOOP_BACKWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_S
                        CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_forward_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
            CUBICMIXER_LOOP_FORWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                    CUBICMIXER_FILTER_R
                        CUBICMIXER_WRITE_OUT_S
                            CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

static void
kbfloat_mix_cubic_noscopes_filtered_backward_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
            CUBICMIXER_LOOP_BACKWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                    CUBICMIXER_FILTER_R
                        CUBICMIXER_WRITE_OUT_S
                            CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

/* --- 12 --- */
static void
kbfloat_mix_cubic_scopes_unfiltered_forward_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_LOOP_FORWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_S
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_unfiltered_backward_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_LOOP_BACKWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_S
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_forward_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_LOOP_FORWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_FILTER_R
    CUBICMIXER_WRITE_OUT_S
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

static void
kbfloat_mix_cubic_scopes_filtered_backward_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_LOOP_BACKWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_FILTER_R
    CUBICMIXER_WRITE_OUT_S
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

/* --- 0 --- */
static void
kbfloat_mix_cubic_noscopes_unfiltered_forward_noramp_virtual_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
            CUBICMIXER_LOOP_FORWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_VIRTUAL_S
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_backward_noramp_virtual_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
            CUBICMIXER_LOOP_BACKWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_VIRTUAL_S
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_forward_noramp_virtual_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
            CUBICMIXER_LOOP_FORWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                    CUBICMIXER_FILTER_R
                        CUBICMIXER_WRITE_OUT_VIRTUAL_S
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

static void
kbfloat_mix_cubic_noscopes_filtered_backward_noramp_virtual_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
            CUBICMIXER_LOOP_BACKWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                    CUBICMIXER_FILTER_R
                        CUBICMIXER_WRITE_OUT_VIRTUAL_S
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

/* --- 4 --- */
static void
kbfloat_mix_cubic_scopes_unfiltered_forward_noramp_virtual_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_LOOP_FORWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_VIRTUAL_S
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_unfiltered_backward_noramp_virtual_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_LOOP_BACKWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_VIRTUAL_S
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_forward_noramp_virtual_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_LOOP_FORWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_FILTER_R
    CUBICMIXER_WRITE_OUT_VIRTUAL_S
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

static void
kbfloat_mix_cubic_scopes_filtered_backward_noramp_virtual_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_LOOP_BACKWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_FILTER_R
    CUBICMIXER_WRITE_OUT_VIRTUAL_S
    CUBICMIXER_SCOPES
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

/* --- 8 --- */
static void
kbfloat_mix_cubic_noscopes_unfiltered_forward_virtual_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
            CUBICMIXER_LOOP_FORWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_VIRTUAL_S
                        CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_unfiltered_backward_virtual_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
            CUBICMIXER_LOOP_BACKWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_WRITE_OUT_VIRTUAL_S
                        CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_noscopes_filtered_forward_virtual_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_FORWARD
            CUBICMIXER_LOOP_FORWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                    CUBICMIXER_FILTER_R
                        CUBICMIXER_WRITE_OUT_VIRTUAL_S
                            CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

static void
kbfloat_mix_cubic_noscopes_filtered_backward_virtual_stereo(kb_x86_mixer_data* data){
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S

        CUBICMIXER_COMMON_LOOP_START
            CUBICMIXER_LOOP_BACKWARD
            CUBICMIXER_LOOP_BACKWARD_R
                CUBICMIXER_ADVANCE_POINTER
                    CUBICMIXER_FILTER
                    CUBICMIXER_FILTER_R
                        CUBICMIXER_WRITE_OUT_VIRTUAL_S
                            CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

/* --- 12 --- */
static void
kbfloat_mix_cubic_scopes_unfiltered_forward_virtual_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_LOOP_FORWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_VIRTUAL_S
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_unfiltered_backward_virtual_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    gint16* positionir;
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_LOOP_BACKWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_WRITE_OUT_VIRTUAL_S
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
}

static void
kbfloat_mix_cubic_scopes_filtered_forward_virtual_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_FORWARD
    CUBICMIXER_LOOP_FORWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_FILTER_R
    CUBICMIXER_WRITE_OUT_VIRTUAL_S
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

static void
kbfloat_mix_cubic_scopes_filtered_backward_virtual_stereo(kb_x86_mixer_data* data)
{
    CUBICMIXER_COMMON_HEAD
    CUBICMIXER_COMMON_HEAD_S
    gint16* scopebuf = data->scopebuf;

    CUBICMIXER_COMMON_LOOP_START
    CUBICMIXER_LOOP_BACKWARD
    CUBICMIXER_LOOP_BACKWARD_R
    CUBICMIXER_ADVANCE_POINTER
    CUBICMIXER_FILTER
    CUBICMIXER_FILTER_R
    CUBICMIXER_WRITE_OUT_VIRTUAL_S
    CUBICMIXER_SCOPES
    CUBICMIXER_VOLRAMP
}

CUBICMIXER_COMMON_FOOT
CUBICMIXER_COMMON_FOOT_S
}

static void (*kbfloat_mixers[64])(kb_x86_mixer_data*) = {
    kbfloat_mix_cubic_noscopes_unfiltered_forward_noramp,
    kbfloat_mix_cubic_noscopes_unfiltered_backward_noramp,
    kbfloat_mix_cubic_noscopes_filtered_forward_noramp,
    kbfloat_mix_cubic_noscopes_filtered_backward_noramp,
    kbfloat_mix_cubic_scopes_unfiltered_forward_noramp,
    kbfloat_mix_cubic_scopes_unfiltered_backward_noramp,
    kbfloat_mix_cubic_scopes_filtered_forward_noramp,
    kbfloat_mix_cubic_scopes_filtered_backward_noramp,
    kbfloat_mix_cubic_noscopes_unfiltered_forward,
    kbfloat_mix_cubic_noscopes_unfiltered_backward,
    kbfloat_mix_cubic_noscopes_filtered_forward,
    kbfloat_mix_cubic_noscopes_filtered_backward,
    kbfloat_mix_cubic_scopes_unfiltered_forward,
    kbfloat_mix_cubic_scopes_unfiltered_backward,
    kbfloat_mix_cubic_scopes_filtered_forward,
    kbfloat_mix_cubic_scopes_filtered_backward,
    kbfloat_mix_cubic_noscopes_unfiltered_forward_noramp_virtual,
    kbfloat_mix_cubic_noscopes_unfiltered_backward_noramp_virtual,
    kbfloat_mix_cubic_noscopes_filtered_forward_noramp_virtual,
    kbfloat_mix_cubic_noscopes_filtered_backward_noramp_virtual,
    kbfloat_mix_cubic_scopes_unfiltered_forward_noramp_virtual,
    kbfloat_mix_cubic_scopes_unfiltered_backward_noramp_virtual,
    kbfloat_mix_cubic_scopes_filtered_forward_noramp_virtual,
    kbfloat_mix_cubic_scopes_filtered_backward_noramp_virtual,
    kbfloat_mix_cubic_noscopes_unfiltered_forward_virtual,
    kbfloat_mix_cubic_noscopes_unfiltered_backward_virtual,
    kbfloat_mix_cubic_noscopes_filtered_forward_virtual,
    kbfloat_mix_cubic_noscopes_filtered_backward_virtual,
    kbfloat_mix_cubic_scopes_unfiltered_forward_virtual,
    kbfloat_mix_cubic_scopes_unfiltered_backward_virtual,
    kbfloat_mix_cubic_scopes_filtered_forward_virtual,
    kbfloat_mix_cubic_scopes_filtered_backward_virtual,
    kbfloat_mix_cubic_noscopes_unfiltered_forward_noramp_stereo,
    kbfloat_mix_cubic_noscopes_unfiltered_backward_noramp_stereo,
    kbfloat_mix_cubic_noscopes_filtered_forward_noramp_stereo,
    kbfloat_mix_cubic_noscopes_filtered_backward_noramp_stereo,
    kbfloat_mix_cubic_scopes_unfiltered_forward_noramp_stereo,
    kbfloat_mix_cubic_scopes_unfiltered_backward_noramp_stereo,
    kbfloat_mix_cubic_scopes_filtered_forward_noramp_stereo,
    kbfloat_mix_cubic_scopes_filtered_backward_noramp_stereo,
    kbfloat_mix_cubic_noscopes_unfiltered_forward_stereo,
    kbfloat_mix_cubic_noscopes_unfiltered_backward_stereo,
    kbfloat_mix_cubic_noscopes_filtered_forward_stereo,
    kbfloat_mix_cubic_noscopes_filtered_backward_stereo,
    kbfloat_mix_cubic_scopes_unfiltered_forward_stereo,
    kbfloat_mix_cubic_scopes_unfiltered_backward_stereo,
    kbfloat_mix_cubic_scopes_filtered_forward_stereo,
    kbfloat_mix_cubic_scopes_filtered_backward_stereo,
    kbfloat_mix_cubic_noscopes_unfiltered_forward_noramp_virtual_stereo,
    kbfloat_mix_cubic_noscopes_unfiltered_backward_noramp_virtual_stereo,
    kbfloat_mix_cubic_noscopes_filtered_forward_noramp_virtual_stereo,
    kbfloat_mix_cubic_noscopes_filtered_backward_noramp_virtual_stereo,
    kbfloat_mix_cubic_scopes_unfiltered_forward_noramp_virtual_stereo,
    kbfloat_mix_cubic_scopes_unfiltered_backward_noramp_virtual_stereo,
    kbfloat_mix_cubic_scopes_filtered_forward_noramp_virtual_stereo,
    kbfloat_mix_cubic_scopes_filtered_backward_noramp_virtual_stereo,
    kbfloat_mix_cubic_noscopes_unfiltered_forward_virtual_stereo,
    kbfloat_mix_cubic_noscopes_unfiltered_backward_virtual_stereo,
    kbfloat_mix_cubic_noscopes_filtered_forward_virtual_stereo,
    kbfloat_mix_cubic_noscopes_filtered_backward_virtual_stereo,
    kbfloat_mix_cubic_scopes_unfiltered_forward_virtual_stereo,
    kbfloat_mix_cubic_scopes_unfiltered_backward_virtual_stereo,
    kbfloat_mix_cubic_scopes_filtered_forward_virtual_stereo,
    kbfloat_mix_cubic_scopes_filtered_backward_virtual_stereo
};

void kbasm_mix(kb_x86_mixer_data* data)
{
    kbfloat_mixers[data->flags >> 2](data);
}
