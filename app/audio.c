
/*
 * The Real SoundTracker - Audio handling thread
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

#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "audio.h"
#include "audio-subs.h"
#include "driver.h"
#include "endian-conv.h"
#include "errors.h"
#include "event-waiter.h"
#include "gui-settings.h"
#include "gui-subs.h"
#include "main.h"
#include "mixer.h"
#include "poll.h"
#include "time-buffer.h"
#include "tracer.h"
#include "xm-player.h"

st_mixer* mixer = NULL;
st_driver* playback_driver = NULL;
st_driver* editing_driver = NULL;
st_driver* current_driver = NULL;
void* playback_driver_object = NULL;
void* editing_driver_object = NULL;
void* current_driver_object = NULL;
void* file_driver_object = NULL;

gint8 player_mute_channels[32];

/* Time buffers for Visual<->Audio synchronization */

time_buffer* audio_playerpos_tb;
time_buffer* audio_clipping_indicator_tb;
time_buffer* audio_mixer_position_tb;
time_buffer* audio_channels_status_tb;

static guint32 audio_visual_feedback_counter;
static guint32 audio_visual_feedback_clipping;
static guint32 audio_visual_feedback_update_interval = 2000; // just a dummy value

static const guint audio_visual_feedback_updates_per_second = 50;
static const guint audio_visual_feedback_smear_clipping = 3; // keep clipflag on for how long

/* Event waiters */

event_waiter* audio_songpos_ew;
event_waiter* audio_tempo_ew;
event_waiter* audio_bpm_ew;

static int set_songpos_wait_for = -1;
static int confirm_tempo = 0, confirm_bpm = 0;

/* Internal variables */

static int nice_value = 0;
static int ctlpipe;
static pthread_t threadid;

static int playing = 0;
static gboolean playing_noloop;

// --- for audio_mix() "main loop":

static int mixfmt_req, mixfmt, mixfmt_conv;
static int mixfreq_req;
static int audio_numchannels;
static float audio_ampfactor_f = 0.25;
static gint audio_ampfactor_i = 8;

static double pitchbend = +0.0, pitchbend_req;
static double audio_next_tick_time_unbent, audio_next_tick_time_bent, audio_current_playback_time_bent;
static double audio_mixer_current_time, audio_prev_tick_time;
static gint audio_prev_tempo;

static guint prev_bufsize = 0;
// Hardcoded, should be changed while implementing the virtual channels support
static st_mixer_buffer chan_buffers[64] = {{NULL, 0}};
static void* mix_buffer = NULL;
static gboolean clipflag = FALSE;

#define MIXFMT_16 1
#define MIXFMT_STEREO 2

#define MIXFMT_CONV_TO_16 1
#define MIXFMT_CONV_TO_8 2
#define MIXFMT_CONV_TO_UNSIGNED 4
#define MIXFMT_CONV_TO_STEREO 8
#define MIXFMT_CONV_TO_MONO 16
#define MIXFMT_CONV_BYTESWAP 32

/* Oscilloscope buffers */

gint16* scopebufs[32];
gint32 scopebuf_length;
scopebuf_endpoint scopebuf_start, scopebuf_end;
int scopebuf_freq;
gboolean scopebuf_ready;

void audio_prepare_for_playing(void);

static void
audio_raise_priority(void)
{
// The RT code tends to crash my kernel :-(
#if defined(_POSIX_PRIORITY_SCHEDULING) && 0
    struct sched_param sp;
    sp.sched_priority = 40;

    if (sched_setscheduler(0, SCHED_FIFO, &sp) == 0) {
        printf(">> running as realtime process now\n");
    } else {
        fprintf(stderr, "WARNING: Can't get realtime priority (try running as root)!\n");
    }
#else
    if (nice_value == 0) {
        if (!nice(-14)) {
            nice_value = -14;
        }
    }
#endif
}

static void
audio_ctlpipe_init_player(void)
{
    g_assert(xm != NULL);

    xmplayer_init_module();
}

static gboolean
driver_open(st_driver* driver, void* obj)
{
    if (driver->open(obj)) {
        current_driver = driver;
        current_driver_object = obj;
        return TRUE;
    }

    current_driver = NULL;
    return FALSE;
}

#define DRIVER_OPEN(arg) driver_open(arg##_driver, arg##_driver_object)

