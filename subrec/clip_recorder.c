#include <clip_recorder.h>
#include <string.h>
#include <math.h>

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
  if (recorder->adjust_pipeline) {
    gst_element_set_state(GST_ELEMENT(recorder->adjust_pipeline),
			  GST_STATE_NULL);
    g_clear_object(&recorder->adjust_pipeline);
  }
}

#define DEFAULT_TRIM_LEVEL 0.1

enum
{
  PROP_0 = 0,
  PROP_TRIM_LEVEL,
};

enum {
  RUN_ERROR,
  RECORDING,
  PLAYING,
  STOPPED,
  POWER,
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
clip_recorder_power(ClipRecorder *recorder, gdouble power)
{
}

static void
clip_recorder_set_property (GObject * object, guint prop_id,
			    const GValue * value, GParamSpec * pspec);

static void
clip_recorder_get_property (GObject * object, guint prop_id,
			    GValue * value, GParamSpec * pspec);

static void
clip_recorder_class_init(ClipRecorderClass *obj_class)
{
  GParamSpec *pspec;
  GObjectClass *gobject_class = G_OBJECT_CLASS (obj_class);
  gobject_class->finalize = clip_recorder_finalize;
  gobject_class->set_property = clip_recorder_set_property;
  gobject_class->get_property = clip_recorder_get_property;
  obj_class->run_error = clip_recorder_run_error;
  obj_class->recording = clip_recorder_recording;
  obj_class->playing = clip_recorder_playing;
  obj_class->stopped = clip_recorder_stopped;
  obj_class->power = clip_recorder_power;

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
  clip_recorder_signals[POWER] =
    g_signal_new("power",
		 G_OBJECT_CLASS_TYPE (obj_class), G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET(ClipRecorderClass, power),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__DOUBLE,
		 G_TYPE_NONE, 1, G_TYPE_DOUBLE);

  /* Properties */
  
   /* trim-level */
  pspec =  g_param_spec_double ("trim-level",
				"Power level used for trimming",
				"Given as a fraction (not dB).",
				0.0, 1.0,
				DEFAULT_TRIM_LEVEL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  
  g_object_class_install_property(gobject_class, PROP_TRIM_LEVEL, pspec);
}


static void
clip_recorder_init(ClipRecorder *recorder)
{
  recorder->record_pipeline = NULL;
  recorder->adjust_pipeline = NULL;
  recorder->playback_pipeline = NULL;
  recorder->active_pipeline = NULL;

  recorder->trim_level = DEFAULT_TRIM_LEVEL;

}

static void
clip_recorder_set_property (GObject * object, guint prop_id,
			    const GValue * value, GParamSpec * pspec)
{
  ClipRecorder *recorder = CLIP_RECORDER(object);
  switch (prop_id) {
  case PROP_TRIM_LEVEL:
    recorder->trim_level = g_value_get_double (value);
    if (recorder->record_pipeline) {
      GstElement *analysis =
	gst_bin_get_by_name(GST_BIN(recorder->record_pipeline), "analyze");
      g_assert(analysis);
      g_object_set(analysis, "trim-level", recorder->trim_level, NULL);
      g_object_unref(analysis);
    }
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}



static void
clip_recorder_get_property (GObject * object, guint prop_id,
			    GValue * value, GParamSpec * pspec)
{
  ClipRecorder *recorder = CLIP_RECORDER(object);
  switch (prop_id) {
  case PROP_TRIM_LEVEL:
    g_value_set_double (value, recorder->trim_level);
    break;
    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static GstPipeline *
get_adjust_pipeline(ClipRecorder *recorder, GError **err);

#define TARGET_LOUDNESS 5.01187233627e-3 /* -23dB */
static void
start_adjustment(ClipRecorder *recorder)
{
  GstStateChangeReturn state_ret;
  GstPipeline *adjust;
  GstElement *filesrc;
  GstElement *amplifier;
  GstClockTimeDiff duration = recorder->trim_end - recorder->trim_start;
  gfloat amplification;
  adjust = get_adjust_pipeline(recorder, NULL);
  g_assert(adjust);
  filesrc = gst_bin_get_by_name(GST_BIN(adjust), "filesrc");
  g_assert(filesrc);
  g_object_set(filesrc,
	       "start", (GstClockTime)0,
	       "duration", duration,
	       "media-start", recorder->trim_start,
	       "media-duration", duration,
	       NULL);
  g_object_unref(filesrc);
  amplification = sqrt(TARGET_LOUDNESS / recorder->loudness);
  g_debug("Amplify by %f", amplification);
  amplifier = gst_bin_get_by_name(GST_BIN(adjust), "amplify");
  g_assert(amplifier);
  g_object_set(amplifier,
	       "amplification", amplification,
	       "clipping-method", 3, /* No clipping */
	       NULL);
  g_object_unref(amplifier);
  
  state_ret = gst_element_set_state(GST_ELEMENT(adjust), GST_STATE_PLAYING);
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    GError *err = NULL;
    g_set_error(&err, CLIP_RECORDER_ERROR, CLIP_RECORDER_ERROR_STATE,
		"Failed to set state of adjustment pipeline to PLAYING");
    g_signal_emit(recorder, clip_recorder_signals[RUN_ERROR], 0, err);
    g_error_free (err);
    return;
  }
  recorder->active_pipeline = adjust;
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
	    gst_element_set_state(GST_ELEMENT(recorder->active_pipeline),
				  GST_STATE_NULL);
	    recorder->active_pipeline = NULL;
	    if (msg->src == (GstObject*)recorder->record_pipeline) {
	      start_adjustment(recorder);
	    } else {
	      g_signal_emit(recorder, clip_recorder_signals[STOPPED], 0);
	    }
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
  case GST_MESSAGE_ELEMENT:
    {
      const gchar *name = gst_structure_get_name(msg->structure);
      if (strcmp(name, "sub-block-message") == 0) {
	gdouble power;
	if (gst_structure_get_double (msg->structure, "power", &power)) {
	  g_signal_emit(recorder, clip_recorder_signals[POWER], 0, power);
	}
      } else if (strcmp(name, "analysis-message") == 0) {
	gst_structure_get_double (msg->structure, "loudness",
				  &recorder->loudness);
	gst_structure_get_clock_time (msg->structure, "trim-start",
				      &recorder->trim_start);
	gst_structure_get_clock_time (msg->structure, "trim-end",
				      &recorder->trim_end);
	
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
  GstElement *analyze;
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

    analyze = gst_element_factory_make ("audiormspower", "analyze");
    if (!analyze) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create audio analyzer");
      gst_object_unref(pipeline);
      return NULL;
    }
    g_object_set(analyze, "analysis-message", TRUE, NULL);
    g_object_set(analyze, "trim-level", recorder->trim_level, NULL);
    gst_bin_add(GST_BIN(pipeline), analyze);

    
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

    if (!gst_element_link_many(input, convert1, analyze, convert2,
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
output_pad_added (GstElement* object, GstPad* new_pad, GstElement *sink_elem)
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
      g_warning("Linking new pad failed: %d", link_ret);
    }
  }
}
static GstPipeline *
get_adjust_pipeline(ClipRecorder *recorder, GError **err)
{
  
  GstElement *pipeline;
  GstBus *bus;
  GstElement *filesrc;
  GstElement *composition;
  GstElement *high_pass;
  GstElement *amplify;
  GstElement *limit;
  GstElement *convert1;
  GstElement *convert2;
  GstElement *wavenc;
  GstElement *filesink;
  GstCaps *output_filter;
  if (!recorder->adjust_pipeline) {
    pipeline = gst_pipeline_new ("playback");
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, recorder);
    gst_object_unref (bus);

    composition = gst_element_factory_make ("gnlcomposition", "srccomposition");
    if (!composition) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to source composition");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), composition);
    
   

    filesrc = gst_element_factory_make ("gnlurisource", "filesrc");
    if (!filesrc) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create URI source");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(composition), filesrc);

    convert1 = gst_element_factory_make ("audioconvert", "convert1");
    if (!convert1) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create first format converter");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), convert1);

    high_pass = gst_element_factory_make ("audiocheblimit", "highpass");
    if (!high_pass) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create high pass filter");
      gst_object_unref(pipeline);
      return NULL;
    }
    g_object_set(high_pass, "mode", 1, "poles", 2, "cutoff", (gfloat)100, NULL);
    gst_bin_add(GST_BIN(pipeline), high_pass);
    
    amplify = gst_element_factory_make ("audioamplify", "amplify");
    if (!amplify) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create amplifier");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), amplify);
    
    g_signal_connect(composition, "pad-added",
		     (GCallback)output_pad_added, convert1);
     
    limit = gst_element_factory_make ("rglimiter", "limit");
    if (!limit) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create limiter");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), limit);
    
    convert2 = gst_element_factory_make ("audioconvert", "convert2");
    if (!convert2) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create second format converter");
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

    
    filesink = gst_element_factory_make ("giosink", "filesink");
    if (!filesink) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
		  "Failed to create file sink");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_bin_add(GST_BIN(pipeline), filesink);


    if (!gst_element_link_many(convert1, high_pass, amplify, limit,
			       convert2, NULL)) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_LINK_FAILED,
		  "Failed to link adjust pipeline (first part)");
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
		  "Failed to link adjust pipeline (second part)");
      gst_object_unref(pipeline);
      return NULL;
    }
    gst_caps_unref(output_filter);
    
