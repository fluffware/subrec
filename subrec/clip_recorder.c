#include <clip_recorder.h>


GQuark
clip_recorder_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("clip-recorder-error-quark");
  return error_quark;
}

G_DEFINE_TYPE (ClipRecorder, clip_recorder, G_TYPE_OBJECT)

static void
clip_recorder_finalize(GObject *obj)
{
  ClipRecorder *recorder = CLIP_RECORDER(obj);
  if (recorder->record_pipeline) {
    gst_element_set_state(GST_ELEMENT(recorder->record_pipeline),
			  GST_STATE_NULL);
    g_clear_object(&recorder->record_pipeline);
  }
  if (recorder->playback_pipeline) {
    gst_element_set_state(GST_ELEMENT(recorder->playback_pipeline),
			  GST_STATE_NULL);
    g_clear_object(&recorder->playback_pipeline);
  }
}

enum {
  RUN_ERROR,
  RECORDING,
  PLAYING,
  STOPPED,
  LAST_SIGNAL
};

static guint clip_recorder_signals[LAST_SIGNAL] = {0 };

static void
clip_recorder_run_error(ClipRecorder *recorder, GError *error,
			gpointer user_data)
{
}

static void
clip_recorder_recording(ClipRecorder *recorder, gpointer user_data)
{
}

static void
clip_recorder_playing(ClipRecorder *recorder, gpointer user_data)
{
}

static void
clip_recorder_stopped(ClipRecorder *recorder, gpointer user_data)
{
}