static void
audio_ctlpipe_play_song(int songpos,
    int patpos,
    const gboolean looped)
{
    g_assert(playback_driver != NULL);
    g_assert(mixer != NULL);
    g_assert(xm != NULL);
    g_assert(!playing);

    audio_prepare_for_playing();
    playing_noloop = !looped;

    if (gui_settings.permanent_channels) { /* Tracing only if really needed */
        xmplayer_init_play_song(0, 0, TRUE);

        if (songpos > 0 || patpos > 0) { /* Tracing! */
            int i;

            tracer_setnumch(audio_numchannels);
            tracer_trace(playback_driver->get_play_rate(playback_driver_object), songpos, patpos);
            xmplayer_init_play_song(songpos, patpos, FALSE);

            for (i = 0; i < audio_numchannels; i++)
                if (gui_settings.permanent_channels & (1 << i))
                    mixer->loadchsettings(i);
        }
    } else {
        xmplayer_init_play_song(songpos, patpos, TRUE);
    }

    /* When driver is opened, xmplayer can already be called. So all initialzation
       is to be performed before opening the driver to avoid possible concurrency. */
    if (DRIVER_OPEN(playback))
        audio_backpipe_write(AUDIO_BACKPIPE_PLAYING_STARTED);
    else
        audio_backpipe_write(AUDIO_BACKPIPE_DRIVER_OPEN_FAILED);
}

void
audio_prepare_for_rendering(audio_render_target target,
    gint pattern,
    gint patpos,
    gint stoppos,
    gint ch_start,
    gint num_ch)
{
    g_assert(mixer != NULL);
    g_assert(xm != NULL);
    g_assert(!playing);

    audio_prepare_for_playing();
    playing_noloop = TRUE;
    if (target == AUDIO_RENDER_SONG)
        xmplayer_init_play_song(0, 0, TRUE);
    else
        xmplayer_init_play_pattern(pattern, patpos, FALSE, stoppos, ch_start, num_ch);
}

void
audio_cleanup_after_rendering(void)
{
    xmplayer_stop();
    playing = 0;
}

static void
audio_ctlpipe_play_pattern(int pattern,
    int patpos,
    int only1row,
    gboolean looped,
    int stoppos,
    int ch_start,
    int num_ch)
{
    audio_backpipe_id a;

    g_assert(playback_driver != NULL);
    g_assert(mixer != NULL);
    g_assert(xm != NULL);
    g_assert(!playing);

    if (only1row) {
        if (DRIVER_OPEN(editing)) {
            audio_prepare_for_playing();
            xmplayer_init_play_pattern(pattern, patpos, only1row, -1, -1, -1);
            a = AUDIO_BACKPIPE_PLAYING_NOTE_STARTED;
        } else {
            a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
        }
    } else {
        if (DRIVER_OPEN(playback)) {
            audio_prepare_for_playing();
            playing_noloop = !looped;
            xmplayer_init_play_pattern(pattern, patpos, only1row, stoppos, ch_start, num_ch);
            a = AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED;
        } else {
            a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
        }
    }

    audio_backpipe_write(a);
}

static void
audio_ctlpipe_play_note(int channel,
    int note,
    int instrument,
    gboolean all)
{
    audio_backpipe_id a = AUDIO_BACKPIPE_PLAYING_NOTE_STARTED;

    if (!playing) {
        if (DRIVER_OPEN(editing))
            audio_prepare_for_playing();
        else
            a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
    }

    audio_backpipe_write(a);

    if (!playing)
        return;

    xmplayer_play_note(channel, note, instrument, all);
}

static void
audio_ctlpipe_play_note_full(int channel,
    int note,
    STSample* sample,
    guint32 offset,
    guint32 playend,
    gboolean all,
    gint inst,
    gint smpl)
{
    audio_backpipe_id a = AUDIO_BACKPIPE_PLAYING_NOTE_STARTED;

    if (!playing) {
        if (DRIVER_OPEN(editing))
            audio_prepare_for_playing();
        else
            a = AUDIO_BACKPIPE_DRIVER_OPEN_FAILED;
    }

    audio_backpipe_write(a);

    if (!playing)
        return;

    xmplayer_play_note_full(channel, note, sample, offset, playend, all, inst, smpl);
}

static void
audio_ctlpipe_play_note_keyoff(int channel)
{
    if (!playing)
        return;

    xmplayer_play_note_keyoff(channel);
}

static void
audio_ctlpipe_stop_note(int channel)
{
    if (!playing)
        return;

    xmplayer_stop_note(channel);
}

static void
audio_ctlpipe_stop_playing(void)
{
    if (playing == 1) {
        if (current_driver) {
            current_driver->release(current_driver_object);
            current_driver = NULL;
        }
        xmplayer_stop();
        current_driver_object = NULL;
        playing = 0;
    }

    if (set_songpos_wait_for != -1) {
        /* confirm pending request */
        event_waiter_confirm(audio_songpos_ew, 0.0);
        set_songpos_wait_for = -1;
    }

    audio_backpipe_write(AUDIO_BACKPIPE_PLAYING_STOPPED);
    audio_raise_priority();
}

