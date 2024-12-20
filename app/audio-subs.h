
/*
 * The Real SoundTracker - Audio thread communication stuff (header)
 *
 * Copyright (C) 2020 Yury Aliaev
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

#ifndef _ST_AUDIO_SUBS_H
#define _ST_AUDIO_SUBS_H

#include <glib.h>

typedef enum audio_ctlpipe_id {
    AUDIO_CTLPIPE_INIT_PLAYER = 2000, /* void */
    AUDIO_CTLPIPE_PLAY_SONG, /* int songpos, int patpos, int looped */
    AUDIO_CTLPIPE_PLAY_PATTERN, /* int pattern, int patpos, int only_one_row, int looped, int stoppos,
                                   int from_channel, int num_channels */
    AUDIO_CTLPIPE_PLAY_NOTE, /* int channel, int note, int instrument, int all */
    AUDIO_CTLPIPE_PLAY_NOTE_FULL, /* int channel, int note, STSample *sample, int offset, int count, int use_all_channels */
    AUDIO_CTLPIPE_PLAY_NOTE_KEYOFF, /* int channel */
    AUDIO_CTLPIPE_STOP_NOTE, /* int channel */
    AUDIO_CTLPIPE_STOP_PLAYING, /* void */
    AUDIO_CTLPIPE_SET_SONGPOS, /* int songpos */
    AUDIO_CTLPIPE_SET_PATTERN, /* int pattern */
    AUDIO_CTLPIPE_SET_AMPLIFICATION, /* double */
    AUDIO_CTLPIPE_SET_PITCHBEND, /* double */
    AUDIO_CTLPIPE_SET_MIXER, /* st_mixer* */
    AUDIO_CTLPIPE_SET_TEMPO, /* int */
    AUDIO_CTLPIPE_SET_BPM, /* int */
    AUDIO_CTLPIPE_DATA_REQUESTED /* pointer, int, int, int */
} audio_ctlpipe_id;

typedef enum audio_backpipe_id {
    AUDIO_BACKPIPE_DRIVER_OPEN_FAILED = 1000,
    AUDIO_BACKPIPE_PLAYING_STARTED,
    AUDIO_BACKPIPE_PLAYING_PATTERN_STARTED,
    AUDIO_BACKPIPE_PLAYING_NOTE_STARTED,
    AUDIO_BACKPIPE_PLAYING_STOPPED,
    AUDIO_BACKPIPE_ERROR_MESSAGE, /* int len, string (len+1 bytes) */
    AUDIO_BACKPIPE_WARNING_MESSAGE, /* int len, string (len+1 bytes) */
    AUDIO_BACKPIPE_ERRNO_MESSAGE, /* int errno, int len, string (len+1 bytes) */
} audio_backpipe_id;

void audio_ctlpipe_write(audio_ctlpipe_id cmd, ...);
void audio_backpipe_write(audio_backpipe_id cmd, ...);
gboolean audio_request_data(void *buf,
    guint32 count,
    gint mixfreq,
    gint mixformat);
gboolean audio_communication_init(int* ctlpipe);
int audio_get_backpipe(void);

#endif /* _ST_AUDIO_SUBS_H */
