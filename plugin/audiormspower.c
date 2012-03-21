/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2012 Simon Berg <ksb@users.sourceforge.net>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:audio-rms-power
 *
 * Measure power in audio streams
 *
 * <refsect2>
 * 
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "audiormspower.h"
#include <math.h>
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (audio_rms_power_debug);
#define GST_CAT_DEFAULT audio_rms_power_debug

static GQuark sub_block_message_quark = 0;
static GQuark sub_block_power_quark = 0;

static GQuark analysis_message_quark = 0;
static GQuark analysis_loudness_quark = 0;
static GQuark analysis_trim_start_quark = 0;
static GQuark analysis_trim_end_quark = 0;



enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_SUB_BLOCK_LENGTH (100*GST_MSECOND)
#define DEFAULT_BLOCK_LENGTH 4 /* 400 ms */
#define DEFAULT_BLOCK_OVERLAP 3 /* 75% */
#define DEFAULT_SUB_BLOCK_MESSAGE FALSE
#define DEFAULT_ANALYSIS_MESSAGE FALSE
#define DEFAULT_TRIM_LEVEL 0.1
#define DEFAULT_REGENERATE_TIMESTAMPS TRUE

enum
{
  PROP_0 = 0,
  PROP_SUB_BLOCK_LENGTH,
  PROP_BLOCK_LENGTH, /* In number of sub blocks */
  PROP_BLOCK_OVERLAP, /* In number of sub blocks */
  PROP_SUB_BLOCK_MESSAGE, /* Post a power level message for each sub block */
  PROP_ANALYSIS_MESSAGE, /* Post a analysis message at EOF */
  PROP_TRIM_LEVEL, /* Power level used for trimming */
  PROP_POWER_BUFFERS /* A GList of GstBuffer containing power values for
			the last analysis */
};

#define AUDIO_PAD_CAPS "audio/x-raw-float,"	\
"rate=(int)48000,"				\
"channels= (int) 1,"				\
"endianness= (int) BYTE_ORDER,"			\
"signed=(boolean)TRUE,"				\
"width=(int)32,"				\
  "depth=(int)32"

static GstStaticPadTemplate sink_factory =
  GST_STATIC_PAD_TEMPLATE ("sink",
			   GST_PAD_SINK,
			   GST_PAD_ALWAYS,
			   GST_STATIC_CAPS(AUDIO_PAD_CAPS) );

static GstStaticPadTemplate src_factory =
  GST_STATIC_PAD_TEMPLATE ("src",
			   GST_PAD_SRC,
			   GST_PAD_ALWAYS,
			   GST_STATIC_CAPS(AUDIO_PAD_CAPS) );


#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (audio_rms_power_debug, "plugin", 0, "Audio RMS power plugin");

GST_BOILERPLATE_FULL (AudioRmsPower, audio_rms_power, GstBaseTransform,
		      GST_TYPE_BASE_TRANSFORM, DEBUG_INIT)
static void
release_power_buffers(AudioRmsPower *filter)
{
  GList *b = filter->power_buffers;
  while(b) {
    gst_buffer_unref(b->data);
    b = b->next;
  }
  g_list_free(filter->power_buffers);
  filter->power_buffers = NULL;
  if (filter->current_power_buffer) {
    gst_buffer_unref(filter->current_power_buffer);
    filter->current_power_buffer = NULL;
  }
}

/* GObject vmethod implementations */
static void
audio_rms_power_finalize (GObject *obj)
{
  AudioRmsPower *filter = AUDIO_RMS_POWER (obj);
  release_power_buffers(filter);
}

static void
audio_rms_power_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
				       "RMS power measurement",
				       "RMS power measurement",
				       "Calculate RMS power for an audio stream",
				       "Simon Berg ksb@users.sourceforge.net");
  
  gst_element_class_add_pad_template(element_class,
				     gst_static_pad_template_get(&sink_factory));
  gst_element_class_add_pad_template(element_class,
				     gst_static_pad_template_get(&src_factory));
}

static GstFlowReturn
audio_rms_power_analyze(GstBaseTransform *trans, GstBuffer *buf);

static void
audio_rms_power_set_property (GObject * object, guint prop_id,
			      const GValue * value, GParamSpec * pspec);

static void
audio_rms_power_get_property (GObject * object, guint prop_id,
			      GValue * value, GParamSpec * pspec);