static void
audio_ctlpipe_mix(void* sndbuf, int fragsize, int mixfreq, int format)
{
    audio_mix(sndbuf, fragsize, mixfreq, format, TRUE, NULL);
    if (current_driver && current_driver->commit)
        current_driver->commit(current_driver_object);
}

static void
audio_ctlpipe_set_songpos(int songpos)
{
    g_assert(playing);

    xmplayer_set_songpos(songpos);
    if (set_songpos_wait_for != -1) {
        /* confirm previous request */
        event_waiter_confirm(audio_songpos_ew, 0.0);
    }
    set_songpos_wait_for = songpos;
}

static void
audio_ctlpipe_set_tempo(int tempo)
{
    xmplayer_set_tempo(tempo);
    if (confirm_tempo != 0) {
        /* confirm previous request */
        event_waiter_confirm(audio_tempo_ew, 0.0);
    }
    confirm_tempo = 1;
}

static void
audio_ctlpipe_set_bpm(int bpm)
{
    xmplayer_set_bpm(bpm);
    if (confirm_bpm != 0) {
        /* confirm previous request */
        event_waiter_confirm(audio_bpm_ew, 0.0);
    }
    confirm_bpm = 1;
}

static void
audio_ctlpipe_set_pattern(int pattern)
{
    g_assert(playing);

    xmplayer_set_pattern(pattern);
}

void
audio_set_amplification(float af)
{
    g_assert(mixer != NULL);

    audio_ampfactor_f = af * 0.25;
    audio_ampfactor_i = lrint(af) << 3;
}

gint
audio_get_playback_rate()
{
    if (!playback_driver)
        return -1;
    return playback_driver->get_play_rate(playback_driver_object);
}

/* read()s on pipes are always non-blocking, so when we want to read 8
   bytes, we might only get 4. this routine is a workaround. seems
   like i can't put a pipe into blocking mode. alternatively the other
   end of the pipe could send packets in one big chunk instead of
   using single write()s. */
void readpipe(int fd, void* p, int count)
{
    struct pollfd pfd = { fd, POLLIN, 0 };
    int n;

    while (count) {
        n = read(fd, p, count);
        count -= n;
        p += n;
        if (count != 0) {
            pfd.revents = 0;
            poll(&pfd, 1, -1);
        }
    }
}

static void
audio_thread(void)
{
    gboolean result = FALSE;
    static gboolean skip = FALSE;
    audio_ctlpipe_id c;
    int a[7];
    void* b;
    float af;

    audio_raise_priority();

    while (1) {
        readpipe(ctlpipe, &c, sizeof(c));
        switch (c) {
        case AUDIO_CTLPIPE_INIT_PLAYER:
            audio_ctlpipe_init_player();
            break;
        case AUDIO_CTLPIPE_PLAY_SONG:
            readpipe(ctlpipe, a, 3 * sizeof(a[0]));
            skip = FALSE;
            audio_ctlpipe_play_song(a[0], a[1], a[2]);
            break;
        case AUDIO_CTLPIPE_PLAY_PATTERN:
            readpipe(ctlpipe, a, 7 * sizeof(a[0]));
            skip = FALSE;
            audio_ctlpipe_play_pattern(a[0], a[1], a[2], a[3], a[4], a[5], a[6]);
            break;
        case AUDIO_CTLPIPE_PLAY_NOTE:
            readpipe(ctlpipe, a, 4 * sizeof(a[0]));
            skip = FALSE;
            audio_ctlpipe_play_note(a[0], a[1], a[2], a[3]);
            break;
        case AUDIO_CTLPIPE_PLAY_NOTE_FULL:
            readpipe(ctlpipe, &a[0], 2 * sizeof(a[0]));
            readpipe(ctlpipe, &b, 1 * sizeof(b));
            readpipe(ctlpipe, &a[2], 5 * sizeof(a[0]));
            skip = FALSE;
            audio_ctlpipe_play_note_full(a[0], a[1], b, a[2], a[3], a[4], a[5], a[6]);
            break;
        case AUDIO_CTLPIPE_PLAY_NOTE_KEYOFF:
            readpipe(ctlpipe, a, 1 * sizeof(a[0]));
            skip = FALSE;
            audio_ctlpipe_play_note_keyoff(a[0]);
            break;
        case AUDIO_CTLPIPE_STOP_NOTE:
            readpipe(ctlpipe, a, 1 * sizeof(a[0]));
            skip = FALSE;
            audio_ctlpipe_stop_note(a[0]);
            break;
        case AUDIO_CTLPIPE_STOP_PLAYING:
            skip = TRUE;
            audio_ctlpipe_stop_playing();
            break;
        case AUDIO_CTLPIPE_SET_SONGPOS:
            result = (read(ctlpipe, a, 1 * sizeof(a[0])) != 1 * sizeof(a[0]));
            audio_ctlpipe_set_songpos(a[0]);
            break;
        case AUDIO_CTLPIPE_SET_PATTERN:
            result = (read(ctlpipe, a, 1 * sizeof(a[0])) != 1 * sizeof(a[0]));
            audio_ctlpipe_set_pattern(a[0]);
            break;
        case AUDIO_CTLPIPE_SET_AMPLIFICATION:
            result = (read(ctlpipe, &af, sizeof(af)) != sizeof(af));
            audio_set_amplification(af);
            break;
        case AUDIO_CTLPIPE_SET_PITCHBEND:
            result = (read(ctlpipe, &af, sizeof(af)) != sizeof(af));
            pitchbend_req = af;
            break;
        case AUDIO_CTLPIPE_SET_MIXER:
            result = (read(ctlpipe, &b, sizeof(b)) != sizeof(b));
            mixer = b;
            if (playing) {
                mixer->reset();
                mixfmt_req = -666;
                mixer->setnumch(audio_numchannels);
            }
            prev_bufsize = 0; /* To force buffers reallocation */
            break;
        case AUDIO_CTLPIPE_SET_TEMPO:
            readpipe(ctlpipe, a, 1 * sizeof(a[0]));
            audio_ctlpipe_set_tempo(a[0]);
            break;
        case AUDIO_CTLPIPE_SET_BPM:
            readpipe(ctlpipe, a, 1 * sizeof(a[0]));
            audio_ctlpipe_set_bpm(a[0]);
            break;
        case AUDIO_CTLPIPE_DATA_REQUESTED:
            readpipe(ctlpipe, &b, sizeof(b));
            readpipe(ctlpipe, a, 3 * sizeof(a[0]));
            if (!skip)
                audio_ctlpipe_mix(b, a[0], a[1], a[2]);
            break;
        default:
            fprintf(stderr, "\n\n*** audio_thread: unknown ctlpipe id %d\n\n\n", c);
            pthread_exit(NULL);
            break;
        }
        if (result)
            fprintf(stderr, "\n\n*** audio_thread: read incomplete\n\n\n");
    }
}

