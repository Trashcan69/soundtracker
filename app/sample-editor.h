
/*
 * The Real SoundTracker - sample editor (header)
 *
 * Copyright (C) 1998-2019 Michael Krause
 * Copyright (C) 2020-2022 Yury Aliaev
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

#ifndef _SAMPLE_EDITOR_H
#define _SAMPLE_EDITOR_H

#include "driver.h"
#include "sample-display.h"
#include "xm.h"
#include <gtk/gtk.h>

typedef enum {
    SAMPLE_EDITOR_DISPLAY_EDITOR = 0,
    SAMPLE_EDITOR_DISPLAY_SAMPLING
} SampleEditorDisplayInstance;

typedef enum {
    SAMPLE_EDITOR_VOLUME = 0,
    SAMPLE_EDITOR_PANNING,
    SAMPLE_EDITOR_FINETUNE,
    SAMPLE_EDITOR_RELNOTE,
    SAMPLE_EDITOR_LOOPSTART,
    SAMPLE_EDITOR_LOOPEND,
    SAMPLE_EDITOR_SPIN_LAST
} SampleEditorSpin;

typedef struct _SampleEditorDisplay SampleEditorDisplay;
typedef struct _SampleEditorDisplayPriv SampleEditorDisplayPriv;

struct _SampleEditorDisplay {
    SampleDisplay display; /* Must be at the first place */
    SampleEditorDisplayPriv* priv;
    STSample* sample;
};

typedef struct {
    SampleDisplayClass parent_class;
    DIGC meter_line_gc, mixerpos_gc;
} SampleEditorDisplayClass;

typedef void (*SampleUpdateNotifyFunc)(STSample*);

GType sample_editor_display_get_type(void) G_GNUC_CONST;

#define TYPE_SAMPLE_EDITOR_DISPLAY (sample_editor_display_get_type())
#define SAMPLE_EDITOR_DISPLAY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_SAMPLE_EDITOR_DISPLAY, SampleEditorDisplay))
#define IS_SAMPLE_EDITOR_DISPLAY(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, sample_editor_display_get_type()))
#define SAMPLE_EDITOR_DISPLAY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST(klass, sample_editor_display_get_type(), SampleEditorDisplayClass))
#define SAMPLE_EDITOR_DISPLAY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), sample_editor_display_get_type(), SampleEditorDisplayClass))

GtkWidget* sample_editor_display_new(GtkWidget* scroll,
    const gboolean edit,
    SampleDisplayMode mode,
    void (*update_status_func)(SampleEditorDisplay*,
        const gboolean,
        const guint32,
        gpointer),
    gpointer status_data);
void sample_editor_display_set_sample(SampleEditorDisplay* s,
    STSample* sample);
gint sample_editor_display_print_status(SampleEditorDisplay* s,
    const gboolean cursor_in,
    const guint32 pos,
    gchar* buf,
    const gsize bufsize);

void sample_editor_display_zoom_in(SampleEditorDisplay* s);
void sample_editor_display_zoom_out(SampleEditorDisplay* s);
void sample_editor_display_zoom_to_selection(SampleEditorDisplay* s);
void sample_editor_display_select_none(SampleEditorDisplay* s);
void sample_editor_display_select_all(SampleEditorDisplay* s);

#define SAMPLE_GET_RATE(sample) 8363.0 * exp((sample->relnote * 128 + sample->finetune) * \
    2.0 * M_LN2 / (float)PITCH_OCTAVE)

void sample_editor_page_create(GtkNotebook* nb);

gboolean sample_editor_handle_keys(int shift,
    int ctrl,
    int alt,
    guint32 keyval,
    gboolean pressed);

void sample_editor_set_sample(STSample*);
void sample_editor_update_full(const gboolean full);
#define sample_editor_update() sample_editor_update_full(TRUE)
STSample* sample_editor_get_sample(void);

void sample_editor_stop_sampling(void);
void sample_editor_clear_buffers(STSampleChain* bufs);
void sample_editor_chain_to_sample(STSampleChain *bufs,
    gsize rlen,
    STMixerFormat fmt,
    guint32 srate,
    const gchar* samplename,
    const gchar* action,
    const gchar* log_name);

void sample_editor_start_updating(void);
void sample_editor_stop_updating(void);

void sample_editor_copy_cut_common(gboolean copy, gboolean spliceout);
void sample_editor_paste_clicked(void);
void sample_editor_xcopy_samples(STSample* src_smp,
    STSample* dest_smp,
    const gboolean xchg);

gboolean sample_editor_check_and_log_sample(STSample* sample,
    const gchar* title,
    const gint flags,
    const gsize data_length);

extern st_driver* sampling_driver;
extern void* sampling_driver_object;

void sample_editor_set_mode(SampleEditorDisplayInstance, SampleDisplayMode);
GtkAdjustment* sample_editor_get_adjustment(SampleEditorSpin);
void sample_editor_update_status(void);
GtkWidget* sample_editor_get_widget(const gchar* name);
void sample_editor_get_selection(gint* start, gint* end);
void sample_editor_set_selection(const gint start, const gint end);
void sample_editor_notify_update(SampleUpdateNotifyFunc func);
void sample_editor_log_action(void (*action)(void));

gboolean
sample_editor_sampled(void* src,
    guint32 count,
    gint mixfreq,
    gint mixformat);

#endif /* _SAMPLE_EDITOR_H */
