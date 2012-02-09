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
 * SECTION:audio-trim
 *
 * Trim silence from begining and end of clip
 *
 * <refsect2>
 * 
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "audiotrim.h"
#include <math.h>

GST_DEBUG_CATEGORY_STATIC (audio_trim_debug);
#define GST_CAT_DEFAULT audio_trim_debug

enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_THRESHOLD 10
#define DEFAULT_START_SKIP 0
#define DEFAULT_END_SKIP 0
#define DEFAULT_PRE_SILENCE (GST_SECOND / 10)
#define DEFAULT_POST_SILENCE (GST_SECOND / 10)
#define DEFAULT_MAX_SILENCE (GST_SECOND * 5)

enum
{
  PROP_0 = 0,
  PROP_THRESHOLD,
  PROP_START_THRESHOLD,
  PROP_END_THRESHOLD,
  PROP_START_SKIP,
  PROP_END_SKIP,
  PROP_PRE_SILENCE,
  PROP_POST_SILENCE,
  PROP_MAX_SILENCE,
  PROP_SOUND_DURATION
};

#define AUDIO_PAD_CAPS "audio/x-raw-float,"	\
"rate=(int)[1,96000],"				\
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
  GST_DEBUG_CATEGORY_INIT (audio_trim_debug, "plugin", 0, "Audio trim plugin");

GST_BOILERPLATE_FULL (AudioTrim, audio_trim, GstElement,
		      GST_TYPE_ELEMENT, DEBUG_INIT)

/* Forward declarations */
static void audio_trim_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void audio_trim_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean audio_trim_set_sink_caps (GstPad * pad, GstCaps * caps);
static gboolean audio_trim_set_src_caps (GstPad * pad, GstCaps * caps);
static GstFlowReturn audio_trim_chain (GstPad * pad, GstBuffer * buf);
static gboolean audio_trim_event (GstPad * pad, GstEvent *event);
static GstStateChangeReturn
audio_trim_change_state (GstElement *element, GstStateChange transition);


static void
buffer_unref(gpointer buf)
{
  if (buf) {
    gst_buffer_unref(buf);
  }
}

static void
release_buffers(AudioTrim *filter)
{
  g_list_free_full(filter->buffers, buffer_unref);
  filter->buffers = NULL;
  filter->buffered = 0;
}

/* GObject vmethod implementations */
static void
audio_trim_finalize (GObject *obj)
{
  AudioTrim *filter = AUDIO_TRIM (obj);
  release_buffers(filter);
}

static void
audio_trim_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple(element_class,
    "Audio trimmer",
    "Trim audio clip",
    "Trim silence from begining and end of audio clip",
    "Simon Berg ksb@users.sourceforge.net");

  gst_element_class_add_pad_template(element_class,
				     gst_static_pad_template_get(&sink_factory));
  gst_element_class_add_pad_template(element_class,
				     gst_static_pad_template_get(&src_factory));
}