    if (!gst_element_link(wavenc, filesink)) {
      g_set_error(err, CLIP_RECORDER_ERROR,
		  CLIP_RECORDER_ERROR_LINK_FAILED,
		  "Failed to link adjust pipeline (last part)");
      gst_object_unref(pipeline);
      return NULL;
    }
    recorder->adjust_pipeline = GST_PIPELINE(pipeline);
  }
  return recorder->adjust_pipeline;
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
		     (GCallback)output_pad_added, convert);

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

static gboolean
signal_connected(gpointer instance, guint signal_id)
{
  return g_signal_handler_find (instance, G_SIGNAL_MATCH_ID, signal_id, 0, NULL,
				NULL, NULL) != 0;
}

static GFile *
create_raw_file(GFile *orig)
{
  GFile *new_file;
  char *base = g_file_get_basename (orig);
  char *split = strrchr(base,'.');
  char *new_base;
  GFile *dir;
  if (split) {
    *split++ = '\0';
    new_base = g_strconcat(base,"_raw.",split, NULL);
  } else {
    new_base = g_strconcat(base,"_raw", NULL);
  }
  g_free(base);
  dir = g_file_get_parent (orig);
  new_file = g_file_resolve_relative_path (dir, new_base);
  g_object_unref(dir);
  g_free(new_base);
  return new_file;
}