gboolean
audio_init(void)
{
    int i;

    if (!audio_communication_init(&ctlpipe))
        return FALSE;

    for (i = 0; i < 32; i++) {
        scopebufs[i] = NULL;
    }
    scopebuf_ready = FALSE;
    scopebuf_length = -1;

    memset(player_mute_channels, 0, sizeof(player_mute_channels));

    if (!(audio_playerpos_tb = time_buffer_new()))
        return FALSE;
    if (!(audio_clipping_indicator_tb = time_buffer_new()))
        return FALSE;
    if (!(audio_mixer_position_tb = time_buffer_new()))
        return FALSE;
    if (!(audio_channels_status_tb = time_buffer_new()))
        return FALSE;
    if (!(audio_songpos_ew = event_waiter_new()))
        return FALSE;
    if (!(audio_tempo_ew = event_waiter_new()))
        return FALSE;
    if (!(audio_bpm_ew = event_waiter_new()))
        return FALSE;

    if (0 == pthread_create(&threadid, NULL, (void* (*)(void*))audio_thread, NULL))
        return TRUE;

    return FALSE;
}

void audio_set_mixer(st_mixer* newmixer)
{
    audio_ctlpipe_write(AUDIO_CTLPIPE_SET_MIXER, newmixer);
}

static void
mixer_mix_format(STMixerFormat m, int s)
{
    g_assert(mixer != NULL);

    mixfmt_conv = 0;
    mixfmt = 0;

    switch (m) {
    case ST_MIXER_FORMAT_S16_BE:
    case ST_MIXER_FORMAT_S16_LE:
    case ST_MIXER_FORMAT_U16_BE:
    case ST_MIXER_FORMAT_U16_LE:
        if (mixer->setmixformat(16)) {
            mixfmt = MIXFMT_16;
        } else if (mixer->setmixformat(8)) {
            mixfmt_conv |= MIXFMT_CONV_TO_16;
        } else {
            g_error("Weird mixer. No 8 or 16 bits modes.\n");
        }
        break;
    case ST_MIXER_FORMAT_S8:
    case ST_MIXER_FORMAT_U8:
        if (mixer->setmixformat(8)) {
        } else if (mixer->setmixformat(16)) {
            mixfmt = MIXFMT_16;
            mixfmt_conv |= MIXFMT_CONV_TO_8;
        } else {
            g_error("Weird mixer. No 8 or 16 bits modes.\n");
        }
        break;
    default:
        g_error("Unknown argument for STMixerFormat.\n");
        break;
    }

    if (mixfmt & MIXFMT_16) {
        switch (m) {
#ifdef WORDS_BIGENDIAN
        case ST_MIXER_FORMAT_S16_LE:
        case ST_MIXER_FORMAT_U16_LE:
#else
        case ST_MIXER_FORMAT_S16_BE:
        case ST_MIXER_FORMAT_U16_BE:
#endif
            mixfmt_conv |= MIXFMT_CONV_BYTESWAP;
            break;
        default:
            break;
        }
    }

    switch (m) {
    case ST_MIXER_FORMAT_U16_LE:
    case ST_MIXER_FORMAT_U16_BE:
    case ST_MIXER_FORMAT_U8:
        mixfmt_conv |= MIXFMT_CONV_TO_UNSIGNED;
        break;
    default:
        break;
    }

    mixfmt |= s ? MIXFMT_STEREO : 0;
    if (!mixer->setstereo(s)) {
        if (s) {
            mixfmt &= ~MIXFMT_STEREO;
            mixfmt_conv |= MIXFMT_CONV_TO_STEREO;
        } else {
            mixfmt_conv |= MIXFMT_CONV_TO_MONO;
        }
    }
}

