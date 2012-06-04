#ifndef __SAVE_SEQUENCE_H__VM70XRUTTB__
#define __SAVE_SEQUENCE_H__VM70XRUTTB__


#include <subtitle_store.h>
#include <gst/gst.h>
#include <blocked_seek.h>

#define SAVE_SEQUENCE_ERROR (save_sequence_error_quark())
enum {
  SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED = 1,
  SAVE_SEQUENCE_ERROR_LINK_FAILED,
  SAVE_SEQUENCE_ERROR_FILE_NOT_FOUND,
  SAVE_SEQUENCE_ERROR_PIPELINE,
  SAVE_SEQUENCE_ERROR_INVALID_SEQUENCE
};

/*
 * Type macros.
 */
#define SAVE_SEQUENCE_TYPE (save_sequence_get_type ())
#define SAVE_SEQUENCE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SAVE_SEQUENCE_TYPE, SaveSequence))
#define IS_SAVE_SEQUENCE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SAVE_SEQUENCE_TYPE))
#define SAVE_SEQUENCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SAVE_SEQUENCE_TYPE, SaveSequenceClass)) 
#define IS_SAVE_SEQUENCE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SAVE_SEQUENCE_TYPE))
#define SAVE_SEQUENCE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SAVE_SEQUENCE_TYPE, SaveSequenceClass))

typedef struct _SaveSequence        SaveSequence;
typedef struct _SaveSequenceClass   SaveSequenceClass;
typedef struct _SaveSequenceReel   SaveSequenceReel;
typedef struct _SaveSequenceAsset   SaveSequenceAsset;
typedef struct _SaveSequenceRatio   SaveSequenceRatio;

struct _SaveSequence
{
  GObject parent_instance;
  
  /* instance members */
  GFile *working_directory;
  SubtitleStore *subtitle_store;
  GstElement *pipeline;

  GstElement *file_src_bin;
  GstElement *file_src;
  GstElement *silence_src;
  GstElement *active_src;
  GstElement *output_element;
  BlockedSeek *blocked_seek;

  GtkTreeIter next_pos;
  guint64 next_sample; /* Sample offset */
  guint64 start_sample;
  guint64 end_sample;
  guint64 current_sample;
  gboolean last_pos;
  guint depth;
};

struct _SaveSequenceClass
{
  GObjectClass parent_class;

  /* class members */
  /* Signals */
  void (*run_error)(SaveSequence *sseq, GError *error, gpointer user_data);
  void (*done)(SaveSequence *sseq, gpointer user_data);
};

/* used by SAVE_SEQUENCE_TYPE */
GType save_sequence_get_type (void);

/*
 * Method definitions.
 */


/*
 * Create a new list from file
 */
SaveSequence *
save_sequence_new(GError **error);


gboolean
save_sequence(SaveSequence *sseq, GFile *save_file,
	      SubtitleStore *subtitles, GFile *working_directory,
	      GstClockTime start, GstClockTime end,
	      GError **err);

gdouble
save_sequence_progress(SaveSequence *sseq);

#endif /* __SAVE_SEQUENCE_H__VM70XRUTTB__ */
