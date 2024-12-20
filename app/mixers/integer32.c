
/*
 * The Real SoundTracker - Basic 32bit integers mixer. Probably the
 * worst which you can come up with.
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

#include <config.h>

#include <glib/gi18n.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "mixer.h"
#include "tracer.h"

static int num_channels, mixfreq;
static int stereo;

typedef struct integer32_channel {
    st_mixer_sample_info* sample;
    st_mixer_buffer* mixbuf;

    void* data; /* copy of sample->data */
    guint32 length; /* length of sample (converted) */
    guint32 playend; /* for a forced premature end of the sample */

    int running; /* this channel is active */
    guint32 current; /* current playback position in sample (converted) */
    guint32 speed; /* sample playback speed (converted) */

    guint32 loopstart; /* loop start (converted) */
    guint32 loopend; /* loop end (converted) */
    int loopflags; /* 0 none, 1 forward, 2 pingpong */
    int direction; /* current pingpong direction (+1 forward, -1 backward) */

    int volume; /* 0..64 */
    float panning; /* -1.0 .. +1.0 */
} integer32_channel;

static integer32_channel channels[32];

#define ACCURACY 12 /* accuracy of the fixed point stuff, ALSO HARDCODED in the assembly routines!! */

#define MAX_SAMPLE_LENGTH ((1 << (32 - ACCURACY)) - 1)

static void
integer32_setnumch(int n)
{
    g_assert(n >= 1 && n <= 32);

    num_channels = n;
}

static void
integer32_setbuffers(st_mixer_buffer buffers[])
{
    int i;

    for (i = 0; i < 32; i++)
        channels[i].mixbuf = &buffers[i];
}

static void
integer32_updatesample(st_mixer_sample_info* si)
{
    int i;
    integer32_channel* c;

    for (i = 0; i < 32; i++) {
        c = &channels[i];
        if (c->sample != si || !c->running) {
            continue;
        }

        if (c->data != si->data
            || c->length != MIN(si->length, MAX_SAMPLE_LENGTH) << ACCURACY
            || c->loopflags != (si->flags & ST_SAMPLE_LOOP_MASK)) {
            c->running = 0;
        }

        /* No relevant data has changed. Don't stop the sample, but update
	   our local loop data instead. */
        c->loopstart = MIN(si->loopstart, MAX_SAMPLE_LENGTH) << ACCURACY;
        c->loopend = MIN(si->loopend, MAX_SAMPLE_LENGTH) << ACCURACY;
        c->loopflags = si->flags & ST_SAMPLE_LOOP_MASK;
        if (c->loopflags != ST_SAMPLE_LOOPTYPE_NONE) {
            // we can be more clever here...
            c->current = c->loopstart;
            c->direction = 1;
        }
    }
}

static gboolean
integer32_setmixformat(int format)
{
    if (format != 16)
        return FALSE;

    return TRUE;
}

static gboolean
integer32_setstereo(int on)
{
    stereo = on;
    return TRUE;
}

static void
integer32_setmixfreq(guint32 frequency)
{
    mixfreq = frequency;
}

static void
integer32_reset(void)
{
    guint i;
    st_mixer_buffer* tmp[32];

    for (i = 0; i < 32; i++)
        tmp[i] = channels[i].mixbuf;
    memset(channels, 0, sizeof(channels));
    for (i = 0; i < 32; i++)
        channels[i].mixbuf = tmp[i];
}

static void
integer32_startnote(int channel,
    st_mixer_sample_info* s)
{
    integer32_channel* c = &channels[channel];

    c->sample = s;
    c->data = s->data;
    c->length = MIN(s->length, MAX_SAMPLE_LENGTH) << ACCURACY;
    c->playend = 0;
    c->running = 1;
    c->speed = 1;
    c->current = 0;
    c->loopstart = MIN(s->loopstart, MAX_SAMPLE_LENGTH) << ACCURACY;
    c->loopend = MIN(s->loopend, MAX_SAMPLE_LENGTH) << ACCURACY;
    c->loopflags = s->flags & ST_SAMPLE_LOOP_MASK;
    c->direction = 1;
}