static void
audio_trim_class_init (AudioTrimClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GParamSpec *pspec;
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = audio_trim_set_property;
  gobject_class->get_property = audio_trim_get_property;
  gobject_class->finalize = audio_trim_finalize;
  gstelement_class->change_state = audio_trim_change_state;

  /* threshold */
  pspec =  g_param_spec_uint ("threshold",
			      "Silence threshold",
			      "Levels below this threshold is considered "
			      "silence. Given in percent of max level.",
			      0,100, DEFAULT_THRESHOLD,
			      G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_THRESHOLD, pspec);
  
  /* start-threshold */
  pspec =  g_param_spec_uint ("start-threshold",
			      "Start silence threshold",
			      "Levels below this threshold is considered "
			      "silence at start of clip. "
			      "Given in percent of max level.",
			      0,100, DEFAULT_THRESHOLD,
			      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_START_THRESHOLD, pspec);
  
  /* end-threshold */
  pspec =  g_param_spec_uint ("end-threshold",
			      "End silence threshold",
			      "Levels below this threshold is considered "
			      "silence at end of clip. "
			      "Given in percent of max level.",
			      0,100, DEFAULT_THRESHOLD,
			      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_START_THRESHOLD, pspec);

  
  
  /* start-skip */
  pspec = g_param_spec_uint64 ("start-skip",
			       "Start skip",
			       "Ignore this number of ns at start of clip.",
			       0,G_MAXINT64, DEFAULT_START_SKIP,
			       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_START_SKIP, pspec);

  /* end-skip */
  pspec = g_param_spec_uint64 ("end-skip",
			       "End skip",
			       "Ignore this many ns at end of clip.",
			       0,G_MAXINT64, DEFAULT_END_SKIP,
			       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_END_SKIP, pspec);

  /* pre-silence */
  pspec = g_param_spec_uint64 ("pre-silence",
			       "Time before silence",
			       "Start clip this many ns before sound.",
			       0,G_MAXINT64, DEFAULT_PRE_SILENCE,
			       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_PRE_SILENCE, pspec);

  /* post-silence */
  pspec =g_param_spec_uint64 ("post-silence",
			      "Time after silence",
			      "Stop clip this many ns after sound.",
			      0,G_MAXINT64, DEFAULT_POST_SILENCE,
			      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(gobject_class, PROP_POST_SILENCE, pspec);

  /* max-silence */
  pspec = g_param_spec_uint64 ("max-silence",
			       "Max silence allowed",
			       "Don't allow more than this many ns inside a "
			       "clip.",
			       0,G_MAXINT64, DEFAULT_MAX_SILENCE,
			       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_MAX_SILENCE, pspec);
  
  /* sound-duration */
  pspec = g_param_spec_uint64 ("sound-duration",
			       "Duration of sound",
			       "Duration of detected sound so far when running,"
			       " or total sound duration after EOS",
			       0,G_MAXINT64, 0,
			       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_SOUND_DURATION, pspec);
}

static void
audio_trim_init (AudioTrim * filter,
    AudioTrimClass * gclass)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(audio_trim_set_sink_caps));
  gst_pad_set_getcaps_function (filter->sinkpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(audio_trim_chain));
  gst_pad_set_event_function (filter->sinkpad,
			      GST_DEBUG_FUNCPTR(audio_trim_event));
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src ");
  gst_pad_set_setcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(audio_trim_set_src_caps));
  gst_pad_set_getcaps_function (filter->srcpad,
                                GST_DEBUG_FUNCPTR(gst_pad_proxy_getcaps));
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->buffers = NULL;
  
  filter->start_threshold = DEFAULT_THRESHOLD;
  filter->end_threshold = DEFAULT_THRESHOLD;
  filter->start_skip = DEFAULT_START_SKIP;
  filter->end_skip = DEFAULT_END_SKIP;
  filter->pre_silence = DEFAULT_PRE_SILENCE;
  filter->post_silence = DEFAULT_POST_SILENCE;
  filter->max_silence_duration = DEFAULT_MAX_SILENCE;
  filter->empty_start_packet = TRUE;
  
  filter->accumulator = 0.0;
  filter->f0 = 0.999;
  filter->trim_state = AUDIO_TRIM_NOT_STARTED;

  filter->buffered = 0;
  filter->sound_duration = 0;
}