void audio_prepare_for_playing(void)
{
    int i;

    g_assert(mixer != NULL);

    mixer->reset();
    mixfmt_req = -666;
    pitchbend = pitchbend_req;

    playing = 1;
    playing_noloop = FALSE;

    audio_next_tick_time_bent = 0.0;
    audio_prev_tick_time = -1.0;
    audio_next_tick_time_unbent = 0.0;
    audio_current_playback_time_bent = 0.0;
    audio_mixer_current_time = 0.0;
    audio_prev_tempo = 0;

    time_buffer_clear(audio_playerpos_tb);
    time_buffer_clear(audio_clipping_indicator_tb);
    time_buffer_clear(audio_mixer_position_tb);
    time_buffer_clear(audio_channels_status_tb);
    audio_visual_feedback_counter = audio_visual_feedback_update_interval;
    audio_visual_feedback_clipping = 0;

    event_waiter_reset(audio_songpos_ew);
    event_waiter_reset(audio_tempo_ew);
    event_waiter_reset(audio_bpm_ew);

    scopebuf_start.offset = 0;
    scopebuf_start.time = 0.0;
    scopebuf_end.offset = 0;
    scopebuf_end.time = 0.0;
    scopebuf_ready = FALSE;
    if (gui_settings.scopes_buffer_size / 32 > scopebuf_length) {
        scopebuf_length = gui_settings.scopes_buffer_size / 32;
        for (i = 0; i < 32; i++) {
            g_free(scopebufs[i]);
            scopebufs[i] = g_new(gint16, scopebuf_length);
            if (!scopebufs[i])
                return;
        }
    }
    scopebuf_ready = TRUE;
}

static void*
mix(void* dest, const guint32 count, const gboolean stereo)
{
    guint32 num_processed, already_processed = 0;
    guint i, j, num_samples = stereo ? count << 1 : count;
    gint16* outbuf = dest;

    clipflag = FALSE;
    if (mixer->buffer_format == ST_MIXER_BUFFER_FORMAT_INT) {
        /* modules with many channels get additional amplification here */
        gint t = (4.0 * log(audio_numchannels) / log(4.0)) * 64.0 * 8.0;// TODO table

        for (i = 0; i < audio_numchannels; i++) {
            num_processed = stereo ? chan_buffers[i].num_processed << 1 : chan_buffers[i].num_processed;

            if (!num_processed)
                continue;
            for (j = 0; j < MIN(num_processed, already_processed); j++)
                ((gint*)mix_buffer)[j] += ((gint*)chan_buffers[i].buffer)[j];
            if (num_processed > already_processed) {
                for (j = already_processed; j < num_processed; j++)
                    ((gint*)mix_buffer)[j] = ((gint*)chan_buffers[i].buffer)[j];
                already_processed = num_processed;
            }
        }

        for (j = 0; j < already_processed; j++) {
            gint32 a, b;

            a = ((gint32*)mix_buffer)[j];
            a *= audio_ampfactor_i; /* amplify */
            a /= t;

            b = CLAMP(a, -32768, 32767);
            if (a != b) {
                clipflag = TRUE;
            }

            *outbuf++ = b;
        }
    } else {
        for (i = 0; i < audio_numchannels; i++) {
            num_processed = stereo ? (chan_buffers[i].num_processed << 1) : chan_buffers[i].num_processed;

            if (!num_processed)
                continue;
            for (j = 0; j < MIN(num_processed, already_processed); j++)
                ((float*)mix_buffer)[j] += ((float*)chan_buffers[i].buffer)[j];
            if (num_processed > already_processed) {
                for (j = already_processed; j < num_processed; j++)
                    ((float*)mix_buffer)[j] = ((float*)chan_buffers[i].buffer)[j];
                already_processed = num_processed;
            }
        }

        for (j = 0; j < already_processed; j++) {
            float a = ((float*)mix_buffer)[j] * audio_ampfactor_f;

            if (a < -32768.0) {
                a = -32768.0;
                clipflag = TRUE;
            }
            if (a > 32767.0) {
                a = 32767.0;
                clipflag = TRUE;
            }
            *outbuf++ = (gint16)a;
        }
    }
    /* Clear the rest of the buffer (if necessary) */
    if (already_processed < num_samples) {
        already_processed = num_samples - already_processed;
        /* We are forced to clear the rest of buffer since it is not rendered */
        memset(outbuf, 0, already_processed << 1);/* 16 bit */
    }

    return dest + (num_samples << 1);
}

