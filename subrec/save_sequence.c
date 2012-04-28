#include "save_sequence.h"
#include <time_string.h>

GQuark
save_sequence_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("save-sequence-error-quark");
  return error_quark;
}

typedef struct _SequenceBuildContext
{
  SubtitleStore *subtitles;
  GstClockTime start;
  GstClockTime end;
  GstElement *sequence;
  GFile *working_directory;
} SequenceBuildContext;
  
static gboolean
build_sequence_children(SequenceBuildContext *ctxt, GtkTreeIter *iter,
			GError **err)
{
  do {
    GtkTreeIter child;
    if (gtk_tree_model_iter_children (GTK_TREE_MODEL(ctxt->subtitles),
				      &child, iter)) {
      if (!build_sequence_children(ctxt, &child,err)) {
	return FALSE;
      }
    } else {
      GstElement *src;
      GstClockTime in;
      GstClockTime out;
      GstClockTimeDiff duration;
      const gchar *filename;
      GFile *file;
      char *uri;
      gtk_tree_model_get(GTK_TREE_MODEL(ctxt->subtitles), iter,
			 SUBTITLE_STORE_COLUMN_GLOBAL_IN, &in,
			 SUBTITLE_STORE_COLUMN_GLOBAL_OUT, &out,
			 SUBTITLE_STORE_COLUMN_FILE_DURATION, &duration,
			 -1);
#if 0
      {
	gchar in_str[TIME_STRING_MIN_LEN];
	gchar out_str[TIME_STRING_MIN_LEN];
	time_string_format(out_str, TIME_STRING_MIN_LEN, out);
	time_string_format(in_str, TIME_STRING_MIN_LEN, in);
	g_debug("%s-%s", in_str ,out_str);
      }
#endif
      filename = subtitle_store_get_filename(ctxt->subtitles, iter);
      if (filename) {
	file = g_file_get_child(ctxt->working_directory, filename);
	if (!g_file_query_exists(file, NULL)) {
	  g_set_error(err, SAVE_SEQUENCE_ERROR,
		      SAVE_SEQUENCE_ERROR_FILE_NOT_FOUND,
		      "No audiofile named %s", g_file_get_path(file));
	  g_object_unref(file);
	  return FALSE;
	}
	src = gst_element_factory_make ("gnlurisource", NULL);
	if (!src) {
	  g_set_error(err, SAVE_SEQUENCE_ERROR,
		      SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		      "Failed to create URI-source");
	  g_object_unref(file);
	  return FALSE;
	}
	gst_bin_add(GST_BIN(ctxt->sequence), src);
	uri = g_file_get_uri(file);
	g_object_unref(file);
	g_object_set(src, "uri", uri,
		     "media-start", (GstClockTime)0,
		     "media-duration", duration,
		     "start", in,
		     "duration", duration,
		     NULL);
	g_free(uri);
#if 0
	{
	  char *uri;
	  GstClockTime start;
	  GstClockTime duration;
	  g_object_get(src, "uri", &uri, "start", &start,
		       "duration", &duration, NULL);
	  g_debug("URI %s", uri);
	  g_free(uri);
	  {
	    gchar in_str[TIME_STRING_MIN_LEN];
	    gchar out_str[TIME_STRING_MIN_LEN];
	    time_string_format(out_str, TIME_STRING_MIN_LEN, duration);
	    time_string_format(in_str, TIME_STRING_MIN_LEN, start);
	    g_debug("%s+'%s", in_str ,out_str);
	  }
	}
#endif
      }
    }
  } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(ctxt->subtitles), iter));
  return TRUE;
}

static gboolean
build_sequence(SequenceBuildContext *ctxt,
	       GError **err)
{
  GtkTreeIter top;
  if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ctxt->subtitles), &top))
    return TRUE;
  return build_sequence_children(ctxt, &top, err);

}
static void
pad_added (GstElement *element, GstPad *new_pad, gpointer user_data)
{
  GstPadLinkReturn link_ret;
  GstPad *sink_pad = (GstPad*)user_data;
  g_debug("Got new pad");
  link_ret = gst_pad_link(new_pad, sink_pad);
  if (link_ret != GST_PAD_LINK_OK) {
    g_warning("Linking sequence to output failed: %d", link_ret);
  }
}