static void
integer32_stopnote(int channel)
{
    integer32_channel* c = &channels[channel];

    c->running = 0;
}

static void
integer32_setsmplpos(int channel,
    guint32 offset)
{
    integer32_channel* c = &channels[channel];

    if (offset<c->length>> ACCURACY) {
        c->current = offset << ACCURACY;
        c->direction = 1;
    } else {
        c->running = 0;
    }
}

static void
integer32_setsmplend(int channel,
    guint32 playend)
{
    integer32_channel* c = &channels[channel];

    if ((c->current != 0 || playend<c->length>> ACCURACY) && playend > 0) {
        c->playend = MIN(playend, MAX_SAMPLE_LENGTH) << ACCURACY;
    }
}

static void
integer32_setfreq(int channel,
    float frequency)
{
    integer32_channel* c = &channels[channel];

    if (frequency > (0x7fffffff >> ACCURACY)) {
        frequency = (0x7fffffff >> ACCURACY);
    }

    c->speed = frequency * (1 << ACCURACY) / mixfreq;
    if (c->speed == 0) {
        c->speed = 1;
    }
}

static void
integer32_setvolume(int channel,
    float volume)
{
    integer32_channel* c = &channels[channel];

    c->volume = 64 * volume;
}

static void
integer32_setpanning(int channel,
    float panning)
{
    integer32_channel* c = &channels[channel];

    c->panning = panning;
}

static void
integer32_render(guint32 count,
    gint16* scopebufs[],
    int scopebuf_offset,
    time_buffer* channels_status_tb,
    gdouble time)
{
    guint32 t;
    int i, j, *m, v;
    integer32_channel* c;
    int done;
    int offs2end, oflcnt, looplen;
    gint16* scopedata = NULL;
    int vl = 0;
    int vr = 0;
    gint16* data;
    int s, val;

    for (i = 0; i < num_channels; i++) {
        c = &channels[i];
        t = count;
        m = c->mixbuf->buffer;
        v = c->volume;

        if (scopebufs)
            scopedata = scopebufs[i] + scopebuf_offset;

        if (!c->running) {
            c->mixbuf->num_processed = 0;
            if (scopebufs)
                memset(scopedata, 0, 2 * count);
            continue;
        }

        g_mutex_lock(&c->sample->lock);

        while (t) {
            /* Check how much of the sample we can fill in one run */
            if (c->loopflags && c->playend == 0) {
                looplen = c->loopend - c->loopstart;
                g_assert(looplen > 0);
                if (c->loopflags == ST_SAMPLE_LOOPTYPE_AMIGA) {
                    offs2end = c->loopend - c->current;
                    if (offs2end <= 0) {
                        oflcnt = -offs2end / looplen;
                        offs2end += oflcnt * looplen;
                        c->current = c->loopstart - offs2end;
                        offs2end = c->loopend - c->current;
                    }
                } else /* if(c->loopflags == ST_SAMPLE_LOOPTYPE_PINGPONG) */ {
                    if (c->direction == 1)
                        offs2end = c->loopend - c->current;
                    else
                        offs2end = c->current - c->loopstart;

                    if (offs2end <= 0) {
                        oflcnt = -offs2end / looplen;
                        offs2end += oflcnt * looplen;
                        if ((oflcnt && 1) ^ (c->direction == -1)) {
                            c->current = c->loopstart - offs2end;
                            offs2end = c->loopend - c->current;
                            c->direction = 1;
                        } else {
                            c->current = c->loopend + offs2end;
                            if (c->current == c->loopend)
                                c->current--;
                            offs2end = c->current - c->loopstart;
                            c->direction = -1;
                        }
                    }
                }
                g_assert(offs2end >= 0);
                done = offs2end / c->speed + 1;
            } else /* if(c->loopflags == LOOP_NO) */ {
                done = ((c->playend ? c->playend : c->length) - c->current) / c->speed;
                if (!done) {
                    c->running = 0;
                    break;
                }
            }

            g_assert(done > 0);

            if (done > t)
                done = t;
            t -= done;

            g_assert(c->current >= 0 && (c->current >> ACCURACY) < c->length);

            if (stereo) {
                vl = 64 - ((c->panning + 1.0) * 32);
                vr = (c->panning + 1.0) * 32;
            }

            /* This one does the actual mixing */
            data = c->data;
            if (scopebufs) {
                if (stereo) {
                    for (j = c->current, s = c->speed * c->direction; done; done--, j += s) {
                        val = v * data[j >> ACCURACY];
                        *m++ = vl * val >> 6;
                        *m++ = vr * val >> 6;
                        *scopedata++ = val >> 6;
                    }
                } else {
                    for (j = c->current, s = c->speed * c->direction; done; done--, j += s) {
                        val = v * data[j >> ACCURACY];
                        *m++ = val;
                        *scopedata++ = val >> 6;
                    }
                }
            } else {
                if (stereo) {
                    vl *= v;
                    vr *= v;
                    for (j = c->current, s = c->speed * c->direction; done; done--, j += s) {
                        val = data[j >> ACCURACY];
                        *m++ = vl * val >> 6;
                        *m++ = vr * val >> 6;
                    }
                } else {
                    for (j = c->current, s = c->speed * c->direction; done; done--, j += s) {
                        val = v * data[j >> ACCURACY];
                        *m++ = val;
                    }
                }
            }

            c->current = j;
        }

        g_mutex_unlock(&c->sample->lock);
        c->mixbuf->num_processed = count - t;
    }
}

