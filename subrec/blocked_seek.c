#include "blocked_seek.h"
#include <glib.h>

enum BlockedSeekState
  {
    IDLE = 0,
    INITIAL_PREROLL,
    SEEK_PREROLL,
    PLAYING,
    EXIT
  };

struct _BlockedSeek
{
  GMutex mutex;
  GCond cond;
  GThread *thread;
  enum BlockedSeekState state;
  GstElement *element;
  GstPad *exit_pad; /* Last src pad in chain to be seeked */
  GstEvent *seek_event;
};


static void
seek_done()
{
  g_debug("Seek done");
}

static void
preroll_done(GstPad *pad, gboolean blocked, gpointer user_data)
{
  BlockedSeek *bs = user_data;
  g_mutex_lock(&bs->mutex);
  switch(bs->state) {
  case INITIAL_PREROLL:
    bs->state = SEEK_PREROLL;
   g_cond_signal(&bs->cond);
   break;
  case SEEK_PREROLL:
    bs->state = PLAYING;
    g_cond_signal(&bs->cond);
    break;
  default:
    break;
  }
  g_mutex_unlock(&bs->mutex);
}

static gpointer
seek_thread(gpointer data)
{
  BlockedSeek *bs = data;
  enum BlockedSeekState current_state = IDLE;
  gboolean running = TRUE;
  while(running) {
    g_mutex_lock(&bs->mutex);
    while(current_state == bs->state) {
      g_cond_wait(&bs->cond, &bs->mutex);
    }
    g_mutex_unlock(&bs->mutex);
    g_debug("State %d -> %d", current_state, bs->state);
    current_state = bs->state;
    switch(current_state) {
    case SEEK_PREROLL:
      {
	gst_pad_send_event(bs->exit_pad, bs->seek_event);
	bs->seek_event = NULL;
      }
      break;
    case PLAYING:
      gst_pad_set_blocked_async(bs->exit_pad, FALSE, seek_done , bs);
      break;
    case EXIT:
      running = FALSE;
    break;
    default:
      break;
    }
  }
  return NULL;
}

BlockedSeek *
blocked_seek_new(void)
{
  BlockedSeek *bs = g_new(BlockedSeek, 1);
  g_mutex_init(&bs->mutex);
  g_cond_init (&bs->cond);
  bs->state = IDLE;
  bs->seek_event = NULL;
  bs->element = NULL;
  bs->exit_pad = NULL;
  bs->thread = g_thread_new("Blocked seek", seek_thread, bs);
  return bs;
}

void
blocked_seek_destroy(BlockedSeek *bs)
{
  g_mutex_lock(&bs->mutex);
  bs->state = EXIT;
  g_cond_signal(&bs->cond);
  g_mutex_unlock(&bs->mutex);
  g_thread_join(bs->thread);
  g_clear_object(&bs->exit_pad);
  g_clear_object(&bs->element);
  g_cond_clear(&bs->cond);
  g_mutex_clear(&bs->mutex);
  g_free(bs);
}

gboolean
blocked_seek_start(BlockedSeek *bs, GstElement *element, GstPad *pad,
		   GstSeekFlags seek_flags,
		   GstSeekType start_type, GstClockTime start,
		   GstSeekType stop_type, GstClockTime stop)
{
  g_mutex_lock(&bs->mutex);
  g_clear_object(&bs->exit_pad);
  g_clear_object(&bs->element);
  bs->exit_pad = pad;
  g_object_ref(pad);
  bs->element = element;
  g_object_ref(element);
    
  if (bs->seek_event) {
    gst_event_unref(bs->seek_event);
  }
  bs->seek_event = gst_event_new_seek(1.0,
				      GST_FORMAT_TIME,
				      seek_flags,
				      start_type, start,
				      stop_type, stop);
  bs->state = INITIAL_PREROLL;
  gst_pad_set_blocked_async(bs->exit_pad, TRUE, preroll_done , bs);
  g_mutex_unlock(&bs->mutex);
  return TRUE;
}
