#include <gst/gst.h>
#include <gio/gio.h>
#include <stdlib.h>

#define SEGMENT_TEST_ERROR (sequence_test_error_quark())
enum {
  SEGMENT_TEST_ERROR_CREATE_ELEMENT_FAILED = 1,
  SEGMENT_TEST_ERROR_SEEK_FAILED,
};

GQuark
sequence_test_error_quark(void)
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("sequence-test-error-quark");
  return error_quark;
}
static GMainLoop *main_loop = NULL;

static gboolean
bus_call (GstBus     *bus,
	  GstMessage *msg,
	  gpointer    data)
{
  g_debug("bus_call: %s", GST_MESSAGE_TYPE_NAME(msg));
  switch (GST_MESSAGE_TYPE (msg)) {
  case GST_MESSAGE_EOS:
    g_debug ("End-of-stream");
    g_main_loop_quit(main_loop);
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
  case GST_MESSAGE_SEGMENT_DONE:
    {
      GstFormat format;
      gint64 pos;
      gst_message_parse_segment_done(msg, &format, &pos);
      g_debug("Segment done %lld", pos);
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

static GstElement *
create_pipeline(GError **err)
{
  GstBus *bus;
  GstElement *pipeline;
  GstElement *sine_src;
  GstElement *wavenc;
  GstElement *filesink;
  GstCaps *channel_caps;
  
  pipeline = gst_pipeline_new ("pipeline");
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_watch (bus, bus_call, NULL);
  gst_object_unref (bus);

  
  sine_src = gst_element_factory_make ("audiotestsrc", "silence");
  if (!sine_src) {
    g_set_error(err, SEGMENT_TEST_ERROR,
		SEGMENT_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create sine");
    g_object_unref(pipeline);
    return NULL;
  }
  gst_bin_add(GST_BIN(pipeline), sine_src);
  
   
  wavenc = gst_element_factory_make ("wavenc", "wavenc");
  if (!wavenc) {
    g_set_error(err, SEGMENT_TEST_ERROR,
		SEGMENT_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create WAV encoder");
    return NULL;
  }
  gst_bin_add(GST_BIN(pipeline), wavenc);
  
  filesink = gst_element_factory_make ("giosink", "filesink");
  if (!filesink) {
    g_set_error(err, SEGMENT_TEST_ERROR,
		  SEGMENT_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create file sink");
    g_object_unref(pipeline);
    return NULL;
  }
  
  gst_bin_add(GST_BIN(pipeline), filesink);
  channel_caps = gst_caps_new_simple("audio/x-raw", "channels",G_TYPE_INT, 2, NULL);
  if (!gst_element_link_pads_filtered(sine_src,NULL, wavenc, NULL, channel_caps)) {
    g_set_error(err, SEGMENT_TEST_ERROR,
		  SEGMENT_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to link pipeline");
    g_object_unref(pipeline);
    return NULL;
  }
  if (!gst_element_link_many(wavenc, filesink, NULL)) {
    g_set_error(err, SEGMENT_TEST_ERROR,
		  SEGMENT_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to link pipeline");
    g_object_unref(pipeline);
    return NULL;
  }
  gst_caps_unref(channel_caps);
  return pipeline;
}

static gboolean 
set_element_file(GstBin *parent, const gchar *name, const char *filename,
		 GError **err)
{
  GstElement *element;
  GFile *file = g_file_new_for_commandline_arg(filename);
  element = gst_bin_get_by_name(parent, name);
  g_assert(element);
  g_object_set(element, "file", file, NULL);
  g_object_unref(file);
  g_object_unref(element);
  return TRUE;
}

int
main(int argc, char **argv)
{
  GError *err = NULL;
  GstElement *pipe;
  GOptionContext *option_ctxt;
  guint n_samples = 10;
  
  option_ctxt = g_option_context_new("");
  g_option_context_add_group(option_ctxt, gst_init_get_option_group());
  if (!g_option_context_parse(option_ctxt, &argc, &argv, &err)) {
    g_error("Failed to parse options: %s", err->message);
    g_error_free(err);
    return EXIT_FAILURE;
  }
  g_option_context_free(option_ctxt);
  
  if (argc < 2) {
    g_error("usage: %s <number of samples>", argv[0]);
    return EXIT_FAILURE;
  }
  
  n_samples = atoi(argv[1]);
  
  pipe = create_pipeline(&err);
  if (!pipe) {
    g_error("Failed to create pipeline: %s", err->message);
    g_error_free(err);
    return EXIT_FAILURE;
  }

  
  set_element_file(GST_BIN(pipe), "filesink", "/tmp/segment.wav", &err);
  
  main_loop = g_main_loop_new (NULL, FALSE);

  gst_element_set_state(pipe, GST_STATE_PAUSED);
  gst_element_get_state(pipe, NULL, NULL, GST_CLOCK_TIME_NONE);
  gst_element_seek(pipe, 1.0, GST_FORMAT_DEFAULT,
		   GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH,
		   GST_SEEK_TYPE_SET, 0,
		   GST_SEEK_TYPE_SET, n_samples);
		   
  gst_element_set_state(pipe, GST_STATE_PLAYING);
  g_main_loop_run(main_loop);
  g_main_loop_unref(main_loop);
  gst_element_set_state(pipe, GST_STATE_NULL);
  return EXIT_SUCCESS;
}
