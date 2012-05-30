#include <gst/gst.h>
#include <gio/gio.h>
#include <blocked_seek.h>

#define SEQUENCE_TEST_ERROR (sequence_test_error_quark())
enum {
  SEQUENCE_TEST_ERROR_CREATE_ELEMENT_FAILED = 1,
  SEQUENCE_TEST_ERROR_SEEK_FAILED,
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



typedef struct _SequenceContext
{
  GstElement *pipeline;  /*referenced */
  GstElement *file_src_bin;
  GstElement *file_src;
  GstElement *silence_src;
  GstElement *active_src;
  BlockedSeek *blocked_seek;
  GstElement *output_element;
  char **files;
  guint n_files;
  guint index;
} SequenceContext;

static void
sequence_ctxt_init(SequenceContext *ctxt)
{
  ctxt->pipeline = NULL;
  ctxt->file_src_bin = NULL;
  ctxt->file_src = NULL;
  ctxt->active_src = NULL;
  ctxt->blocked_seek = NULL;
}

static void
sequence_ctxt_clear(SequenceContext *ctxt)
{
  if (ctxt->blocked_seek) {
    blocked_seek_destroy(ctxt->blocked_seek);
    ctxt->blocked_seek = NULL;
  }
  g_clear_object(&ctxt->pipeline);
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
next_file(SequenceContext *ctxt, GError **err)
{
  GstSeekFlags seek_flags;
  GstState state;
  GstState pending;
  GstPad *input_src_pad;
  GFile *file;

  
  if (ctxt->active_src) {
    gst_element_set_state(ctxt->active_src, GST_STATE_PAUSED);
    gst_element_get_state(ctxt->active_src, &state, &pending, GST_CLOCK_TIME_NONE);
    gst_element_unlink(ctxt->active_src, ctxt->output_element);
    gst_element_set_state(ctxt->active_src, GST_STATE_READY);
    gst_element_get_state(ctxt->active_src, &state, &pending, GST_CLOCK_TIME_NONE);
  }
  
  if (ctxt->active_src == ctxt->file_src_bin) {
    ctxt->active_src = ctxt->silence_src;

#if 0
    gst_element_set_state(ctxt->active_src, GST_STATE_READY);
    gst_element_get_state(ctxt->active_src, &state, &pending, GST_CLOCK_TIME_NONE);
#endif
    
    input_src_pad = gst_element_get_static_pad(ctxt->active_src, "src");
    seek_flags = GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH|GST_SEEK_FLAG_SEGMENT;
    blocked_seek_start(ctxt->blocked_seek, ctxt->active_src, input_src_pad,
		       GST_FORMAT_TIME,
		       seek_flags,
		       GST_SEEK_TYPE_SET, 0,
		       GST_SEEK_TYPE_SET, GST_SECOND);
    g_object_unref(input_src_pad);
  } else {
    ctxt->active_src = ctxt->file_src_bin;

    file = g_file_new_for_commandline_arg(ctxt->files[ctxt->index++]);

    g_object_set(ctxt->file_src, "file", file, NULL);
    g_object_unref(file);
    input_src_pad = gst_element_get_static_pad(ctxt->active_src, "src");
    seek_flags = GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_FLUSH;
    if (ctxt->index != ctxt->n_files) {
    seek_flags |= GST_SEEK_FLAG_SEGMENT;
    }
    blocked_seek_start(ctxt->blocked_seek, ctxt->active_src, input_src_pad,
		       GST_FORMAT_TIME,
		       seek_flags,
		       GST_SEEK_TYPE_SET, 0,
		       GST_SEEK_TYPE_END, 0);
    g_object_unref(input_src_pad);
    
  }
  
  gst_element_link(ctxt->active_src, ctxt->output_element);


		     
  gst_element_set_state(ctxt->active_src, GST_STATE_PLAYING);
  
  
  return TRUE;
}

static gboolean
bus_call (GstBus     *bus,
	  GstMessage *msg,
	  gpointer    data)
{
  SequenceContext *ctxt = data;
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
      next_file(ctxt, NULL);
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
create_pipeline(SequenceContext *ctxt, GError **err)
{
  GstBus *bus;
  GstElement *wavparse;
  GstElement *wavenc;
  GstElement *filesink;
  GstPad *ghost_pad;
  
  ctxt->pipeline = gst_pipeline_new ("pipeline");
  bus = gst_pipeline_get_bus (GST_PIPELINE (ctxt->pipeline));
  gst_bus_add_watch (bus, bus_call, ctxt);
  gst_object_unref (bus);

  
  ctxt->file_src_bin = gst_bin_new("filesrc_bin");
  if (!ctxt->file_src_bin) {
    g_set_error(err, SEQUENCE_TEST_ERROR,
		SEQUENCE_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create file source bin");
    return NULL;
  }
  ghost_pad = gst_ghost_pad_new_no_target ("src", GST_PAD_SRC);
  gst_element_add_pad(ctxt->file_src_bin, ghost_pad);
  
  ctxt->file_src = gst_element_factory_make ("giosrc", "filesrc");
  if (!ctxt->file_src) {
    g_set_error(err, SEQUENCE_TEST_ERROR,
		  SEQUENCE_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create file source");
    return NULL;
  }
  gst_bin_add(GST_BIN(ctxt->file_src_bin), ctxt->file_src);
  
  wavparse = gst_element_factory_make ("wavparse", "wavdec");
  if (!wavparse) {
    g_set_error(err, SEQUENCE_TEST_ERROR,
		  SEQUENCE_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create WAV decoder");
    return NULL;
  }
  gst_bin_add(GST_BIN(ctxt->file_src_bin), wavparse);

  g_signal_connect(wavparse, "pad-added", G_CALLBACK(set_ghost_target), ghost_pad);

  gst_element_set_locked_state(ctxt->file_src_bin, TRUE);
  gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->file_src_bin);

  ctxt->silence_src = gst_element_factory_make ("audiotestsrc", "silence");
  if (!ctxt->silence_src) {
    g_set_error(err, SEQUENCE_TEST_ERROR,
		SEQUENCE_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create silence");
    return NULL;
  }
  gst_element_set_locked_state(ctxt->silence_src, TRUE);
  gst_bin_add(GST_BIN(ctxt->pipeline), ctxt->silence_src);
  
   
  wavenc = gst_element_factory_make ("wavenc", "wavenc");
  if (!wavenc) {
    g_set_error(err, SEQUENCE_TEST_ERROR,
		SEQUENCE_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create WAV encoder");
    return NULL;
  }
  gst_bin_add(GST_BIN(ctxt->pipeline), wavenc);
  ctxt->output_element = wavenc;
  
  filesink = gst_element_factory_make ("giosink", "filesink");
  if (!filesink) {
    g_set_error(err, SEQUENCE_TEST_ERROR,
		  SEQUENCE_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to create file sink");
    return NULL;
  }
  
  gst_bin_add(GST_BIN(ctxt->pipeline), filesink);

  if (!gst_element_link_many(ctxt->file_src, wavparse, NULL)) {
    g_set_error(err, SEQUENCE_TEST_ERROR,
		  SEQUENCE_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to link source bin");
    return NULL;
  }
  
  if (!gst_element_link_many(wavenc, filesink, NULL)) {
    g_set_error(err, SEQUENCE_TEST_ERROR,
		  SEQUENCE_TEST_ERROR_CREATE_ELEMENT_FAILED,
		"Failed to link pipeline (last part)");
    return NULL;
  }

 
  
  return ctxt->pipeline;
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
  SequenceContext ctxt;
  GOptionContext *option_ctxt;
  sequence_ctxt_init(&ctxt);
  option_ctxt = g_option_context_new(" <dest> <source>...");
  g_option_context_add_group(option_ctxt, gst_init_get_option_group());
  if (!g_option_context_parse(option_ctxt, &argc, &argv, &err)) {
    g_error("Failed to parse options: %s", err->message);
    g_error_free(err);
    sequence_ctxt_clear(&ctxt);
    return EXIT_FAILURE;
  }
  g_option_context_free(option_ctxt);
  
  if (argc < 3) {
    g_error("usage: %s <dest> <source>...", argv[0]);
    return EXIT_FAILURE;
  }
  ctxt.files = argv + 2;
  ctxt.index = 0;
  ctxt.n_files = argc - 2;
  pipe = create_pipeline(&ctxt, &err);
  if (!pipe) {
    g_error("Failed to create pipeline: %s", err->message);
    g_error_free(err);
    sequence_ctxt_clear(&ctxt);
    return EXIT_FAILURE;
  }
  set_element_file(GST_BIN(pipe), "filesink", argv[1], &err);
  main_loop = g_main_loop_new (NULL, FALSE);

  gst_element_set_state(pipe, GST_STATE_PLAYING);
  ctxt.blocked_seek = blocked_seek_new();

  if (!next_file(&ctxt, &err)) {
    g_error("Failed to setup first file: %s", err->message);
    g_error_free(err);
    sequence_ctxt_clear(&ctxt);
    g_main_loop_unref(main_loop);
    return EXIT_FAILURE;
  }

  gst_element_set_state(pipe, GST_STATE_PLAYING);
  g_main_loop_run(main_loop);
  g_main_loop_unref(main_loop);
  gst_element_set_state(ctxt.silence_src, GST_STATE_NULL);
  gst_element_set_state(ctxt.file_src_bin, GST_STATE_NULL);
  gst_element_set_state(pipe, GST_STATE_NULL);
  sequence_ctxt_clear(&ctxt);
  return EXIT_SUCCESS;
}
