G_BEGIN_DECLS

#define GST_TYPE_AUDIO_TRIM \
  (audio_trim_get_type())
#define AUDIO_TRIM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIO_TRIM,AudioTrim))
#define AUDIO_TRIM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIO_TRIM,AudioTrimClass))
#define IS_AUDIO_TRIM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIO_TRIM))
#define IS_AUDIO_TRIM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUDIO_TRIM))

typedef struct _AudioTrim      AudioTrim;
typedef struct _AudioTrimClass AudioTrimClass;

typedef float Sample;



enum AudioTrimState {
  /* No buffers has been received yet */
  AUDIO_TRIM_NOT_STARTED = 0,
  
  /* Skipping samples for start_skip ns */
  AUDIO_TRIM_START_SKIP,
  
  /* Silence at beginning of clip */
  AUDIO_TRIM_START_SILENCE,
  
  /* Sound to save */
  AUDIO_TRIM_NOT_SILENCE,

/* Silence. Save buffers in case of more sound. */
  AUDIO_TRIM_SILENCE
};

struct _AudioTrim
{
  GstElement element;
  GstPad *sinkpad, *srcpad;
  GList *buffers;
  /* Stream properties */
  gint sample_rate;
  /* Properties */
  guint start_threshold;
  guint end_threshold;

  /* Ignore at beginning of clip. */
  GstClockTime start_skip;
  /* Ignore at end of clip */
  GstClockTime end_skip;

  /* Silent samples before trimmed clip. */
  GstClockTime pre_silence;
  /* Silence after trimmed clip. */
  GstClockTime post_silence;

  /* Don't save silence for more than this. */
  GstClockTime max_silence_duration;

  /* Analysis state */
  gfloat accumulator;
  gfloat f0;
  gint trim_state;

  /* Currently buffered duration. */
  GstClockTime buffered;


  /* A reference time depending on the state. In samples
     
     START_SKIP:
     Transition to START_SILENCE at this time.
     
     START_SILENCE:
     Save buffers after this time.
     
     SILENCE:
     Beginning of silence.
  */
  guint64 ref_time;

  
};

struct _AudioTrimClass 
{
  GstElementClass parent_class;
};

GType audio_trim_get_type (void);

G_END_DECLS