static void
audio_trim_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  AudioTrim *filter = AUDIO_TRIM (object);

  switch (prop_id) {
  case PROP_THRESHOLD:
    filter->start_threshold = g_value_get_uint (value);
    filter->end_threshold = filter->start_threshold;
    break;
  case PROP_START_THRESHOLD:
    filter->start_threshold = g_value_get_uint (value);
    break;
  case PROP_END_THRESHOLD:
    filter->end_threshold = g_value_get_uint (value);
    break;
  case PROP_START_SKIP:
    filter->start_skip = g_value_get_uint64(value);
    break;
  case PROP_END_SKIP:
    filter->end_skip = g_value_get_uint64(value);
    break;
  case PROP_PRE_SILENCE:
    filter->pre_silence = g_value_get_uint64(value);
    break;
  case PROP_POST_SILENCE:
    filter->post_silence = g_value_get_uint64(value);
    break;
  case PROP_MAX_SILENCE:
    filter->max_silence_duration = g_value_get_uint64(value);
    break;
    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
audio_trim_get_property (GObject * object, guint prop_id,
			 GValue * value, GParamSpec * pspec)
{
  AudioTrim *filter = AUDIO_TRIM (object);

  switch (prop_id) {
  case PROP_START_THRESHOLD:
    g_value_set_uint (value, filter->start_threshold);
    break;
  case PROP_END_THRESHOLD:
    g_value_set_uint (value, filter->end_threshold);
    break;
  case PROP_START_SKIP:
    g_value_set_uint64 (value, filter->start_skip);
    break;
  case PROP_END_SKIP:
    g_value_set_uint64 (value, filter->end_skip);
    break;
  case PROP_PRE_SILENCE:
    g_value_set_uint64 (value, filter->pre_silence);
    break;
  case PROP_POST_SILENCE:
    g_value_set_uint64 (value, filter->post_silence);
    break;
  case PROP_MAX_SILENCE:
    g_value_set_uint64 (value, filter->max_silence_duration);
    break;
  case PROP_SOUND_DURATION:
    g_value_set_uint64 (value, filter->sound_duration);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

/* GstElement vmethod implementations */

/* this function handles the link with other elements */
static gboolean
audio_trim_set_caps (GstPad * pad, GstCaps * caps)
{
  AudioTrim *filter;
  GstStructure *caps_struct = gst_caps_get_structure (caps, 0);
  filter = AUDIO_TRIM (GST_OBJECT_PARENT (pad));
  gst_structure_get_int(caps_struct, "rate", &filter->sample_rate);
  g_debug("Sample rate: %d", filter->sample_rate);
  return TRUE;
}

static gboolean
audio_trim_set_sink_caps (GstPad * pad, GstCaps * caps)
{
  AudioTrim *filter;
  g_debug("audio_trim_set_sink_caps");
  filter = AUDIO_TRIM (GST_OBJECT_PARENT (pad));
  if (!gst_pad_set_caps(filter->srcpad, caps)) return FALSE;
  
  return audio_trim_set_caps (pad, caps);
}

static gboolean
audio_trim_set_src_caps (GstPad * pad, GstCaps * caps)
{
  AudioTrim *filter;
  g_debug("audio_trim_set_src_caps");
  filter = AUDIO_TRIM (GST_OBJECT_PARENT (pad));
  if (!gst_pad_set_caps(filter->sinkpad, caps)) return FALSE;
  
  return audio_trim_set_caps (pad, caps);
}


static GstStateChangeReturn
audio_trim_change_state (GstElement *element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  AudioTrim *filter;
  filter = AUDIO_TRIM (element);
  switch(transition) {
  case GST_STATE_CHANGE_READY_TO_PAUSED:
    filter->accumulator = 0.0;
    filter->f0 = 0.99;
    filter->trim_state = AUDIO_TRIM_NOT_STARTED;
    filter->sound_duration = 0;
    break;
  default:
    break;
  }
  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;
  
  switch(transition) {
  case GST_STATE_CHANGE_PAUSED_TO_READY:
    release_buffers(filter);
    break;
  default:
    break;
  }
  
  return ret;
}

static guint64
find_not_silence(AudioTrim *filter, GstBuffer *buf)
{
  gfloat *data = (gfloat*) GST_BUFFER_DATA(buf);
  gfloat *end = (gfloat*) &((guint8*)data)[GST_BUFFER_SIZE(buf)];
  gfloat f = filter->f0;
  gfloat acc = filter->accumulator;
  gfloat t = filter->start_threshold / (100.0 * (1-f));
  while(data < end && acc < t) {
    acc = acc * f + fabsf(*data++);
  }
  filter->accumulator = acc;
  if (data == end) return GST_BUFFER_OFFSET_NONE;
  return GST_BUFFER_OFFSET(buf) + (end - data);
}

static guint64
find_not_silence_rev(AudioTrim *filter, GstBuffer *buf, gint64 end_offset)
{
  gfloat *start = (gfloat*) GST_BUFFER_DATA(buf);
  gfloat *data;
  gfloat f = filter->f0;
  gfloat acc = filter->accumulator;
  gfloat t = filter->start_threshold / (100.0 * (1-f));
  if (end_offset <= GST_BUFFER_OFFSET(buf)) return GST_BUFFER_OFFSET_NONE;
  if (end_offset > GST_BUFFER_OFFSET_END(buf))
    end_offset = GST_BUFFER_OFFSET_END(buf);
  data = &((gfloat*)start)[end_offset - GST_BUFFER_OFFSET(buf)];
  while(data-- > start && acc < t) {
    acc = acc * f + fabsf(*data);
  }
  data++;
  filter->accumulator = acc;
  if (data == start) return GST_BUFFER_OFFSET_NONE;
  return GST_BUFFER_OFFSET(buf) + (data - start);
}

static inline guint64
time_to_sample(AudioTrim *filter, GstClockTime time)
{
  return time * filter->sample_rate / GST_SECOND;
}

static inline guint64
sample_to_time(AudioTrim *filter, gint64 sample)
{
  return sample * GST_SECOND / filter->sample_rate;
}

static void
copy_caps(GstBuffer *from, GstBuffer *to)
{
  GstCaps *caps;
  if ((caps = GST_BUFFER_CAPS (from)))
    gst_caps_ref (caps);
  GST_BUFFER_CAPS (to) = caps;
}

/* Return a buffer containing all samples before the given sample */

static GstBuffer *
buffer_head(AudioTrim *filter, GstBuffer *buf, gint64 pos)
{
  GstBuffer *head;
  guint end;
  if (pos <= GST_BUFFER_OFFSET(buf)) return NULL;
  if (pos >= GST_BUFFER_OFFSET_END(buf)) return gst_buffer_ref(buf);
  end = (pos -  GST_BUFFER_OFFSET(buf)) * sizeof(gfloat);
  g_assert(end < GST_BUFFER_SIZE(buf));
  head = gst_buffer_create_sub(buf, 0, end);
  copy_caps(buf, head);
  GST_BUFFER_DURATION(head) = sample_to_time(filter,
					    pos -  GST_BUFFER_OFFSET(buf));
  GST_BUFFER_OFFSET_END(head) = pos;
  return head;
}

/* Return a buffer containing all samples after the given sample */

static GstBuffer *
buffer_tail(AudioTrim *filter, GstBuffer *buf, gint64 pos)
{
  GstBuffer *head;
  guint start;
  if (pos >= GST_BUFFER_OFFSET_END(buf)) return NULL;
  if (pos <= GST_BUFFER_OFFSET(buf)) return gst_buffer_ref(buf);
  start = (pos -  GST_BUFFER_OFFSET(buf)) * sizeof(gfloat);
  g_assert(start < GST_BUFFER_SIZE(buf));
  head = gst_buffer_create_sub(buf, start, GST_BUFFER_SIZE(buf) - start);
  copy_caps(buf, head);

  GST_BUFFER_TIMESTAMP(head) = sample_to_time(filter, pos);
  GST_BUFFER_OFFSET(head) = pos;
  GST_BUFFER_DURATION(head) = sample_to_time(filter,
					    GST_BUFFER_OFFSET_END(buf) - pos);
  GST_BUFFER_OFFSET_END(head) = GST_BUFFER_OFFSET_END(buf);
  return head;
}

static GstBuffer *
buffer_slice(AudioTrim *filter, GstBuffer *buf, gint64 start, gint64 end)
{
  GstBuffer *sub;
  gint byte_start;
  gint byte_size;
  g_assert(start <= end);
  /* Check if buffer is outside range */
  if (start >= GST_BUFFER_OFFSET_END(buf)
      || end <= GST_BUFFER_OFFSET(buf)
      || start == end) {
    return NULL;
  }
  /* Return original buffer if possible */
  if (start <= GST_BUFFER_OFFSET(buf) && end >= GST_BUFFER_OFFSET_END(buf)) {
    return gst_buffer_ref(buf);
  }
  if (start < GST_BUFFER_OFFSET(buf)) start = GST_BUFFER_OFFSET(buf);
  if (end > GST_BUFFER_OFFSET_END(buf)) end = GST_BUFFER_OFFSET_END(buf);
  byte_start = (start -  GST_BUFFER_OFFSET(buf)) * sizeof(gfloat);
  g_assert(byte_start < GST_BUFFER_SIZE(buf));
  byte_size = (end - start) * sizeof(gfloat);
  sub = gst_buffer_create_sub(buf, byte_start, byte_size);
  copy_caps(buf, sub);
  GST_BUFFER_TIMESTAMP(sub) = sample_to_time(filter, start);
  GST_BUFFER_OFFSET(sub) = start;
  GST_BUFFER_DURATION(sub) = sample_to_time(filter, end-start);
  GST_BUFFER_OFFSET_END(sub) = end;
  return sub;
}

static void
save_buffer(AudioTrim *filter, GstBuffer *buf)
{
  filter->buffers = g_list_append(filter->buffers, buf);
  filter->buffered += GST_BUFFER_DURATION(buf);
}

static GstFlowReturn
send_buffers_after(AudioTrim *filter, gint64 after)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GList *b = filter->buffers;
  while(b) {
    GstBuffer *buf = b->data;
    b->data = NULL;
    if ((gint64)GST_BUFFER_OFFSET_END(buf) <= after) {
      gst_buffer_unref(buf);
    } else {
      if ((gint64)GST_BUFFER_OFFSET(buf) < after) {
	GstBuffer *tail = buffer_tail(filter, buf, after);
	gst_buffer_unref(buf);
	buf = tail;
      }
      ret = gst_pad_push(filter->srcpad, buf);
      if (ret != GST_FLOW_OK) break;
    }
    b = g_list_next(b);
  }
  release_buffers(filter);
  return ret;
}

static GstFlowReturn
send_buffers_before(AudioTrim *filter, gint64 before)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GList *b = filter->buffers;
  while(b) {
    GstBuffer *buf = b->data;
    b->data = NULL;
    if ((gint64)GST_BUFFER_OFFSET(buf) >= before) {
      gst_buffer_unref(buf);
    } else {
      if ((gint64)GST_BUFFER_OFFSET_END(buf) > before) {
	GstBuffer *head = buffer_head(filter, buf, before);
	gst_buffer_unref(buf);
	buf = head;
      }
      ret = gst_pad_push(filter->srcpad, buf);
      if (ret != GST_FLOW_OK) break;
    }
    b = g_list_next(b);
  }
  release_buffers(filter);
  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
audio_trim_chain (GstPad * pad, GstBuffer * buf)
{
  AudioTrim *filter;

  g_assert(GST_BUFFER_OFFSET(buf) != GST_BUFFER_OFFSET_NONE);
  g_assert(GST_BUFFER_OFFSET_END(buf) != GST_BUFFER_OFFSET_NONE);
  
  filter = AUDIO_TRIM (GST_OBJECT_PARENT (pad));
  while(buf) {
    g_assert(GST_IS_BUFFER(buf));
    switch(filter->trim_state) {
    case AUDIO_TRIM_NOT_STARTED:
      filter->ref_time = (GST_BUFFER_OFFSET(buf)
			  + time_to_sample(filter, filter->start_skip));
      if (filter->empty_start_packet) {
	GstFlowReturn ret;
	GstBuffer *first;
	first = gst_buffer_new_and_alloc (sizeof(gfloat));
	*(gfloat*)GST_BUFFER_DATA(first) = 0.0;
	GST_BUFFER_SIZE(first) = 4;
	GST_BUFFER_OFFSET(first) = GST_BUFFER_OFFSET(buf); 
	GST_BUFFER_OFFSET_END(first) = GST_BUFFER_OFFSET(buf);
	GST_BUFFER_TIMESTAMP(first) = GST_BUFFER_TIMESTAMP(buf);
	GST_BUFFER_DURATION(first) = 0;
	GST_BUFFER_CAPS(first) = gst_caps_ref(GST_BUFFER_CAPS(buf));
	
	ret = gst_pad_push(filter->srcpad, first);
	if (ret != GST_FLOW_OK) {
	  gst_buffer_unref(buf);
	  return ret;
	}
      }
      filter->trim_state = AUDIO_TRIM_START_SKIP;
      break;
    case AUDIO_TRIM_START_SKIP:
      if (GST_BUFFER_OFFSET_END(buf) <= filter->ref_time) {
	gst_buffer_unref(buf); /* Ignore buffer completely */
      } else {
	GstBuffer *tail = buffer_tail(filter, buf, filter->ref_time);
	if (buf) gst_buffer_unref(buf);
	buf = tail;
	filter->trim_state = AUDIO_TRIM_START_SILENCE;
      }
      break;
    case AUDIO_TRIM_START_SILENCE:
      {
	guint64 offset = find_not_silence(filter, buf);
	if (offset == GST_BUFFER_OFFSET_NONE) {
	  while(filter->buffered > filter->pre_silence) {
	    GstBuffer *old = filter->buffers->data;
	    filter->buffered -= GST_BUFFER_DURATION(old);
	    gst_buffer_unref(old);
	    filter->buffers =
	      g_list_delete_link(filter->buffers, filter->buffers);
	  }
	  save_buffer(filter, buf);
	  buf = NULL;
	} else {
	  GstBuffer *head;
	  GstBuffer *tail;
	  GstFlowReturn ret;
	  gint64 clip_start;
	  clip_start = offset - time_to_sample(filter, filter->pre_silence);
	  ret = send_buffers_after(filter, clip_start);
	  if (ret != GST_FLOW_OK) {
	    gst_buffer_unref(buf);
	    return ret;
	  }
	  head = buffer_slice(filter, buf, clip_start, offset);
	  if (head) {
	    ret = gst_pad_push(filter->srcpad, head);
	    if (ret != GST_FLOW_OK) {
	      gst_buffer_unref(buf);
	      return ret;
	    }
	  }
	  tail = buffer_tail(filter, buf, offset);
	  filter->sound_duration =
	    sample_to_time(filter, GST_BUFFER_OFFSET_END(buf) - clip_start);
	  filter->ref_time = clip_start;
	  gst_buffer_unref(buf);
	  buf = tail;
	  filter->trim_state = AUDIO_TRIM_NOT_SILENCE;
	  g_debug("Got sound");
	}
      }
      break;
    case AUDIO_TRIM_NOT_SILENCE:
      {
	GstFlowReturn ret;
	filter->sound_duration += GST_BUFFER_DURATION(buf);
	while(filter->buffered > filter->max_silence_duration) {
	  GstBuffer *old = filter->buffers->data;
	  filter->buffered -= GST_BUFFER_DURATION(old);
	  filter->buffers = g_list_delete_link(filter->buffers,filter->buffers);
	  ret = gst_pad_push(filter->srcpad, old);
	   if (ret != GST_FLOW_OK) {
	     gst_buffer_unref(buf);
	     return ret;
	   }
	}
	save_buffer(filter, buf);
	buf = 0;
      }
      break;
    default:
      gst_buffer_unref(buf);
      buf = NULL;
    }
  }

  return GST_FLOW_OK;
}

static gboolean
audio_trim_event (GstPad * pad, GstEvent *event)
{
  if (GST_EVENT_TYPE(event) == GST_EVENT_EOS) {
    GList *b;
    gint64 last_offset;
    AudioTrim *filter;
    g_debug("Got EOS");
    filter = AUDIO_TRIM (GST_OBJECT_PARENT (pad));
    b = g_list_last(filter->buffers);
    if (b) {
      filter->accumulator = 0.0;
      last_offset = GST_BUFFER_OFFSET_END((GstBuffer*)b->data);
      last_offset -= time_to_sample(filter, filter->end_skip);
      while(b) {
	gint64 pos = find_not_silence_rev(filter, b->data, last_offset);
	if (pos != GST_BUFFER_OFFSET_NONE) {
	  pos += time_to_sample(filter, filter->post_silence);
	  send_buffers_before(filter,pos);
	  filter->sound_duration=sample_to_time(filter, pos - filter->ref_time);
	  break;
	}
	b = g_list_previous(b);
      }
    }
  }
  return gst_pad_event_default (pad, event);
}

static gboolean
audio_trim_plugin_init (GstPlugin *plugin)
{
  g_debug("audio_trim_plugin_init");
  GST_DEBUG_CATEGORY_INIT (audio_trim_debug, "audiotrim",
      0, "Audio trim");

  return gst_element_register (plugin, "audiotrim", GST_RANK_NONE,
			       GST_TYPE_AUDIO_TRIM);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */

#ifndef PACKAGE
#define PACKAGE "audiotrim"
#endif

/* gstreamer looks for this structure to register audiotrim
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "audiotrim",
    "Trim silence from beginning and end of clip",
    audio_trim_plugin_init,
    VERSION,
    "LGPL",
    "Websync",
    "http://gstreamer.net/"
)