static gboolean
audio_rms_power_set_caps (GstBaseTransform *trans,
			  GstCaps *incaps, GstCaps *outcaps);
static gboolean
audio_rms_power_start (GstBaseTransform *trans);

static gboolean
audio_rms_power_stop (GstBaseTransform *trans);

static gboolean
audio_rms_power_event (GstBaseTransform *trans, GstEvent *event);

static void
audio_rms_power_class_init (AudioRmsPowerClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseTransformClass *transform_class;
  GParamSpec *pspec;
  gobject_class = (GObjectClass *) klass;
  transform_class = (GstBaseTransformClass *) klass;
  
  gobject_class->set_property = audio_rms_power_set_property;
  gobject_class->get_property = audio_rms_power_get_property;
  gobject_class->finalize = audio_rms_power_finalize;
  transform_class->passthrough_on_same_caps = TRUE;
  transform_class->transform_ip = audio_rms_power_analyze;
  transform_class->set_caps = audio_rms_power_set_caps;
  transform_class->start = audio_rms_power_start;
  transform_class->stop = audio_rms_power_stop;
  transform_class->event = audio_rms_power_event;

  if (sub_block_power_quark == 0) {
    sub_block_message_quark =
      g_quark_from_static_string(AUDIO_RMS_POWER_SUB_BLOCK_MESSAGE);
    sub_block_power_quark =
      g_quark_from_static_string(AUDIO_RMS_POWER_SUB_BLOCK_MESSAGE_POWER);
    
    analysis_message_quark =
      g_quark_from_static_string(AUDIO_RMS_POWER_ANALYSIS_MESSAGE);
    analysis_loudness_quark =
      g_quark_from_static_string(AUDIO_RMS_POWER_ANALYSIS_MESSAGE_LOUDNESS);
    analysis_trim_start_quark =
      g_quark_from_static_string(AUDIO_RMS_POWER_ANALYSIS_MESSAGE_TRIM_START);
    analysis_trim_end_quark =
      g_quark_from_static_string(AUDIO_RMS_POWER_ANALYSIS_MESSAGE_TRIM_END);
  }
  /* transform_class->transform_caps = audio_rms_power_transform_caps; */
  /* sub-block-length */
  pspec =  g_param_spec_int64 ("sub-block-length",
			      "Length of a sub block",
			      "A sub block is the smallest duration used for "
			      "RMS calculations. In nanoseconds.",
			      0,GST_SECOND, DEFAULT_SUB_BLOCK_LENGTH,
			      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_SUB_BLOCK_LENGTH, pspec);
  
  /* block-length */
  pspec =  g_param_spec_uint ("block-length",
			      "Block length for RMS calculations",
			      "Given as an integral count of sub blocks.",
			      0,100, DEFAULT_BLOCK_LENGTH,
			      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_BLOCK_LENGTH, pspec);

  /* block-overlap */
  pspec =  g_param_spec_uint ("block-overlap",
			      "Block overlap",
			      "Given as an integral count of sub blocks. "
			      "Must be less than block-length.",
			      0,100, DEFAULT_BLOCK_OVERLAP,
			      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  
  g_object_class_install_property(gobject_class, PROP_BLOCK_OVERLAP, pspec);
  
  /* sub-block-message */
  pspec =  g_param_spec_boolean ("sub-block-message",
				 "Send power messages for each sub-block",
				 "Given as a fraction (not dB).",
				 DEFAULT_SUB_BLOCK_MESSAGE,
				 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  
  g_object_class_install_property(gobject_class, PROP_SUB_BLOCK_MESSAGE, pspec);
  
  /* analysis-message */
  pspec =  g_param_spec_boolean ("analysis-message",
				 "Send analysis messages at EOS",
				 "Given as a fraction (not dB).",
				 DEFAULT_ANALYSIS_MESSAGE,
				 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  
  g_object_class_install_property(gobject_class, PROP_ANALYSIS_MESSAGE, pspec);
  
  /* trim-level */
  pspec =  g_param_spec_double ("trim-level",
				"Power level used for trimming",
				"Given as a fraction (not dB).",
				0.0, 1.0,
				DEFAULT_TRIM_LEVEL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  
  g_object_class_install_property(gobject_class, PROP_TRIM_LEVEL, pspec);
  
  /* power-buffers */
  pspec =  g_param_spec_pointer ("power-buffers",
				 "List of buffers containing power values.",
				 "Power values are given as a fraction (not dB).",
				 G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  
  g_object_class_install_property(gobject_class, PROP_POWER_BUFFERS, pspec);
  
}


static void
setup_sub_block(AudioRmsPower *filter)
{
  filter->sub_block_sample_count =
    (filter->sample_rate * filter->sub_block_length) / GST_SECOND;
}

static void
restart_analysis(AudioRmsPower * filter)
{
  /* Clear filters */
  memset(filter->prefilter_x, 0, sizeof(filter->prefilter_x));
  memset(filter->prefilter_y, 0, sizeof(filter->prefilter_y));
  release_power_buffers(filter);
  filter->sub_block_samples_left = filter->sub_block_sample_count;
  filter->square_acc = 0.0;
  filter->generated_offset = 0;
}
  
static void
audio_rms_power_init (AudioRmsPower * filter, AudioRmsPowerClass * gclass)
{
  gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(filter), TRUE);
  filter->sub_block_length = DEFAULT_SUB_BLOCK_LENGTH;
  filter->block_length = DEFAULT_BLOCK_LENGTH;
  filter->block_overlap =DEFAULT_BLOCK_OVERLAP;
  filter->sub_block_message = DEFAULT_SUB_BLOCK_MESSAGE;
  filter->analysis_message = DEFAULT_ANALYSIS_MESSAGE;
  filter->regenerate_timestamps = DEFAULT_REGENERATE_TIMESTAMPS;
  filter->generated_offset = 0;
  filter->current_power_buffer = NULL;
  filter->power_buffers = NULL;
  filter->power_buffer_max_len = 32;
  setup_sub_block(filter);
  restart_analysis(filter);
}


static void
audio_rms_power_get_property (GObject * object, guint prop_id,
			      GValue * value, GParamSpec * pspec)
{
  AudioRmsPower *filter = AUDIO_RMS_POWER (object);
  
  switch (prop_id) {
  case PROP_SUB_BLOCK_LENGTH:
    g_value_set_int64 (value, filter->sub_block_length);
    break;
  case PROP_BLOCK_LENGTH:
    g_value_set_uint (value, filter->block_length);
    break;
  case PROP_BLOCK_OVERLAP:
    g_value_set_uint (value, filter->block_overlap);
    break;
  case PROP_SUB_BLOCK_MESSAGE:
    g_value_set_boolean (value, filter->sub_block_message);
    break;
  case PROP_ANALYSIS_MESSAGE:
    g_value_set_boolean (value, filter->analysis_message);
    break;
  case PROP_TRIM_LEVEL:
    g_value_set_double (value, filter->trim_level);
    break;
  case PROP_POWER_BUFFERS:
    g_value_set_pointer(value, filter->power_buffers);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
audio_rms_power_set_property (GObject * object, guint prop_id,
			      const GValue * value, GParamSpec * pspec)
{
  AudioRmsPower *filter = AUDIO_RMS_POWER (object);

  switch (prop_id) {
  case PROP_SUB_BLOCK_LENGTH:
    filter->sub_block_length = g_value_get_int64 (value);
    setup_sub_block(filter);
    break;
  case PROP_BLOCK_LENGTH:
    filter->block_length = g_value_get_uint (value);
    break;
  case PROP_BLOCK_OVERLAP:
    filter->block_overlap = g_value_get_uint (value);
    break;
  case PROP_SUB_BLOCK_MESSAGE:
    filter->sub_block_message = g_value_get_boolean (value);
    break;
  case PROP_ANALYSIS_MESSAGE:
    filter->analysis_message = g_value_get_boolean (value);
    break;
  case PROP_TRIM_LEVEL:
    filter->trim_level = g_value_get_double (value);
    break;
    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static gboolean
audio_rms_power_set_caps (GstBaseTransform *trans,
			  GstCaps *incaps, GstCaps *outcaps)
{
  GstStructure *caps_struct;
  AudioRmsPower *filter = AUDIO_RMS_POWER (trans);
  caps_struct = gst_caps_get_structure (incaps, 0);
  gst_structure_get_int(caps_struct, "rate", &filter->sample_rate);
  setup_sub_block(filter);
  return TRUE;
}

static void
end_power_block(AudioRmsPower *filter)
{
  GstBuffer *current = filter->current_power_buffer;
  if (current) {
    GST_BUFFER_DURATION(current) =
      ((GST_BUFFER_SIZE(current) / sizeof(gfloat))
       * filter->sub_block_sample_count * GST_SECOND / filter->sample_rate);
    GST_BUFFER_OFFSET_END(current) =
      GST_BUFFER_OFFSET(current) + GST_BUFFER_SIZE(current) / sizeof(gfloat);
    
    filter->power_buffers = g_list_append(filter->power_buffers, current);
    filter->current_power_buffer = NULL;
#if 0
    g_debug("power size: %d, ts: %lld + %lld offset: %lld - %lld",
	    GST_BUFFER_SIZE(current),
	    GST_BUFFER_TIMESTAMP(current), GST_BUFFER_DURATION(current),
	    GST_BUFFER_OFFSET(current), GST_BUFFER_OFFSET_END(current));
#endif
  }
}
static void
add_power_value(AudioRmsPower *filter, gfloat power, gint64 ts)
{
  gfloat *data;
  GstBuffer *current = filter->current_power_buffer;
  gint64 offset = 0;
  if (current &&
      (GST_BUFFER_SIZE(current) ==sizeof(gfloat)*filter->power_buffer_max_len)) {
    end_power_block(filter);
    offset = GST_BUFFER_OFFSET_END(current);
    current = NULL;
  }
  if (!current) {
    current =
      gst_buffer_try_new_and_alloc(sizeof(gfloat)*filter->power_buffer_max_len);
    GST_BUFFER_SIZE(current) = 0;
    GST_BUFFER_TIMESTAMP(current) = ts;
    GST_BUFFER_OFFSET(current) = offset;
  }
  filter->current_power_buffer = current;
  data = (gfloat*)(GST_BUFFER_DATA(current) + GST_BUFFER_SIZE(current));
  *data = power;
  GST_BUFFER_SIZE(current) += sizeof(gfloat);
  
}

static gboolean
audio_rms_power_start (GstBaseTransform *trans)
{
  AudioRmsPower *filter = AUDIO_RMS_POWER (trans);
  restart_analysis(filter);
  return TRUE;
}

typedef struct 
{
  GList *list;
  const gfloat *pos;
  const gfloat *end;
} BufferListPos;

static inline void
buffer_list_init(BufferListPos *pos, GList *list)
{
  GstBuffer *buf;
  pos->list = list;
  if (list) {
    buf = list->data;
    pos->pos = (const gfloat*)GST_BUFFER_DATA(buf);
    pos->end = (const gfloat*)(GST_BUFFER_DATA(buf) + GST_BUFFER_SIZE(buf));
  }
}

static inline gboolean
buffer_list_has_next(BufferListPos *pos)
{
  return pos->list != NULL;
}

static inline gfloat
buffer_list_next_value(BufferListPos *pos)
{
  gfloat v = *pos->pos++;
  if (pos->pos == pos->end) {
    pos->list = pos->list->next;
    if (pos->list) {
      GstBuffer *buf = pos->list->data;
      pos->pos = (const gfloat*)GST_BUFFER_DATA(buf);
      pos->end = (const gfloat*)(GST_BUFFER_DATA(buf) + GST_BUFFER_SIZE(buf));
    }
  }
  return v;
}

static gfloat
calculate_gated_loudness(AudioRmsPower *filter, gfloat threshold)
{
  guint index = 0;
  BufferListPos head_pos;
  BufferListPos tail_pos;
  gfloat block_power = 0.0;
  gfloat total_power = 0.0;
  guint count = 0;
  guint step =filter->block_length - filter->block_overlap;
  buffer_list_init(&head_pos, filter->power_buffers);
  buffer_list_init(&tail_pos, filter->power_buffers);
  while(buffer_list_has_next(&head_pos) && index < filter->block_length) {
    block_power += buffer_list_next_value(&head_pos);
    index++;
  }
  if (index== 0) return 0.0;
  if (index != filter->block_length) return block_power / index;
  while(TRUE) {
    int s;
    if (block_power > threshold) {
      total_power += block_power;
      count++;
    }
    for (s = 0; buffer_list_has_next(&head_pos) && s < step; s++) { 
      block_power += buffer_list_next_value(&head_pos);
      block_power -= buffer_list_next_value(&tail_pos);
    }
    if (!buffer_list_has_next(&head_pos)) break;
  }
  if (count == 0) return 0.0;
  return total_power / (count * filter->block_length);
}

#define LKFS_SCALE 0.852903703071 /* -0.691dB */

gfloat
audio_rms_power_calculate_loudness(AudioRmsPower *filter)
{
  gfloat abs_threshold = powf(10, (-70 + 0.691) / 10);
  gfloat rel_threshold;
  rel_threshold = calculate_gated_loudness(filter, abs_threshold) * 0.1;
  
  return LKFS_SCALE * calculate_gated_loudness(filter, rel_threshold);
}

void
audio_rms_power_trim_positions(AudioRmsPower *filter,
			       GstClockTime *start_ts, GstClockTime *end_ts)
{
  GList *list = filter->power_buffers;
  if (list) {
    gint64 last;
    /* Find first block above trim level */
    gint64 first = GST_BUFFER_TIMESTAMP(list->data);
    *start_ts = first;
    while(list) {
      GstBuffer *buf = (GstBuffer*)list->data;
      gfloat *start = (gfloat*)GST_BUFFER_DATA(buf);
      gfloat *end =  (gfloat*)(GST_BUFFER_DATA(buf)+GST_BUFFER_SIZE(buf));
      gfloat *pos = start;
      while(pos < end) {
	if (*pos > filter->trim_level) {
	  *start_ts = (GST_BUFFER_TIMESTAMP(buf)
		    + filter->sub_block_length * (pos - start - 1));
	  break;
	}
	pos++;
      }
      if (pos < end) break;
      list = list->next;
    }
    if (((gint64)*start_ts) < first) *start_ts = first;

    /* Find last block above trim level */
    list = g_list_last(filter->power_buffers);
    last = GST_BUFFER_TIMESTAMP(list->data) + GST_BUFFER_DURATION(list->data);
    *end_ts = last;
    while(list) {
      GstBuffer *buf = (GstBuffer*)list->data;
      gfloat *start = (gfloat*)GST_BUFFER_DATA(buf);
      gfloat *end =  (gfloat*)(GST_BUFFER_DATA(buf)+GST_BUFFER_SIZE(buf));
      gfloat *pos = end - 1;
      while(pos >= start) {
	if (*pos > filter->trim_level) {
	  *end_ts = (GST_BUFFER_TIMESTAMP(buf)
		    + filter->sub_block_length * (pos - start + 2));
	  break;
	}
	pos--;
      }
      if (pos >= start) break;
      list = list->prev;

    }
    if (*end_ts > last) *end_ts = last;
  }
  

}

static gboolean
audio_rms_power_event (GstBaseTransform *trans, GstEvent *event)
{
  if (GST_EVENT_TYPE(event) == GST_EVENT_EOS) {
    AudioRmsPower *filter = AUDIO_RMS_POWER (trans);
    end_power_block(filter);
    if (filter->analysis_message) {
      GstStructure *power_struct;
      GstMessage *msg;
      GstClockTime start;
      GstClockTime end;
      gfloat loudness = audio_rms_power_calculate_loudness(filter);
      audio_rms_power_trim_positions(filter, &start, &end);
      power_struct = gst_structure_id_new(analysis_message_quark,
					  analysis_loudness_quark,
					  G_TYPE_DOUBLE, (gdouble)loudness,
					  analysis_trim_start_quark,
					  GST_TYPE_CLOCK_TIME, start,
					  analysis_trim_end_quark,
					  GST_TYPE_CLOCK_TIME, end,
					  NULL);
      msg = gst_message_new_element (GST_OBJECT(filter), power_struct);
      gst_bus_post(GST_ELEMENT_BUS(filter), msg);
    }
  }
  return TRUE;
}

static gboolean
audio_rms_power_stop (GstBaseTransform *trans)
{
  return TRUE;
}


#define A1 -3.680706748016390
#define A2 5.087045247971131
#define A3 -3.131546351446730
#define A4 0.725208888477870

#define B0 1.53512485958697
#define B1  -5.76194590858032
#define B2  8.11691004925258
#define B3 -5.08848181111208
#define B4 1.19839281085285

#define FILTER(x0, x1, x2, x3, x4, y1, y2, y3, y4)	\
(B0*(x0) + B1*(x1) + B2*(x2) + B3*(x3) + B4*(x4)	\
 - A1*(y1) - A2*(y2) - A3*(y3) - A4*(y4))

#define X(i) x[i-1]
#define Y(i) y[i-1]

static GstFlowReturn
audio_rms_power_analyze(GstBaseTransform *trans, GstBuffer *buf)
{
  AudioRmsPower *filter = AUDIO_RMS_POWER (trans);
  guint block_left = filter->sub_block_samples_left;
  gfloat acc = filter->square_acc;
  guint buffer_left = GST_BUFFER_SIZE(buf) / sizeof(gfloat);
  const gfloat *data = (const gfloat*)GST_BUFFER_DATA(buf);
  gfloat *x = filter->prefilter_x;
  gfloat *y = filter->prefilter_y;
  if (filter->regenerate_timestamps) {
    GST_BUFFER_OFFSET(buf) = filter->generated_offset;
    GST_BUFFER_OFFSET_END(buf) = filter->generated_offset + buffer_left;
    GST_BUFFER_TIMESTAMP(buf) =
      filter->generated_offset * GST_SECOND / filter->sample_rate;
    GST_BUFFER_DURATION(buf) = buffer_left * GST_SECOND / filter->sample_rate;
    filter->generated_offset += buffer_left;
  }
#if 0
  g_debug("size: %d, ts: %lld + %lld offset: %lld - %lld",
	  GST_BUFFER_SIZE(buf),
	  GST_BUFFER_TIMESTAMP(buf), GST_BUFFER_DURATION(buf),
	  GST_BUFFER_OFFSET(buf), GST_BUFFER_OFFSET_END(buf));
#endif
  while(buffer_left > 0) {
    const gfloat *end;
    if (block_left > buffer_left) {
      end = data + buffer_left;
      block_left -= buffer_left;
      buffer_left = 0;
    } else {
      end = data + block_left;
      buffer_left -= block_left;
      block_left = 0;
    }
    while(data != end) {
      gfloat y0 = FILTER(*data, X(1), X(2), X(3), X(4), Y(1), Y(2), Y(3), Y(4));
      Y(4) = Y(3);
      Y(3) = Y(2);
      Y(2) = Y(1);
      Y(1) = y0;
      X(4) = X(3);
      X(3) = X(2);
      X(2) = X(1);
      X(1) = *data;

      acc += y0 * y0;
      data++;
    }
    if (block_left == 0) {
      gfloat power = acc / filter->sub_block_sample_count;
      add_power_value(filter, power,
		      (GST_BUFFER_TIMESTAMP(buf) + GST_BUFFER_DURATION(buf)
		       - (buffer_left + filter->sub_block_sample_count) * GST_SECOND / filter->sample_rate));
      /* g_debug("Power: %f", 10*log10(power)); */
      if (filter->sub_block_message) {
	GstStructure *power_struct;
	GstMessage *msg;
	power_struct = gst_structure_id_new(sub_block_message_quark,
					    sub_block_power_quark,
					    G_TYPE_DOUBLE, (gdouble)power,
					    NULL);
	msg = gst_message_new_element (GST_OBJECT(filter), power_struct);
	gst_bus_post(GST_ELEMENT_BUS(filter), msg);
      }
      block_left = filter->sub_block_sample_count;
      acc = 0.0;
    }
  }
  filter->sub_block_samples_left = block_left;
  filter->square_acc = acc;
  return GST_FLOW_OK;
}
static gboolean
audio_rms_power_plugin_init (GstPlugin *plugin)
{
  /* g_debug("audio_rms_power_plugin_init"); */
  GST_DEBUG_CATEGORY_INIT (audio_rms_power_debug, "audiormspower",
      0, "RMS Power");

  return gst_element_register (plugin, "audiormspower", GST_RANK_NONE,
			       GST_TYPE_AUDIO_RMS_POWER);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */

#ifndef PACKAGE
#define PACKAGE "audiormspower"
#endif

/* gstreamer looks for this structure to register audiormspower
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "audiormspower",
    "Measure RMS power",
    audio_rms_power_plugin_init,
    VERSION,
    "LGPL",
    "subrec",
    "http://gstreamer.net/"
)
