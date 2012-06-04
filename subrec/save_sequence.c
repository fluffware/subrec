#include "save_sequence.h"
#include <time_string.h>

#define SAMPLE_RATE 48000

GQuark
save_sequence_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("save-sequence-error-quark");
  return error_quark;
}

G_DEFINE_TYPE (SaveSequence, save_sequence, G_TYPE_OBJECT)
enum {
  PROP_0 = 0,
  PROP_WORK_DIR,
  PROP_SUBTITLE_STORE,
  PROP_LAST
};

enum {
  RUN_ERROR,
  DONE,
  LAST_SIGNAL
};

static guint save_sequence_signals[LAST_SIGNAL] = {0 };

static void
save_sequence_run_error(SaveSequence *sseq, GError *error,
			gpointer user_data)
{
}

static void
save_sequence_done(SaveSequence *sseq, gpointer user_data)
{
}

static void
save_sequence_finalize(GObject *object)
{
  SaveSequence *sseq = SAVE_SEQUENCE(object);
  if (sseq->pipeline) {
    gst_element_set_state(sseq->silence_src, GST_STATE_NULL);
    gst_element_set_state(sseq->file_src_bin, GST_STATE_NULL);
    gst_element_set_state(sseq->pipeline, GST_STATE_NULL);
  }
  if (sseq->blocked_seek) {
    blocked_seek_destroy(sseq->blocked_seek);
    sseq->blocked_seek = NULL;
  }
  g_clear_object(&sseq->pipeline);
  g_clear_object(&sseq->working_directory);
  g_clear_object(&sseq->subtitle_store);
  G_OBJECT_CLASS (save_sequence_parent_class)->finalize (object);
}

