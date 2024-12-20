
/*
 * The Real SoundTracker - Audio thread communication stuff
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

#include <stdio.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "audio-subs.h"
#include "errors.h"


struct _ip {
    audio_ctlpipe_id cmd;
} __attribute__((packed)); /* To prevent addition of spacers between fields */

struct _ps {
    audio_ctlpipe_id cmd;
    gint songpos, patpos, looped;
} __attribute__((packed));

struct _pp {
    audio_ctlpipe_id cmd;
    gint pattern, patpos, only1row, looped, stoppos, ch_start, num_ch;
} __attribute__((packed));

struct _pn {
    audio_ctlpipe_id cmd;
    gint channel, note, instr, all;
} __attribute__((packed));

struct _pnf {
    audio_ctlpipe_id cmd;
    gint channel, note;
    gpointer sample;
    gint offset, count, all_channels, instr, smpno;
} __attribute__((packed));

struct _pko {
    audio_ctlpipe_id cmd;
    gint channel;
} __attribute__((packed));

struct _sn {
    audio_ctlpipe_id cmd;
    gint channel;
} __attribute__((packed));

struct _sp {
    audio_ctlpipe_id cmd;
} __attribute__((packed));

struct _ss {
    audio_ctlpipe_id cmd;
    gint songpos;
} __attribute__((packed));

struct _spt {
    audio_ctlpipe_id cmd;
    gint pattern;
} __attribute__((packed));

struct _sa {
    audio_ctlpipe_id cmd;
    gfloat amplification;
} __attribute__((packed));

struct _spb {
    audio_ctlpipe_id cmd;
    gfloat pitchbend;
} __attribute__((packed));

struct _sm {
    audio_ctlpipe_id cmd;
    gpointer mixer;
} __attribute__((packed));

struct _st {
    audio_ctlpipe_id cmd;
    gint tempo;
} __attribute__((packed));

struct _sbpm {
    audio_ctlpipe_id cmd;
    gint bpm;
} __attribute__((packed));

struct _dr {
    audio_ctlpipe_id cmd;
    void* buf;
    gint fragsize, mixfreq, mixformat;
} __attribute__((packed));

union audio_ctlpipe_args {
    struct _ip ip;
    struct _ps ps;
    struct _pp pp;
    struct _pn pn;
    struct _pnf pnf;
    struct _pko pko;
    struct _sn sn;
    struct _sp sp;
    struct _ss ss;
    struct _spt spt;
    struct _sa sa;
    struct _spb spb;
    struct _sm sm;
    struct _st st;
    struct _sbpm sbpm;
    struct _dr dr;
};

union audio_backpipe_args {
    struct _cmdtext {
        audio_backpipe_id cmd;
        gint length;
        gchar text[1];
    } __attribute__((packed)) cmdtext;

    struct _cmderrno {
        audio_backpipe_id cmd;
        gint errno_;
        gint length;
        gchar text[1];
    } __attribute__((packed)) cmderrno;
};

static int ctlpipe[2], backpipe[2];
static gboolean initialized = FALSE;