void integer32_dumpstatus(st_mixer_channel_status array[])
{
    int i;

    for (i = 0; i < 32; i++) {
        if (channels[i].running) {
            array[i].current_sample = channels[i].sample;
            array[i].current_position = channels[i].current >> ACCURACY;
        } else {
            array[i].current_sample = NULL;
        }
    }
}

static void
integer32_loadchsettings(int ch)
{
    tracer_channel* tch;
    integer32_channel* c;
    guint64 tmp64;

    g_assert(ch < num_channels);

    tch = tracer_return_channel(ch);
    c = &channels[ch];

    c->sample = tch->sample;
    c->data = tch->data;

    if (tch->sample) {
        c->loopflags = tch->looptype;
        c->loopstart = MIN(tch->sample->loopstart, MAX_SAMPLE_LENGTH) << ACCURACY;
        c->loopend = MIN(tch->sample->loopend, MAX_SAMPLE_LENGTH) << ACCURACY;
    }
    c->length = MIN(tch->length, MAX_SAMPLE_LENGTH) << ACCURACY;
    c->volume = tch->volume * 64;
    c->panning = tch->panning;
    c->direction = tch->direction;
    c->playend = MIN(tch->playend, MAX_SAMPLE_LENGTH) << ACCURACY;
    tmp64 = (((guint64)tch->positionw << 32) + tch->positionf) >> (32 - ACCURACY);
    c->current = MIN(tmp64, MAX_SAMPLE_LENGTH << ACCURACY);
    tmp64 = (((guint64)tch->freqw << 32) + tch->freqf) >> (32 - ACCURACY);
    c->speed = MIN(tmp64, MAX_SAMPLE_LENGTH << ACCURACY);

    c->running = tch->flags & TR_FLAG_SAMPLE_RUNNING;
}

st_mixer mixer_integer32 = {
    "integer32",
    N_("Integers mixer, no interpolation, no filters, maximum sample length 1M"),

    integer32_setnumch,
    integer32_setbuffers,
    integer32_updatesample,
    integer32_setmixformat,
    integer32_setstereo,
    integer32_setmixfreq,
    integer32_reset,
    integer32_startnote,
    integer32_stopnote,
    integer32_setsmplpos,
    integer32_setsmplend,
    integer32_setfreq,
    integer32_setvolume,
    integer32_setpanning,
    NULL,
    NULL,
    integer32_render,
    integer32_dumpstatus,
    integer32_loadchsettings,

    MAX_SAMPLE_LENGTH,
    ST_MIXER_BUFFER_FORMAT_INT,

    NULL
};