static void*
mixer_mix_and_handle_scopes(void* dest,
    guint32 count,
    gboolean simple)
{
    int n;
    audio_clipping_indicator* c;
    audio_mixer_position* p;
    gboolean stereo = (mixfmt_conv & MIXFMT_CONV_TO_MONO) || (mixfmt & MIXFMT_STEREO);

    /* Check buffers' size and update if necessary (tracer doesn't need this) */
    if (mixer->setbuffers && count > prev_bufsize) {
        guint i;
        gsize ssize = mixer_get_buffer_sizeof(mixer->buffer_format);

        if (!prev_bufsize) /* Pref_bufsize == 0 indicates mixer change */
            mixer->setbuffers(chan_buffers);
        prev_bufsize = count << 1;
        for (i = 0; i < audio_numchannels; i++) {
            if (chan_buffers[i].buffer)
                g_free(chan_buffers[i].buffer);
            chan_buffers[i].buffer = calloc(prev_bufsize,  stereo ? ssize << 1 : ssize);
        }
        if (mix_buffer)
            g_free(mix_buffer);
        mix_buffer = calloc(prev_bufsize, stereo  ? ssize << 1 : ssize);
    }

    // See comments in audio.h for Oscilloscope stuff

    if (simple) {
        mixer->render(count, NULL, 0, NULL, 0.0);
        return mix(dest, count, stereo);
    }

    while (count) {
        n = scopebuf_length - scopebuf_end.offset;
        if (n > count)
            n = count;

        if (n > audio_visual_feedback_counter) {
            n = audio_visual_feedback_counter;
        }

        gui_settings.gui_display_scopes && scopebuf_ready ?
            mixer->render(n, scopebufs, scopebuf_end.offset,
                audio_channels_status_tb, audio_current_playback_time_bent) :
            mixer->render(n, NULL, 0,
                audio_channels_status_tb, audio_current_playback_time_bent);
        dest = mix(dest, n, stereo);

        scopebuf_end.offset += n;
        scopebuf_end.time += (double)n / scopebuf_freq;
        audio_mixer_current_time += (double)n / scopebuf_freq;
        count -= n;
        audio_visual_feedback_counter -= n;

        if (scopebuf_end.offset == scopebuf_length) {
            scopebuf_end.offset = 0;
        }

        if (clipflag) {
            audio_visual_feedback_clipping = audio_visual_feedback_smear_clipping;
        }

        if (audio_visual_feedback_counter == 0) {
            /* Get up-to-date info from mixer about current sample positions */
            audio_visual_feedback_counter = audio_visual_feedback_update_interval;
            if ((p = g_new(audio_mixer_position, 1))) {
                mixer->dumpstatus(p->dump);
                time_buffer_add(audio_mixer_position_tb, p, audio_mixer_current_time);
            }
            if ((c = g_new(audio_clipping_indicator, 1))) {
                c->clipping = audio_visual_feedback_clipping;
                if (audio_visual_feedback_clipping) {
                    audio_visual_feedback_clipping--;
                }
                time_buffer_add(audio_clipping_indicator_tb, c, audio_mixer_current_time);
            }
        }

        if (scopebuf_end.time - scopebuf_start.time >= (double)scopebuf_length / scopebuf_freq) {
            // adjust scopebuf_start
            int d = scopebuf_end.offset - scopebuf_start.offset;
            if (d < 0)
                d += scopebuf_length;
            g_assert(d >= 0);
            scopebuf_start.offset += d;
            scopebuf_start.offset %= scopebuf_length;
            scopebuf_start.time += (double)d / scopebuf_freq;
        }
    }

    return dest;
}

