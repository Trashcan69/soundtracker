
/*
 * The Real SoundTracker - GTK+ sample display widget (header)
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

#ifndef _SAMPLE_DISPLAY_H
#define _SAMPLE_DISPLAY_H

#include "mixer.h"
#include "customdrawing.h"
#include "draw-interlayer.h"

/* I (ya) hope that the mixer and system endianesses are consistent
   and don't regard (rare?) cases when the system endianess differs
   from that of the peripheral HW */
#ifdef WORDS_BIGENDIAN
  #define ST_MIXER_FORMAT_S16 ST_MIXER_FORMAT_S16_BE
  #define ST_MIXER_FORMAT_U16 ST_MIXER_FORMAT_U16_BE
#else
  #define ST_MIXER_FORMAT_S16 ST_MIXER_FORMAT_S16_LE
  #define ST_MIXER_FORMAT_U16 ST_MIXER_FORMAT_U16_LE
#endif

typedef struct _STSampleChain STSampleChain;
struct _STSampleChain {
    STSampleChain* next;
    guint length;
    void* data;
};

typedef enum {
    SAMPLE_DISPLAY_MODE_STROBO = 0,
    SAMPLE_DISPLAY_MODE_MINMAX,
    SAMPLE_DISPLAY_MODE_LAST
} SampleDisplayMode;

G_BEGIN_DECLS

#define TYPE_SAMPLE_DISPLAY (sample_display_get_type())
#define SAMPLE_DISPLAY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_SAMPLE_DISPLAY, SampleDisplay))
#define SAMPLE_DISPLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST(klass, sample_display_get_type(), SampleDisplayClass))
#define IS_SAMPLE_DISPLAY(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, sample_display_get_type()))
#define SAMPLE_DISPLAY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), sample_display_get_type(), SampleDisplayClass))

typedef struct _SampleDisplay SampleDisplay;
typedef struct _SampleDisplayClass SampleDisplayClass;
typedef struct _SampleDisplayPriv SampleDisplayPriv;

struct _SampleDisplay {
    CustomDrawing widget;
    SampleDisplayPriv *priv;

    int width, height;
    int win_start, win_length;
    int sel_start, sel_end; /* offsets into the sample data or -1 */
};

struct _SampleDisplayClass {
    CustomDrawingClass parent_class;

    void (*selection_changed)(SampleDisplay* s, int start, int end);
    void (*loop_changed)(SampleDisplay* s, int start, int end);
    void (*window_changed)(SampleDisplay* s, int start, int end);
    void (*position_changed)(SampleDisplay *s, int pos);
};

GType sample_display_get_type(void) G_GNUC_CONST;
GtkWidget* sample_display_new(const gboolean edit, const SampleDisplayMode mode);

void sample_display_set_data(SampleDisplay* s, void* data, STMixerFormat type, int len, gboolean copy);
void sample_display_set_loop(SampleDisplay* s, int start, int end);
void sample_display_set_selection(SampleDisplay* s, int start, int end);
void sample_display_set_chain(SampleDisplay* s, STSampleChain* chain, STMixerFormat type);

void sample_display_enable_zero_line(SampleDisplay* s, gboolean enable);
void sample_display_set_mode(SampleDisplay* s, const SampleDisplayMode mode);
void sample_display_set_edit(SampleDisplay* s, const gboolean edit);

/* "end" can be negative indicating that the window size is not changed (only start);
   also no signal is being emitted in this case */
void sample_display_set_window(SampleDisplay* s, int start, int end);
gint sample_display_xpos_to_offset(SampleDisplay* s, const gint x);
gint sample_display_offset_to_xpos(SampleDisplay* s, const gint offset);

G_END_DECLS

#endif /* _SAMPLE_DISPLAY_H */