static void
clip_recorder_class_init(ClipRecorderClass *obj_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (obj_class);
  gobject_class->finalize = clip_recorder_finalize;
  obj_class->run_error = clip_recorder_run_error;
  obj_class->recording = clip_recorder_recording;
  obj_class->playing = clip_recorder_playing;
  obj_class->stopped = clip_recorder_stopped;

  clip_recorder_signals[RUN_ERROR] =
    g_signal_new("run-error",
		 G_OBJECT_CLASS_TYPE (obj_class), G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET(ClipRecorderClass, run_error),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__POINTER,
		 G_TYPE_NONE, 1, G_TYPE_POINTER);
  clip_recorder_signals[RECORDING] =
    g_signal_new("recording",
		 G_OBJECT_CLASS_TYPE (obj_class), G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET(ClipRecorderClass, recording),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
  clip_recorder_signals[PLAYING] =
    g_signal_new("playing",
		 G_OBJECT_CLASS_TYPE (obj_class), G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET(ClipRecorderClass, playing),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
  clip_recorder_signals[STOPPED] =
    g_signal_new("stopped",
		 G_OBJECT_CLASS_TYPE (obj_class), G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET(ClipRecorderClass, stopped),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
}


static void
clip_recorder_init(ClipRecorder *recorder)
{
  recorder->record_pipeline = NULL;
  recorder->playback_pipeline = NULL;
  recorder->active_pipeline = NULL;

  recorder->last_record_length = 0;
}

static gboolean
bus_call (GstBus     *bus,
	  GstMessage *msg,
	  gpointer    data)
{
  ClipRecorder *recorder = data;
  if (!recorder->active_pipeline) return FALSE;
  /* g_debug("bus_call: %s", GST_MESSAGE_TYPE_NAME(msg)); */
  switch (GST_MESSAGE_TYPE (msg)) {
  case GST_MESSAGE_EOS:
    g_debug ("End-of-stream");
    if (recorder->active_pipeline == recorder->record_pipeline) {
      GstElement *trim =
	gst_bin_get_by_name(GST_BIN(recorder->active_pipeline), "trim");
      g_object_get(trim, "sound-duration", &recorder->last_record_length, NULL);
    }
    gst_element_set_state(GST_ELEMENT(recorder->active_pipeline),
			  GST_STATE_READY);
    break;
  case GST_MESSAGE_ERROR: {
    gchar *debug = NULL;
    GError *err = NULL;

    gst_message_parse_error (msg, &err, &debug);
    g_signal_emit(recorder, clip_recorder_signals[RUN_ERROR], 0, err);
    g_print ("Error: %s\n", err->message);
    g_error_free (err);

    if (debug) {
      g_print ("Debug details: %s\n", debug);
      g_free (debug);
    }
    gst_element_set_state(GST_ELEMENT(recorder->active_pipeline),
			  GST_STATE_READY);
    break;
  }
  case GST_MESSAGE_STATE_CHANGED:
    {
      GstState old_state;
      GstState new_state;
      gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
      if (msg->src == (GstObject*)recorder->active_pipeline) {
	switch(new_state) {
	case GST_STATE_READY:
	  if (old_state == GST_STATE_PAUSED) {
	    g_signal_emit(recorder, clip_recorder_signals[STOPPED], 0);
	    gst_element_set_state(GST_ELEMENT(recorder->active_pipeline),
				  GST_STATE_NULL);
	    recorder->active_pipeline = NULL;
	  }
	break;
	case GST_STATE_PLAYING:
	  if (msg->src == (GstObject*)recorder->record_pipeline) {
	    g_signal_emit(recorder, clip_recorder_signals[RECORDING], 0);
	  } else if (msg->src == (GstObject*)recorder->playback_pipeline) {
	    g_signal_emit(recorder, clip_recorder_signals[PLAYING], 0);
	  }
	  break;
	default:
	  break;
	}
      }
    }
    break;
  default:
    break;
  }
  
  
  return TRUE;
}

static GstPipeline *
get_record_pipeline(ClipRecorder *recorder, GError **err)
{
  GstElement *pipeline;
  GstBus *bus;
  GstElement *input;
  GstElement *convert1;
  GstElement *high_pass;
  GstElement *trim;
  GstElement *convert2;
  GstElement *wavenc;
  GstElement *filesink;
  GstCaps *output_filter;
  
  if (!recorder->record_pipeline) {
    pipeline = gst_pipeline_new ("record");
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, recorder);
    gst_object_unref (bus);

    input = gst_element_factory_make ("alsasrc", "input");
    if (!input) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create audio input (ALSA)");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), input);

    convert1 = gst_element_factory_make ("audioconvert", "convert1");
    if (!convert1) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create audio converter");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), convert1);

    high_pass = gst_element_factory_make ("audiocheblimit", "high_pass");
    if (!high_pass) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create high pass filter");
      gst_object_unref(pipeline);
      return NULL;
    }
    g_object_set(high_pass, "mode", 1,
		 "cutoff", 50.0, NULL);
    gst_bin_add(GST_BIN(pipeline), high_pass);

    trim = gst_element_factory_make ("audiotrim", "trim");
    if (!trim) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create audio trimmer");
      gst_object_unref(pipeline);
      return NULL;
    }
    g_object_set(trim, "threshold", 3,
		 "post-silence", GST_SECOND/5, NULL);
    gst_bin_add(GST_BIN(pipeline), trim);

    
    convert2 = gst_element_factory_make ("audioconvert", "convert2");
    if (!convert2) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create audio converter");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), convert2);

    wavenc = gst_element_factory_make ("wavenc", "wavenc");
    if (!wavenc) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create WAV encoder");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), wavenc);

    
    filesink = gst_element_factory_make ("giosink", "file");
    if (!filesink) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create file sink");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), filesink);

    if (!gst_element_link_many(input, convert1, high_pass, trim, convert2,
			       NULL)) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_LINK_FAILED,
		  "Failed to link record pipeline (first part)");
      gst_object_unref(pipeline);
      return NULL;
    }
    output_filter = gst_caps_new_simple("audio/x-raw-int",
					"rate", G_TYPE_INT, 48000,
					"depth", G_TYPE_INT, 16,
					"channels", G_TYPE_INT, 1,
					NULL);
    if (!gst_element_link_filtered(convert2, wavenc, output_filter)) {
      gst_caps_unref(output_filter);
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_LINK_FAILED,
		  "Failed to link record pipeline (middle part)");
      gst_object_unref(pipeline);
      return NULL;
    }
      gst_caps_unref(output_filter);
    
    if (!gst_element_link(wavenc, filesink)) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_LINK_FAILED,
		  "Failed to link record pipeline (last part)");
      gst_object_unref(pipeline);
      return NULL;
    }

    recorder->record_pipeline = GST_PIPELINE(pipeline);
  }
  return recorder->record_pipeline;
}
static void
wav_parse_pad_added (GstElement* object, GstPad* new_pad, GstElement *sink_elem)
{
  GstPad *sink_pad;
  sink_pad = gst_element_get_compatible_pad (sink_elem, new_pad, GST_CAPS_ANY);
  if (!sink_pad) {
    g_warning("No compatible pad found");
    return;
  }
  if (!gst_pad_is_linked (sink_pad)) {
    GstPadLinkReturn link_ret = gst_pad_link(new_pad, sink_pad);
    if (link_ret != GST_PAD_LINK_OK) {
      g_warning("Linking WAV parser to converter failed: %d", link_ret);
    }
  }
}