void audio_ctlpipe_write(audio_ctlpipe_id cmd, ...)
{
    union audio_ctlpipe_args args;
    va_list arg_list;
    size_t arg_size;

    va_start(arg_list, cmd);
    switch (cmd) {
    case AUDIO_CTLPIPE_INIT_PLAYER:
        args.ip.cmd = AUDIO_CTLPIPE_INIT_PLAYER;
        arg_size = sizeof(args.ip);
        break;
    case AUDIO_CTLPIPE_PLAY_SONG:
        args.ps.cmd = AUDIO_CTLPIPE_PLAY_SONG;
        args.ps.songpos = va_arg(arg_list, gint);
        args.ps.patpos = va_arg(arg_list, gint);
        args.ps.looped = va_arg(arg_list, gint);
        arg_size = sizeof(args.ps);
        break;
    case AUDIO_CTLPIPE_PLAY_PATTERN:
        args.pp.cmd = AUDIO_CTLPIPE_PLAY_PATTERN;
        args.pp.pattern = va_arg(arg_list, gint);
        args.pp.patpos = va_arg(arg_list, gint);
        args.pp.only1row = va_arg(arg_list, gint);
        args.pp.looped = va_arg(arg_list, gint);
        args.pp.stoppos = va_arg(arg_list, gint);
        args.pp.ch_start = va_arg(arg_list, gint);
        args.pp.num_ch = va_arg(arg_list, gint);
        arg_size = sizeof(args.pp);
        break;
    case AUDIO_CTLPIPE_PLAY_NOTE:
        args.pn.cmd = AUDIO_CTLPIPE_PLAY_NOTE;
        args.pn.channel = va_arg(arg_list, gint);
        args.pn.note = va_arg(arg_list, gint);
        args.pn.instr = va_arg(arg_list, gint);
        args.pn.all = va_arg(arg_list, gint);
        arg_size = sizeof(args.pn);
        break;
    case AUDIO_CTLPIPE_PLAY_NOTE_FULL:
        args.pnf.cmd = AUDIO_CTLPIPE_PLAY_NOTE_FULL;
        args.pnf.channel = va_arg(arg_list, gint);
        args.pnf.note = va_arg(arg_list, gint);
        args.pnf.sample = va_arg(arg_list, gpointer);
        args.pnf.offset = va_arg(arg_list, gint);
        args.pnf.count = va_arg(arg_list, gint);
        args.pnf.all_channels = va_arg(arg_list, gint);
        args.pnf.instr = va_arg(arg_list, gint);
        args.pnf.smpno = va_arg(arg_list, gint);
        arg_size = sizeof(args.pnf);
        break;
    case AUDIO_CTLPIPE_PLAY_NOTE_KEYOFF:
        args.pko.cmd = AUDIO_CTLPIPE_PLAY_NOTE_KEYOFF;
        args.pko.channel = va_arg(arg_list, gint);
        arg_size = sizeof(args.pko);
        break;
    case AUDIO_CTLPIPE_STOP_NOTE:
        args.sn.cmd = AUDIO_CTLPIPE_STOP_NOTE;
        args.sn.channel = va_arg(arg_list, gint);
        arg_size = sizeof(args.sn);
        break;
    case AUDIO_CTLPIPE_STOP_PLAYING:
        args.sp.cmd = AUDIO_CTLPIPE_STOP_PLAYING;
        arg_size = sizeof(args.sp);
        break;
    case AUDIO_CTLPIPE_SET_SONGPOS:
        args.ss.cmd = AUDIO_CTLPIPE_SET_SONGPOS;
        args.ss.songpos = va_arg(arg_list, gint);
        arg_size = sizeof(args.ss);
        break;
    case AUDIO_CTLPIPE_SET_PATTERN:
        args.spt.cmd = AUDIO_CTLPIPE_SET_PATTERN;
        args.spt.pattern = va_arg(arg_list, gint);
        arg_size = sizeof(args.spt);
        break;
    case AUDIO_CTLPIPE_SET_AMPLIFICATION:
        args.sa.cmd = AUDIO_CTLPIPE_SET_AMPLIFICATION;
        args.sa.amplification = va_arg(arg_list, double);
        arg_size = sizeof(args.sa);
        break;
    case AUDIO_CTLPIPE_SET_PITCHBEND:
        args.spb.cmd = AUDIO_CTLPIPE_SET_PITCHBEND;
        args.spb.pitchbend = va_arg(arg_list, double);
        arg_size = sizeof(args.spb);
        break;
    case AUDIO_CTLPIPE_SET_MIXER:
        args.sm.cmd = AUDIO_CTLPIPE_SET_MIXER;
        args.sm.mixer = va_arg(arg_list, gpointer);
        arg_size = sizeof(args.sm);
        break;
    case AUDIO_CTLPIPE_SET_TEMPO:
        args.st.cmd = AUDIO_CTLPIPE_SET_TEMPO;
        args.st.tempo = va_arg(arg_list, gint);
        arg_size = sizeof(args.st);
        break;
    case AUDIO_CTLPIPE_SET_BPM:
        args.sbpm.cmd = AUDIO_CTLPIPE_SET_BPM;
        args.sbpm.bpm = va_arg(arg_list, gint);
        arg_size = sizeof(args.sbpm);
        break;
    case AUDIO_CTLPIPE_DATA_REQUESTED:
        args.dr.cmd = AUDIO_CTLPIPE_DATA_REQUESTED;
        args.dr.buf = va_arg(arg_list, gpointer);
        args.dr.fragsize = va_arg(arg_list, gint);
        args.dr.mixfreq = va_arg(arg_list, gint);
        args.dr.mixformat = va_arg(arg_list, gint);
        arg_size = sizeof(args.dr);
        break;
    default:
        g_assert_not_reached();
    }
    va_end(arg_list);

    if (write(ctlpipe[1], &args, arg_size) != arg_size)
        /* Since this can be called from various threads */
        error_error(_("Connection with audio thread failed!"));
}