static void*
mixer_mix(void* dest,
    guint32 count,
    gboolean simple)
{
    static int bufsize = 0;
    static void* buf = NULL;
    int b, i, c, d;
    void* ende;

    if (count == 0)
        return dest;

    g_assert(mixer != NULL);

    /* The mixer doesn't have to support a format that the driver
       requires. This routine converts between any formats if
       necessary: 16 bits / 8 bits, mono / stereo, little endian / big
       endian, unsigned / signed */

    if (mixfmt_conv == 0) {
        return mixer_mix_and_handle_scopes(dest, count, simple);
    }

    b = count;
    c = 8;
    d = 1;
    if (mixfmt & MIXFMT_16) {
        b *= 2;
        c = 16;
    }
    if ((mixfmt & MIXFMT_STEREO) || (mixfmt_conv & MIXFMT_CONV_TO_MONO)) {
        b *= 2;
        d = 2;
    }

    if (b > bufsize) {
        g_free(buf);
        buf = g_new(guint8, b);
        bufsize = b;
    }

    g_assert(buf != NULL);
    ende = mixer_mix_and_handle_scopes(buf, count, simple);

    if (mixfmt_conv & MIXFMT_CONV_TO_MONO) {
        if (mixfmt & MIXFMT_16) {
            gint16 *a = buf, *b = buf;
            for (i = 0; i < count; i++, a += 2, b += 1)
                *b = (a[0] + a[1]) / 2;
        } else {
            gint8 *a = buf, *b = buf;
            for (i = 0; i < count; i++, a += 2, b += 1)
                *b = (a[0] + a[1]) / 2;
        }
        d = 1;
    }

    if (mixfmt_conv & MIXFMT_CONV_TO_8) {
        gint16* a = buf;
        gint8* b = dest;
        for (i = 0; i < count * d; i++)
            *b++ = *a++ >> 8;
        c = 8;
        ende = b;
    } else if (mixfmt_conv & MIXFMT_CONV_TO_16) {
        gint8* a = buf;
        gint16* b = dest;
        for (i = 0; i < count * d; i++)
            *b++ = *a++ << 8;
        c = 16;
        ende = b;
    } else {
        memcpy(dest, buf, count * d * (c / 8));
        ende = dest + (count * d * (c / 8));
    }

    if (mixfmt_conv & MIXFMT_CONV_TO_UNSIGNED) {
        if (c == 16) {
            gint16* a = dest;
            guint16* b = dest;
            for (i = 0; i < count * d; i++)
                *b++ = *a++ + 32768;
        } else {
            gint8* a = dest;
            guint8* b = dest;
            for (i = 0; i < count * d; i++)
                *b++ = *a++ + 128;
        }
    }

    if (mixfmt_conv & MIXFMT_CONV_BYTESWAP) {
        le_16_array_to_host_order(dest, count * d);
    }

    if (mixfmt_conv & MIXFMT_CONV_TO_STEREO) {
        g_assert(d == 1);
        if (c == 16) {
            gint16 *a = dest, *b = dest;
            ende = b;
            for (i = 0, a += count, b += 2 * count; i < count; i++, a -= 1, b -= 2)
                b[-1] = b[-2] = a[-1];
        } else {
            gint8 *a = dest, *b = dest;
            ende = b;
            for (i = 0, a += count, b += 2 * count; i < count; i++, a -= 1, b -= 2)
                b[-1] = b[-2] = a[-1];
        }
    }

    return ende;
}

void driver_setnumch(int numchannels)
{
    g_assert(numchannels >= 1 && numchannels <= 32);
    audio_numchannels = numchannels;
    mixer->setnumch(numchannels);
    prev_bufsize = 0;
}

void driver_startnote(const gint channel,
    st_mixer_sample_info* si,
    const gint inst,
    const gint smpl,
    const gint note)
{
    if (si->length != 0) {
        audio_channel_status* p;

        p = g_new(audio_channel_status, 1);
        p->command = AUDIO_COMMAND_START_PLAYING;
        p->channel = channel;
        p->instr = inst;
        p->sample = smpl;
        p->note = note;
        time_buffer_add(audio_channels_status_tb, p,
            audio_current_playback_time_bent);

        mixer->startnote(channel, si);
    }
}

void driver_stopnote(int channel)
{
    audio_channel_status* p;

    mixer->stopnote(channel);

    p = g_new(audio_channel_status, 1);
    p->command = AUDIO_COMMAND_STOP_PLAYING;
    p->channel = channel;
    time_buffer_add(audio_channels_status_tb, p,
        audio_current_playback_time_bent);
}

void driver_setsmplpos(int channel,
    guint32 offset)
{
    mixer->setsmplpos(channel, offset);
}

void driver_setsmplend(int channel,
    guint32 offset)
{
    mixer->setsmplend(channel, offset);
}

void driver_setfreq(int channel,
    float frequency)
{
    mixer->setfreq(channel, frequency * ((100.0 + pitchbend) / 100.0));
}

void driver_setvolume(int channel,
    float volume)
{
    g_assert(volume >= 0.0 && volume <= 1.0);

    mixer->setvolume(channel, volume);
}