static gboolean
bus_call (GstBus     *bus,
	  GstMessage *msg,
	  gpointer    data)
{
  /* SequenceBuildContext *ctxt = data; */
  /* g_debug("bus_call: %s", GST_MESSAGE_TYPE_NAME(msg));  */
  switch (GST_MESSAGE_TYPE (msg)) {
  case GST_MESSAGE_EOS:
    g_debug ("End-of-stream");
    return FALSE;
  case GST_MESSAGE_ERROR: {
    GError *err = NULL;
    gchar *debug = NULL;
 
    gst_message_parse_error (msg, &err, &debug);

    g_error_free (err);
    
    if (debug) {
      g_debug ("Debug details: %s\n", debug);
      g_free (debug);
    }
    return FALSE;
  }
  default:
    break;
  }


  return TRUE;
}

static GstElement *
create_silence_source(GstClockTime start, GstClockTime end, GError **err)
{
  GstBin *bin;
  GstElement *silence;
  GstElement *caps_filter;
  GstElement *default_src;
  GstCaps *caps;
  GstPad *ghost_pad;
  
  bin = (GstBin*)gst_bin_new("silence_bin");
  silence = gst_element_factory_make ("audiotestsrc", "silence");
  if (!silence) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to silence");
    g_object_unref(bin);
    return NULL;
  }
  g_object_set(silence, "wave", 4, /* silence */
	       NULL);
  gst_bin_add(bin, silence);

  caps_filter = gst_element_factory_make ("capsfilter", "silence_capsfilter");
  if (!caps_filter) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create caps filter");
    g_object_unref(bin);
    return NULL;
  }
  
  
  caps = gst_caps_new_simple("audio/x-raw-int",
			     "rate", G_TYPE_INT, 48000,
			     "depth", G_TYPE_INT, 16,
			     "channels", G_TYPE_INT, 1,
			     NULL);
  g_object_set(caps_filter, "caps", caps, NULL);
  gst_caps_unref(caps);
  gst_bin_add(bin, caps_filter);
  
  if (!gst_element_link(silence, caps_filter)) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_LINK_FAILED,
		"Failed to link silence source with casp filter");
    g_object_unref(bin);
    return NULL;
  }
  ghost_pad = gst_ghost_pad_new("src",
				gst_element_get_static_pad(caps_filter, "src"));
  gst_element_add_pad(GST_ELEMENT(bin), ghost_pad);
  
  default_src = gst_element_factory_make ("gnlsource", "default");
  if (!default_src) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to defult source");
    g_object_unref(bin);
    return NULL;
  }
  
  g_object_set(default_src, "priority", 10,
	       "media-start", (GstClockTime)0,
	       "start", start,
	       "duration", end-start,
	       NULL);
  gst_bin_add(GST_BIN(default_src), GST_ELEMENT(bin));
  return default_src;
}
static GstElement *
build_pipeline(GstElement *src, GFile *file, GError **err)
{
  GstBus *bus;
  GstElement *pipeline;
  GstElement *caps_filter;
  GstCaps *caps;
  GstElement *wavenc;
  GstElement *filesink;
  GstPad *first_sink;

  pipeline = gst_pipeline_new ("pipeline");
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, bus_call, NULL);
  gst_object_unref (bus);

  gst_bin_add(GST_BIN(pipeline), src);

  /* Caps filter */

  caps_filter = gst_element_factory_make ("capsfilter", "capsfilter");
  if (!caps_filter) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create caps filter");
    g_object_unref(pipeline);
    return NULL;
  }
  caps = gst_caps_new_simple("audio/x-raw-int",
			     "rate", G_TYPE_INT, 48000,
			     "depth", G_TYPE_INT, 16,
			     "channels", G_TYPE_INT, 1,
			     NULL);
  g_object_set(caps_filter, "caps", caps, NULL);
  gst_caps_unref(caps);
  gst_bin_add(GST_BIN(pipeline), caps_filter);
  
  /* WAV encoder */
  wavenc = gst_element_factory_make ("wavenc", "wav");
  if (!wavenc) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create WAV encoder");
    g_object_unref(pipeline);
    return NULL;
  }
  gst_bin_add(GST_BIN(pipeline), wavenc);

  /* File sink */
  filesink = gst_element_factory_make ("giosink", "file");
  if (!filesink) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create file sink");
    g_object_unref(pipeline);
    return NULL;
  }
  gst_bin_add(GST_BIN(pipeline), filesink);
  g_object_set(filesink, "file", file, "sync", FALSE, NULL);

  
  if (!gst_element_link_many(caps_filter, wavenc, filesink, NULL)) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_LINK_FAILED,
		"Failed to link encoder and filesink");
    g_object_unref(pipeline);
    return NULL;
  }

  first_sink = gst_element_get_static_pad(caps_filter, "sink");
  g_signal_connect_data(src, "pad-added", G_CALLBACK(pad_added), first_sink,
			(GClosureNotify)g_object_unref, 0);
  return pipeline;
}
      
