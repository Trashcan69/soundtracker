
/*
 * The Real SoundTracker - XM player (header)
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

#ifndef _ST_XMPLAYER_H
#define _ST_XMPLAYER_H

#include <glib.h>

#include "xm.h"

enum {
    xmpCmdArpeggio = 0,
    xmpCmdPortaU = 1,
    xmpCmdPortaD = 2,
    xmpCmdPortaNote = 3,
    xmpCmdVibrato = 4,
    xmpCmdPortaVol = 5,
    xmpCmdVibVol = 6,
    xmpCmdTremolo = 7,
    xmpCmdPanning = 8,
    xmpCmdOffset = 9,
    xmpCmdVolSlide = 10,
    xmpCmdJump = 11,
    xmpCmdVolume = 12,
    xmpCmdBreak = 13,
    xmpCmdExtended = 14,
    xmpCmdSpeed = 15,
    xmpCmdGVolume = 16,
    xmpCmdGVolSlide = 17,
    xmpCmdKeyOff = 20,
    xmpCmdEnvPos = 21,
    xmpCmdPanSlide = 25,
    xmpCmdSetFReso = 26,
    xmpCmdMRetrigger = 27,
    xmpCmdTremor = 29,
    xmpCmdXPorta = 33,
    xmpCmdSetFCutoff = 35,
    xmpCmdSFilter = 36,
    xmpCmdFPortaU = 37,
    xmpCmdFPortaD = 38,
    xmpCmdGlissando = 39,
    xmpCmdVibType = 40,
    xmpCmdSFinetune = 41,
    xmpCmdPatLoop = 42,
    xmpCmdTremType = 43,
    xmpCmdSPanning = 44,
    xmpCmdRetrigger = 45,
    xmpCmdFVolSlideU = 46,
    xmpCmdFVolSlideD = 47,
    xmpCmdNoteCut = 48,
    xmpCmdDelayNote = 49,
    xmpCmdPatDelay = 50,
    xmpCmdSetFHFCutoff = 31,
    xmpCmdSetFHFReso = 32,
    xmpVCmdVol0x = 1,
    xmpVCmdVol1x = 2,
    xmpVCmdVol2x = 3,
    xmpVCmdVol3x = 4,
    xmpVCmdVol40 = 5,
    xmpVCmdVolSlideD = 6,
    xmpVCmdVolSlideU = 7,
    xmpVCmdFVolSlideD = 8,
    xmpVCmdFVolSlideU = 9,
    xmpVCmdVibRate = 10,
    xmpVCmdVibDep = 11,
    xmpVCmdPanning = 12,
    xmpVCmdPanSlideL = 13,
    xmpVCmdPanSlideR = 14,
    xmpVCmdPortaNote = 15,
    xmpCmdMODtTempo = 128,
};

/* Making an extended FX parameter from a sub-command and a sub-parameter */
#define XMP_CMD_EXTENDED(cmd, param) (((cmd - 36) << 4) | (param & 0xf))

extern int player_songpos, player_patpos, player_patno;
extern int player_tempo, player_bpm;
extern gboolean player_looped;
extern guint8 curtick;

void xmplayer_init_module(void);
gboolean xmplayer_init_play_song(int songpos, int patpos, gboolean initall);
gboolean xmplayer_init_play_pattern(int pattern,
    int patpos,
    int only1row,
    int stoppos,
    int ch_start,
    int num_ch);
gboolean xmplayer_play_note(int channel, int note, int instrument, gboolean all);
gboolean xmplayer_play_note_full(const gint channel,
    const gint note,
    STSample* sample,
    const guint32 offset,
    const guint32 playend,
    const gboolean use_all_channels,
    const gint inst,
    const gint smpl);
void xmplayer_play_note_keyoff(int channel);
void xmplayer_stop_note(int channel);
double xmplayer_play(const gboolean simulation);
void xmplayer_stop(void);
void xmplayer_set_songpos(int songpos);
void xmplayer_set_pattern(int pattern);
void xmplayer_set_tempo(int tempo);
void xmplayer_set_bpm(int bpm);

#endif /* _ST_XMPLAYER_H */