void driver_setpanning(int channel,
    float panning)
{
    g_assert(panning >= -1.0 && panning <= +1.0);

    mixer->setpanning(channel, panning);
}

void driver_set_ch_filter_freq(int channel,
    float freq)
{
    if (mixer->setchcutoff) {
        mixer->setchcutoff(channel, freq);
    }
}

void driver_set_ch_filter_reso(int channel,
    float freq)
{
    if (mixer->setchreso) {
        mixer->setchreso(channel, freq);
    }
}

guint32 audio_mix(void* dest,
    const guint32 count,
    const gint mixfreq,
    const gint mixformat,
    const gboolean full,
    gboolean* clipping)
{
    int nonewtick = FALSE;
    gint count_cur = count;
    static gboolean stop_issued = FALSE;

    if (!(playing_noloop && player_looped))
        stop_issued = FALSE;

    // Set mixer parameters
    if (mixfmt_req != mixformat) {
        mixfmt_req = mixformat;
        mixer_mix_format(mixformat & 15, (mixformat & ST_MIXER_FORMAT_STEREO) != 0);
    }
    scopebuf_freq = mixfreq_req = mixfreq;
    mixer->setmixfreq(mixfreq);

    audio_visual_feedback_update_interval = mixfreq / audio_visual_feedback_updates_per_second;

    while (count_cur) {
        audio_player_pos* p;
        // Mix either until the next time is reached when we should call the XM player,
        // or until the current mixing buffer is full.
        int samples_left = (audio_next_tick_time_bent - audio_current_playback_time_bent) * mixfreq;

        if (samples_left > count_cur) {
            // No new player tick this time...
            samples_left = count_cur;
            nonewtick = TRUE;
        }

        if (playing_noloop && player_looped) {
            if (full) {
                /* "noloop" playing mode, make rest of buffer silent */
                memset(dest, 0,
                    samples_left * ((mixfmt & MIXFMT_16) ? 2 : 1)
                        * ((mixfmt & MIXFMT_STEREO) ? 2 : 1));

                if (!stop_issued) {
                    stop_issued = TRUE;
                    p = g_new(audio_player_pos, 1);
                    /* Issue "STOP_PLAYING" synchronous command
                       with time corresponding to the last non-empty sample */
                    if (p) {
                        p->command = AUDIO_COMMAND_STOP_PLAYING;
                        time_buffer_add(audio_playerpos_tb, p, audio_current_playback_time_bent);
                    }
                }
            } else
                return count - count_cur;
        } else {
            dest = mixer_mix(dest, samples_left, !full);
            if (clipping)
                *clipping = clipflag;
        }
        count_cur -= samples_left;
        audio_current_playback_time_bent += (double)samples_left / mixfreq;

        if (!nonewtick) {
            double t;

            // Pitchbend variable must be updated directly before or after a tick,
            // not in the middle of a filled mixing buffer.
            if (pitchbend_req != pitchbend) {
                pitchbend = pitchbend_req;
            }

            // The following three lines, and the stuff in driver_setfreq() contain all
            // necessary code to handle the pitchbending feature.
            t = xmplayer_play(FALSE);
            audio_next_tick_time_bent += (t - audio_next_tick_time_unbent) * (100.0 / (100.0 + pitchbend));
            audio_next_tick_time_unbent = t;

            if (full && !(playing_noloop && player_looped)) {
                p = g_new(audio_player_pos, 1);

                // Update player position time buffer
                if (p) {
                    p->command = AUDIO_COMMAND_NONE;
                    p->songpos = player_songpos;
                    p->patpos = player_patpos;
                    p->patno = player_patno;
                    p->tempo = player_tempo;
                    p->prev_tempo = audio_prev_tempo;
                    audio_prev_tempo = player_tempo;
                    p->bpm = player_bpm;
                    p->curtick = curtick;
                    p->next_tick_time = audio_next_tick_time_bent;
                    p->prev_tick_time = audio_prev_tick_time;
                    audio_prev_tick_time = audio_current_playback_time_bent;
                    time_buffer_add(audio_playerpos_tb, p, audio_current_playback_time_bent);
                }

                // Confirm pending event requests
                if (set_songpos_wait_for != -1 && player_songpos == set_songpos_wait_for) {
                    event_waiter_confirm(audio_songpos_ew, audio_current_playback_time_bent);
                    set_songpos_wait_for = -1;
                }
                if (confirm_tempo) {
                    event_waiter_confirm(audio_tempo_ew, audio_current_playback_time_bent);
                    confirm_tempo = 0;
                }
                if (confirm_bpm) {
                    event_waiter_confirm(audio_bpm_ew, audio_current_playback_time_bent);
                    confirm_bpm = 0;
                }
            }
        }
    }
    return count;
}