gboolean
save_sequence(SubtitleStore *subtitles, GFile *working_directory,
	      GstClockTime start, GstClockTime end,
	      GError **err)
{
  gboolean ret;
  GstElement *pipeline;
  GstElement *default_src;
  SequenceBuildContext ctxt;
  GFile *out_file = g_file_new_for_path("/tmp/seq.wav");
  
  ctxt.subtitles = subtitles;
  ctxt.start = start;
  ctxt.end = end;
  ctxt.working_directory = working_directory;
  ctxt.sequence = gst_element_factory_make ("gnlcomposition", NULL);
  if (!ctxt.sequence) {
    g_set_error(err, SAVE_SEQUENCE_ERROR,
		SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create sequence composition");
    return FALSE;
  }
  g_object_set(ctxt.sequence, "start", start, "duration", end -start , NULL);

  {
    GstCaps *caps;
    caps = gst_caps_new_simple("audio/x-raw-int",
			       "rate", G_TYPE_INT, 48000,
			       "depth", G_TYPE_INT, 16,
			     "channels", G_TYPE_INT, 1,
			       NULL);
    g_object_set(ctxt.sequence, "caps", caps, NULL);
    gst_caps_unref(caps);
  }
  

  default_src = create_silence_source(start, end,err);
  if (!default_src) {
    g_object_unref(ctxt.sequence);
    return FALSE;
  }
  
  gst_bin_add(GST_BIN(ctxt.sequence), default_src);
  
  
  ret = build_sequence(&ctxt, err);
  if (!ret) {
    g_object_unref(ctxt.sequence);
    return FALSE;
  }
  pipeline = build_pipeline(ctxt.sequence, out_file, err);
  if (!pipeline) {
    return FALSE;
  }

  gst_element_set_state(pipeline, GST_STATE_PLAYING);
  while(TRUE) {
    GstState state;
    GstStateChangeReturn ret;
    ret = gst_element_get_state(pipeline, &state, NULL, GST_SECOND);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      g_set_error(err, SAVE_SEQUENCE_ERROR,
		  SAVE_SEQUENCE_ERROR_PIPELINE,
		  "Failed to start saving pipeline");
      gst_element_set_state(pipeline, GST_STATE_NULL);
      g_object_unref(pipeline);
      return FALSE;
    } else if (ret == GST_STATE_CHANGE_SUCCESS
	       && state == GST_STATE_PLAYING) {
      break;
    }
  }
#if 0
  gst_element_set_state(pipeline, GST_STATE_NULL);
  g_object_unref(pipeline);
#endif
  return TRUE;
}
 
