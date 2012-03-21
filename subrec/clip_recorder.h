#ifndef __CLIP_RECORDER_H__NQKIV1K2IA__
#define __CLIP_RECORDER_H__NQKIV1K2IA__
#include <glib-object.h>
#include <gst/gst.h>
#include <gio/gio.h>

#define CLIP_RECORDER_ERROR (clip_recorder_error_quark())
enum {
  CLIP_RECORDER_ERROR_FAILED = 1,
  CLIP_RECORDER_ERROR_CREATE_ELEMENT_FAILED,
  CLIP_RECORDER_ERROR_LINK_FAILED,
  CLIP_RECORDER_ERROR_STATE,
};

#define CLIP_RECORDER_TYPE                  (clip_recorder_get_type ())
#define CLIP_RECORDER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLIP_RECORDER_TYPE, ClipRecorder))
#define IS_CLIP_RECORDER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLIP_RECORDER_TYPE))
#define CLIP_RECORDER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), CLIP_RECORDER_TYPE, ClipRecorderClass))
#define IS_CLIP_RECORDER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), CLIP_RECORDER_TYPE))
#define CLIP_RECORDER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), CLIP_RECORDER_TYPE, ClipRecorderClass))

typedef struct _ClipRecorder        ClipRecorder;
typedef struct _ClipRecorderClass   ClipRecorderClass;

struct _ClipRecorder
{
  GObject parent_instance;

  /* instance members */
  GstPipeline *record_pipeline;
  GstPipeline *adjust_pipeline;
  GstPipeline *playback_pipeline;
  GstPipeline *active_pipeline;

  gdouble trim_level;

  /* Analysis results */
  GstClockTime trim_start;
  GstClockTime trim_end;
  gdouble loudness;
};

struct _ClipRecorderClass
{
  GObjectClass parent_class;

  /* class members */

  /* Signals */
  void (*run_error)(ClipRecorder *recorder, GError *error, gpointer user_data);
  void (*recording)(ClipRecorder *recorder, gpointer user_data);
  void (*playing)(ClipRecorder *recorder, gpointer user_data);
  void (*stopped)(ClipRecorder *recorder, gpointer user_data);
  void (*power)(ClipRecorder *recorder, gdouble power);
};

ClipRecorder *clip_recorder_new(void);

gboolean clip_recorder_record(ClipRecorder *recorder, GFile *file,GError **err);
gboolean clip_recorder_play(ClipRecorder *recorder, GFile *file, GError **err);
gboolean clip_recorder_stop(ClipRecorder *recorder, GError **err);

GstClockTimeDiff clip_recorder_recorded_length(ClipRecorder *recorder);

#endif /* __CLIP_RECORDER_H__NQKIV1K2IA__ */
