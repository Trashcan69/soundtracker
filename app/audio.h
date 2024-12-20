
/*
 * The Real SoundTracker - Audio handling thread (header)
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

#ifndef _ST_AUDIO_H
#define _ST_AUDIO_H

#include <config.h>

#include <sys/time.h>

#include <glib.h>

#include "driver.h"
#include "event-waiter.h"
#include "mixer.h"
#include "time-buffer.h"

/* === Synchronous commmunications via time buffers */

typedef enum {
    AUDIO_COMMAND_NONE = 0,
    AUDIO_COMMAND_STOP_PLAYING,
    AUDIO_COMMAND_START_PLAYING
} audio_command_id;

/* === Oscilloscope stuff

   There's a buffer for each channel that gets filled in a circular fashion.
   The earliest and the latest valid entry in this "ring buffer" are indicated
   in scopebuf_start and scopebuf_end.

   So the amount of data contained in this buffer at any time is simply
   scopebuf_end.time - scopebuf_start.time, regardless of the offsets. Then you
   simply walk through the scopebufs[] array starting at offset scopebuf_start.offset,
   modulo scopebuf_length, until you reach scopebuf_end.offset. */

extern gboolean scopebuf_ready;
extern gint16* scopebufs[32];
extern gint32 scopebuf_length;
typedef struct {
    guint32 offset; // always < scopebuf_length
    double time;
} scopebuf_endpoint;
extern scopebuf_endpoint scopebuf_start, scopebuf_end;
extern int scopebuf_freq;

/* === Player position time buffer */

typedef struct {
    double time;
    int command;
    double next_tick_time, prev_tick_time;
    int songpos;
    int patpos;
    int patno;
    int tempo, prev_tempo;
    int bpm;
    gint8 curtick;
} audio_player_pos;

extern time_buffer* audio_playerpos_tb;

/* === Clipping indicator time buffer */

typedef struct {
    double time;
    gboolean clipping;
} audio_clipping_indicator;

extern time_buffer* audio_clipping_indicator_tb;

/* === Mixer (sample) position time buffer */

typedef struct {
    double time;
    st_mixer_channel_status dump[32];
} audio_mixer_position;

extern time_buffer* audio_mixer_position_tb;

/* === Status of the mixer channel buffer */

typedef struct {
    double time;
    audio_command_id command;
    gint channel, instr, sample, note;
} audio_channel_status;

extern time_buffer* audio_channels_status_tb;

/* === Other stuff */

typedef enum {
    AUDIO_RENDER_SONG,
    AUDIO_RENDER_PATTERN,
    AUDIO_RENDER_TRACK,
    AUDIO_RENDER_BLOCK
} audio_render_target;

extern st_mixer* mixer;
extern st_driver *playback_driver, *editing_driver, *current_driver;
extern void *playback_driver_object, *editing_driver_object, *current_driver_object;

extern gint8 player_mute_channels[32];

extern event_waiter* audio_songpos_ew;
extern event_waiter* audio_tempo_ew;
extern event_waiter* audio_bpm_ew;

extern st_mixer* mixer;

gboolean audio_init(void);
void audio_set_mixer(st_mixer* mixer);
/* Use the following two functions directly only for asynchronous rendering from
   the main thread. For sound playing communicate with the audio thread through
   the control pipe. */
guint32 audio_mix(void* dest,
    const guint32 count,
    const gint mixfreq,
    const gint mixformat,
    const gboolean full,
    gboolean* clipping);
void audio_set_amplification(float af);
void audio_prepare_for_rendering(audio_render_target target,
    gint pattern,
    gint patpos,
    gint stoppos,
    gint ch_start,
    gint num_ch);
void audio_cleanup_after_rendering(void);
gint audio_get_playback_rate();

void readpipe(int fd, void* p, int count);

/* --- Functions called by the player */

void driver_setnumch(int numchannels);
void driver_startnote(const gint channel,
    st_mixer_sample_info* si,
    const gint inst,
    const gint smpl,
    const gint note);
void driver_stopnote(int channel);
void driver_setsmplpos(int channel,
    guint32 offset);
void driver_setsmplend(int channel,
    guint32 offset);
void driver_setfreq(int channel,
    float frequency);
void driver_setvolume(int channel,
    float volume);
void driver_setpanning(int channel,
    float panning);
void driver_set_ch_filter_freq(int channel,
    float freq);
void driver_set_ch_filter_reso(int channel,
    float freq);

#endif /* _ST_AUDIO_H */