gboolean
clip_recorder_record(ClipRecorder *recorder, GFile *file,GError **err)
{
  GstStateChangeReturn state_ret;
  GstPipeline *pipeline;
  GstPipeline *adjust;
  GstElement *filesink;
  GstElement *adjustsrc;
  GstElement *adjustsink;
  GstElement *analyze;
  GFile *raw_file;
  char *uri;
  cancel_active_pipeline(recorder);
  pipeline = get_record_pipeline(recorder, err);
  if (!pipeline) {
    return FALSE;
  }
  /* Just create the pipeline and catch any errors */
  adjust = get_adjust_pipeline(recorder, err);
  if (!adjust) {
    return FALSE;
  }

  raw_file = create_raw_file(file);
  
  filesink = gst_bin_get_by_name(GST_BIN(pipeline), "file");
  g_assert(filesink);
  g_object_set(filesink, "file", raw_file, NULL);
  g_object_unref(filesink);

  adjustsrc = gst_bin_get_by_name(GST_BIN(adjust), "filesrc");
  g_assert(adjustsrc);
  uri = g_file_get_uri(raw_file);
  
  g_object_set(adjustsrc, "uri", uri, NULL);
  g_free(uri);
  g_object_unref(adjustsrc);
  
  g_object_unref(raw_file);
  
  adjustsink = gst_bin_get_by_name(GST_BIN(adjust), "filesink");
  g_assert(adjustsink);
  g_object_set(adjustsink, "file", file, NULL);
  g_object_unref(adjustsink);
  
  analyze = gst_bin_get_by_name(GST_BIN(pipeline), "analyze");
  g_object_set(analyze,"sub-block-message",
	       signal_connected(recorder, clip_recorder_signals[POWER]), NULL);
  g_object_unref(analyze);
  
  state_ret = gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    g_set_error(err, CLIP_RECORDER_ERROR, CLIP_RECORDER_ERROR_STATE,
		"Failed to set state of recording pipeline to PLAYING");
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
		"Failed to set state of play pipeline to PLAYING");
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

GstClockTimeDiff clip_recorder_recorded_length(ClipRecorder *recorder)
{
  return recorder->trim_end - recorder->trim_start;
}