void audio_backpipe_write(audio_backpipe_id cmd, ...)
{
    va_list arg_list;
    size_t arg_size, l;
    gchar* line;
    void* arg_pointer;
    union audio_backpipe_args* args;

    va_start(arg_list, cmd);
    switch (cmd) {
    case AUDIO_BACKPIPE_DRIVER_OPEN_FAILED ... AUDIO_BACKPIPE_PLAYING_STOPPED:
        arg_pointer = &cmd;
        arg_size = sizeof(cmd);
        break;
    case AUDIO_BACKPIPE_ERROR_MESSAGE ... AUDIO_BACKPIPE_WARNING_MESSAGE:
        line = va_arg(arg_list, gchar*);
        l = strlen(line); /* Whithout trailing zero */
        /* args->cmdtext.text already has 1 byte size, so we add only string length
           without trailing zero */
        arg_size = l + sizeof(args->cmdtext);
        args = arg_pointer = alloca(arg_size);
        args->cmdtext.cmd = cmd;
        args->cmdtext.length = l;
        strncpy(args->cmdtext.text, line, l + 1);
        break;
    case AUDIO_BACKPIPE_ERRNO_MESSAGE:
        line = va_arg(arg_list, gchar*);
        l = strlen(line);
        arg_size = l + sizeof(args->cmderrno);
        args = arg_pointer = alloca(arg_size);
        args->cmderrno.cmd = cmd;
        args->cmderrno.errno_ = va_arg(arg_list, gint);
        args->cmderrno.length = l;
        strncpy(args->cmderrno.text, line, l + 1);
        break;
    default:
        g_assert_not_reached();
    }
    va_end(arg_list);

    if (write(backpipe[1], arg_pointer, arg_size) != arg_size)
        perror("\n\n*** audio_thread: write incomplete");
}

gboolean audio_communication_init(int* c)
{
    if (pipe(ctlpipe) || pipe(backpipe)) {
        fprintf(stderr, "Cr\xc3\xa4nk. Can't pipe().\n");
        return FALSE;
    }

    *c = ctlpipe[0];
    initialized = TRUE;
    return TRUE;
}

int audio_get_backpipe(void)
{
    g_assert(initialized);
    return backpipe[0];
}

/* It returns boolean to have the same format as sample_editor_sampled() */
gboolean audio_request_data(void *buf,
    guint32 count,
    gint mixfreq,
    gint mixformat)
{
    audio_ctlpipe_write(AUDIO_CTLPIPE_DATA_REQUESTED,
        buf, (gint)count, mixfreq, mixformat);

    return TRUE;
}