static void
save_sequence_get_property(GObject    *object,
				  guint       property_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  SaveSequence *sseq = SAVE_SEQUENCE(object);
  switch(property_id) {
  case PROP_WORK_DIR:
    g_value_set_object (value, sseq->working_directory);
    break;
  case PROP_SUBTITLE_STORE:
    g_value_set_object (value, sseq->subtitle_store);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
save_sequence_set_property(GObject * object, guint property_id,
			    const GValue * value, GParamSpec * pspec)
{
  SaveSequence *sseq = SAVE_SEQUENCE(object);
  switch(property_id) {
  case PROP_WORK_DIR:
    sseq->working_directory = g_value_get_object (value);
    g_object_ref(sseq->working_directory);
    break;
  case PROP_SUBTITLE_STORE:
    sseq->subtitle_store = g_value_get_object (value);
    g_object_ref(sseq->subtitle_store);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
save_sequence_class_init(SaveSequenceClass *g_class)
{
  GParamSpec *pspec;
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  gobject_class->finalize = save_sequence_finalize;
  gobject_class->get_property = save_sequence_get_property;
  gobject_class->set_property = save_sequence_set_property;
  
  g_class->run_error = save_sequence_run_error;
  g_class->done = save_sequence_done;
  
  save_sequence_signals[RUN_ERROR] =
    g_signal_new("run-error",
		 G_OBJECT_CLASS_TYPE (g_class), G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET(SaveSequenceClass, run_error),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__POINTER,
		 G_TYPE_NONE, 1, G_TYPE_POINTER);
  save_sequence_signals[DONE] =
    g_signal_new("done",
		 G_OBJECT_CLASS_TYPE (g_class), G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET(SaveSequenceClass, done),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
  
  pspec = g_param_spec_object ("work-dir",
                               "wd",
                               "Directory where audio files are found",
			       G_TYPE_FILE,
                               G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_WORK_DIR, pspec);
  
  pspec = g_param_spec_object ("subtitle-store",
			       "subtitle-store",
                               "Sequence to save",
			       SUBTITLE_STORE_TYPE,
                               G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_SUBTITLE_STORE, pspec);


}

static gboolean
next_or_up(SaveSequence *sseq)
{
  GtkTreeIter iter = sseq->next_pos;
  while (!gtk_tree_model_iter_next(GTK_TREE_MODEL(sseq->subtitle_store),
				    &sseq->next_pos)) {
    if (sseq->depth == 0) return FALSE;
    if (!gtk_tree_model_iter_parent(GTK_TREE_MODEL(sseq->subtitle_store),
				    &sseq->next_pos, &iter)) {
      return FALSE;
    }
    sseq->depth--;
    iter = sseq->next_pos;
  }
  return TRUE;
}

static gboolean
find_valid_subtitle(SaveSequence *sseq)
{
  GtkTreeIter child;
  while(TRUE) {
    if (gtk_tree_model_iter_children (GTK_TREE_MODEL(sseq->subtitle_store),
				      &child, &sseq->next_pos)) {
      sseq->next_pos = child;
      sseq->depth++;
    } else if (subtitle_store_get_filename(sseq->subtitle_store,
					   &sseq->next_pos)) {
      return TRUE;
    } else {
      if (!next_or_up(sseq)) return FALSE;
    }
  }
}

static inline guint64
ns_to_sample(GstClockTime ns)
{
  return gst_util_uint64_scale_int_round(ns,SAMPLE_RATE, GST_SECOND); 
}

static gboolean
next_file(SaveSequence *sseq, GError **err)
{
  GstSeekFlags seek_flags;
  GstState state;
  GstState pending;
  GstPad *input_src_pad;
  guint64 duration;
      
  if (sseq->active_src) {
    gst_element_set_state(sseq->active_src, GST_STATE_PAUSED);
    gst_element_get_state(sseq->active_src, &state, &pending, GST_CLOCK_TIME_NONE);
    gst_element_unlink(sseq->active_src, sseq->output_element);
    gst_element_set_state(sseq->active_src, GST_STATE_READY);
    gst_element_get_state(sseq->active_src, &state, &pending, GST_CLOCK_TIME_NONE);
  }

  if (sseq->last_pos) {
    if (sseq->next_sample > sseq->end_sample) {
      g_set_error(err, SAVE_SEQUENCE_ERROR,
		  SAVE_SEQUENCE_ERROR_INVALID_SEQUENCE,
		  "End position less than current position");
      return FALSE;
    }
    sseq->active_src = sseq->silence_src;
    duration = sseq->end_sample- sseq->next_sample;
  } else {
    GstClockTime in_ns;
    GstClockTimeDiff duration_ns;
    guint64 in;
    gtk_tree_model_get(GTK_TREE_MODEL(sseq->subtitle_store), &sseq->next_pos,
		       SUBTITLE_STORE_COLUMN_GLOBAL_IN, &in_ns,
		       SUBTITLE_STORE_COLUMN_FILE_DURATION, &duration_ns,
		       -1);
    in = ns_to_sample(in_ns);
    duration = ns_to_sample(duration_ns);
    
    if (in > sseq->next_sample) {
      sseq->active_src = sseq->silence_src;
      duration = in - sseq->next_sample;
    } else if (in == sseq->next_sample) {
      const gchar *filename;
      GFile *file;
      sseq->active_src = sseq->file_src_bin;
      filename = subtitle_store_get_filename(sseq->subtitle_store,
					     &sseq->next_pos);
      
      file = g_file_get_child(sseq->working_directory, filename);
      
      
      g_object_set(sseq->file_src, "file", file, NULL);
      g_object_unref(file);
      sseq->last_pos = !(next_or_up(sseq) && find_valid_subtitle(sseq));
    } else {
      g_set_error(err, SAVE_SEQUENCE_ERROR,
		  SAVE_SEQUENCE_ERROR_INVALID_SEQUENCE,
		  "Next in position less than current position");
      return FALSE;
    }
  }

  g_debug("Duration: %lld", duration);
  input_src_pad = gst_element_get_static_pad(sseq->active_src, "src");
  seek_flags = GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH;
  
  /* Use segmented seek unless it's the last file in the sequence */
  if (sseq->next_sample + duration != sseq->end_sample) {
    seek_flags |= GST_SEEK_FLAG_SEGMENT;
  }

  
  blocked_seek_start(sseq->blocked_seek, sseq->active_src, input_src_pad,
		     GST_FORMAT_DEFAULT,
		     seek_flags,
		     GST_SEEK_TYPE_SET, 0,
		     GST_SEEK_TYPE_SET, duration);
  g_object_unref(input_src_pad);
  sseq->next_sample += duration;
  gst_element_link(sseq->active_src, sseq->output_element);


		     
  gst_element_set_state(sseq->active_src, GST_STATE_PLAYING);
  
  
  return TRUE;
}

static void
stop_pipeline(SaveSequence *sseq)
{
  if (sseq->pipeline) {
    gst_element_set_state(sseq->silence_src, GST_STATE_NULL);
    gst_element_set_state(sseq->file_src_bin, GST_STATE_NULL);
    gst_element_set_state(sseq->pipeline, GST_STATE_NULL);
  }
  sseq->active_src = NULL;
}

static void
set_ghost_target(GstElement *element, GstPad *new_pad, gpointer user_data)
{
  GstPad *ghost_pad = user_data;
  if (!gst_ghost_pad_set_target (GST_GHOST_PAD(ghost_pad), new_pad)) {
    g_warning("Failed to set target for ghost pad");
  }
}

static gboolean
bus_call (GstBus     *bus,
	  GstMessage *msg,
	  gpointer    data)
{
  SaveSequence *sseq = data;
  g_debug("bus_call: %s", GST_MESSAGE_TYPE_NAME(msg));
  switch (GST_MESSAGE_TYPE (msg)) {
  case GST_MESSAGE_EOS:
    g_debug ("End-of-stream");
    stop_pipeline(sseq);
    g_signal_emit(sseq, save_sequence_signals[DONE], 0);
    return TRUE;
  case GST_MESSAGE_ERROR: {
    GError *err = NULL;
    gchar *debug = NULL;
 
    gst_message_parse_error (msg, &err, &debug);
    g_signal_emit(sseq, save_sequence_signals[RUN_ERROR], 0, err);
    g_error_free (err);
    
    if (debug) {
      g_debug ("Debug details: %s\n", debug);
      g_free (debug);
    }
    stop_pipeline(sseq);
    return TRUE;
  }
  case GST_MESSAGE_SEGMENT_DONE:
    {
      GstFormat format;
      gint64 pos;
      gst_message_parse_segment_done(msg, &format, &pos);
      g_debug("Segment done %lld (%lld samples)", pos, ns_to_sample(pos));
      {
	GError *err = NULL;
	if (!next_file(sseq, &err)) {
	  g_warning("Sequence failed: %s", err->message);
	  g_error_free(err);
	}
      }
    }
    break;
  case GST_MESSAGE_STATE_CHANGED:
    {
      GstState new_state;
      gst_message_parse_state_changed(msg, NULL, &new_state, NULL);
      g_debug("New state %s for %s", gst_element_state_get_name (new_state), GST_MESSAGE_SRC_NAME(msg));
    }
    break;
  case GST_MESSAGE_SEGMENT_START:
    {
      GstFormat format = GST_FORMAT_TIME;
      gint64 pos;
      gst_message_parse_segment_start(msg, &format, &pos);
    }
    break;
  case GST_MESSAGE_STREAM_STATUS:
    {
      GstStreamStatusType type;
      GstElement *owner;
      gst_message_parse_stream_status (msg, &type, &owner);
      g_debug("Status %d from %s", type , GST_ELEMENT_NAME(owner));
    }
    break;
  default:
    break;
  }


  return TRUE;
}

static gboolean
sample_counter_cb(GstPad *pad, GstBuffer *buffer, SaveSequence *sseq)
{
  sseq->current_sample +=
    GST_BUFFER_OFFSET_END(buffer) - GST_BUFFER_OFFSET(buffer);
  return TRUE;
}

static gboolean
create_pipeline(SaveSequence *sseq, GError **err)
{
  GstBus *bus;
  GstElement *wavparse;
  GstElement *wavenc;
  GstElement *filesink;
  GstPad *ghost_pad;
  GstCaps *caps;
  GstElement *caps_filter;
  
  sseq->pipeline = gst_pipeline_new ("pipeline");
  bus = gst_pipeline_get_bus (GST_PIPELINE (sseq->pipeline));
  gst_bus_add_watch (bus, bus_call, sseq);
  gst_object_unref (bus);

  
  sseq->file_src_bin = gst_bin_new("filesrc_bin");
  if (!sseq->file_src_bin) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create file source bin");
    return FALSE;
  }
  ghost_pad = gst_ghost_pad_new_no_target ("src", GST_PAD_SRC);
  gst_element_add_pad(sseq->file_src_bin, ghost_pad);
  
  sseq->file_src = gst_element_factory_make ("giosrc", "filesrc");
  if (!sseq->file_src) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		  SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create file source");
    return FALSE;
  }
  gst_bin_add(GST_BIN(sseq->file_src_bin), sseq->file_src);
  
  wavparse = gst_element_factory_make ("wavparse", "wavdec");
  if (!wavparse) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		  SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create WAV decoder");
    return FALSE;
  }
  gst_bin_add(GST_BIN(sseq->file_src_bin), wavparse);

  g_signal_connect(wavparse, "pad-added", G_CALLBACK(set_ghost_target), ghost_pad);

  gst_element_set_locked_state(sseq->file_src_bin, TRUE);
  gst_bin_add(GST_BIN(sseq->pipeline), sseq->file_src_bin);
 
  
  sseq->silence_src = gst_element_factory_make ("audiotestsrc", "silence");
  if (!sseq->silence_src) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create silence");
    return FALSE;
  }

  g_object_set(sseq->silence_src, "wave", 4, NULL);
  gst_element_set_locked_state(sseq->silence_src, TRUE);
  gst_bin_add(GST_BIN(sseq->pipeline), sseq->silence_src);
  

  
  caps_filter = gst_element_factory_make ("capsfilter", "silence_capsfilter");
  if (!caps_filter) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create caps filter");
    return FALSE;
  }
  
  caps = gst_caps_new_simple("audio/x-raw-int",
			     "rate", G_TYPE_INT, SAMPLE_RATE,
			     "depth", G_TYPE_INT, 16,
			     "channels", G_TYPE_INT, 1,
			     NULL);
  g_object_set(caps_filter, "caps", caps, NULL);
  gst_caps_unref(caps);
  gst_bin_add(GST_BIN(sseq->pipeline), caps_filter);

  {
    GstPad *pad;
    pad = gst_element_get_static_pad(caps_filter, "sink");
    gst_pad_add_buffer_probe_full (pad, G_CALLBACK(sample_counter_cb), sseq,
				   NULL);
    gst_object_unref(pad);
  }
  wavenc = gst_element_factory_make ("wavenc", "wavenc");
  if (!wavenc) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create WAV encoder");
    return FALSE;
  }
  gst_bin_add(GST_BIN(sseq->pipeline), wavenc);
  
  sseq->output_element = caps_filter;
  
  filesink = gst_element_factory_make ("giosink", "filesink");
  if (!filesink) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		  SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create file sink");
    return FALSE;
  }
  
  gst_bin_add(GST_BIN(sseq->pipeline), filesink);

  if (!gst_element_link_many(sseq->file_src, wavparse, NULL)) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		  SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to link source bin");
    return FALSE;
  }
  
  if (!gst_element_link_many(caps_filter, wavenc, filesink, NULL)) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		  SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to link pipeline (last part)");
    return FALSE;
  }
  
  return TRUE;
}