static GstPipeline *
get_playback_pipeline(ClipRecorder *recorder, GError **err)
{
  GstElement *pipeline;
  GstBus *bus;
  GstElement *filesrc;
  GstElement *wavdec;
  GstElement *convert;
  GstElement *output;
  if (!recorder->playback_pipeline) {
    pipeline = gst_pipeline_new ("playback");
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, recorder);
    gst_object_unref (bus);

    filesrc = gst_element_factory_make ("giosrc", "file");
    if (!filesrc) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to file source");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), filesrc);


    wavdec = gst_element_factory_make ("wavparse", "wavdec");
    if (!wavdec) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create WAV decoder");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), wavdec);

    
    convert = gst_element_factory_make ("audioconvert", "convert");
    if (!convert) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create audio converter");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), convert);

    g_signal_connect(wavdec, "pad-added",
		     (GCallback)wav_parse_pad_added, convert);

    output = gst_element_factory_make ("alsasink", "output");
    if (!output) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create output (ALSA)");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), output);

    if (!gst_element_link(filesrc, wavdec)) {
       g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_LINK_FAILED,
		  "Failed to link playback pipeline (first part)");
       gst_object_unref(pipeline);
       return NULL;
    }
    if (!gst_element_link(convert, output)) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_LINK_FAILED,
		  "Failed to link playback pipeline (second part)");
       gst_object_unref(pipeline);
       return NULL;
    }
    
    recorder->playback_pipeline = GST_PIPELINE(pipeline);
  }
  return recorder->playback_pipeline;
}

static void
cancel_active_pipeline(ClipRecorder *recorder)
{
  if (recorder->active_pipeline) {
    GstState state = GST_STATE(recorder->active_pipeline);
    if (state != GST_STATE_NULL) {
      gst_element_set_state(GST_ELEMENT(recorder->active_pipeline),
			    GST_STATE_NULL);
      gst_element_get_state(GST_ELEMENT(recorder->active_pipeline), &state,
			    NULL, GST_CLOCK_TIME_NONE);
    }
  }
}


ClipRecorder *
clip_recorder_new(void)
{
  ClipRecorder *recorder = g_object_new(CLIP_RECORDER_TYPE, NULL);
  return recorder;
}

gboolean
clip_recorder_record(ClipRecorder *recorder, GFile *file,GError **err)
{
  GstStateChangeReturn state_ret;
  GstPipeline *pipeline;
  GstElement *filesink;
  cancel_active_pipeline(recorder);
  pipeline = get_record_pipeline(recorder, err);
  if (!pipeline) {
    return FALSE;
  }
  filesink = gst_bin_get_by_name(GST_BIN(pipeline), "file");
  g_assert(filesink);
  g_object_set(filesink, "file", file, NULL);
  g_object_unref(filesink);
  state_ret = gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    g_set_error(err, CLIP_RECORDER_ERROR, CLIP_RECORDER_ERROR_STATE,
		"Failed to set state to PLAYING");
    return FALSE;
  }
  recorder->active_pipeline = pipeline;
  return TRUE;
}

gboolean
clip_recorder_play(ClipRecorder *recorder, GFile *file,GError **err)
{
  GstStateChangeReturn state_ret;
  GstPipeline *pipeline;
  GstElement *filesrc;
  cancel_active_pipeline(recorder);
  pipeline = get_playback_pipeline(recorder, err);
  if (!pipeline) {
    return FALSE;
  }
  filesrc = gst_bin_get_by_name(GST_BIN(pipeline), "file");
  g_assert(filesrc);
  g_object_set(filesrc, "file", file, NULL);
  g_object_unref(filesrc);
  state_ret = gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    g_set_error(err, CLIP_RECORDER_ERROR, CLIP_RECORDER_ERROR_STATE,
		"Failed to set state to PLAYING");
    return FALSE;
  }
  recorder->active_pipeline = pipeline;
  return TRUE;
}

gboolean
clip_recorder_stop(ClipRecorder *recorder, GError **err)
{
  if (recorder->active_pipeline) {
    GstEvent *eos = gst_event_new_eos ();
    gst_element_send_event(GST_ELEMENT(recorder->active_pipeline), eos);
  }
  return TRUE;
}

gint64 clip_recorder_recorded_length(ClipRecorder *recorder)
{
  return recorder->last_record_length;
}