static void
save_sequence_init(SaveSequence *instance)
{
  instance->working_directory = NULL;
  instance->subtitle_store = NULL;
  instance->pipeline = NULL;

  instance->active_src = NULL;
  
}

SaveSequence *
save_sequence_new(GError **error)
{
  SaveSequence *sseq = g_object_new(SAVE_SEQUENCE_TYPE, NULL);
  return sseq;
}

gboolean
save_sequence(SaveSequence *sseq, GFile *save_file, 
	      SubtitleStore *subtitles, GFile *working_directory,
	      GstClockTime start, GstClockTime end,
	      GError **err)
{
  GstElement *file_sink;
  sseq->depth = 0;
  stop_pipeline(sseq);
  if (!sseq->pipeline) {
    if (!create_pipeline(sseq, err)) {
      return FALSE;
    }
  }
  if (!sseq->blocked_seek) {
    sseq->blocked_seek = blocked_seek_new();
  }
  
  sseq->subtitle_store = subtitles;
  g_object_ref(subtitles);
  sseq->working_directory = working_directory;
  g_object_ref(working_directory);
  
  file_sink = gst_bin_get_by_name(GST_BIN(sseq->pipeline), "filesink");
  g_assert(file_sink);
  g_object_set(file_sink, "file", save_file, NULL);
  g_object_unref(file_sink);

  sseq->next_sample = 0;
  sseq->end_sample = ns_to_sample(end);
  sseq->start_sample = ns_to_sample(start);
  sseq->current_sample = sseq->start_sample;
  sseq->last_pos = FALSE;
  if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(sseq->subtitle_store),
				    &sseq->next_pos)) {
    if (find_valid_subtitle(sseq)) {
      if (!next_file(sseq, err)) {
	return FALSE;
      }
      gst_element_set_state(sseq->pipeline, GST_STATE_PLAYING);
    }
  }
  return TRUE;
}

gdouble
save_sequence_progress(SaveSequence *sseq)
{
  if (sseq->pipeline) {
    return ((gdouble)(sseq->current_sample - sseq->start_sample)
	    / (gdouble)(sseq->end_sample - sseq->start_sample));
  }
  return 0.0;
}
