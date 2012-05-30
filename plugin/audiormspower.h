G_BEGIN_DECLS
#include <gst/base/gstbasetransform.h>

#define GST_TYPE_AUDIO_RMS_POWER \
  (audio_rms_power_get_type())
#define AUDIO_RMS_POWER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AUDIO_RMS_POWER,AudioRmsPower))
#define AUDIO_RMS_POWER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AUDIO_RMS_POWER,AudioRmsPowerClass))
#define IS_AUDIO_RMS_POWER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AUDIO_RMS_POWER))
#define IS_AUDIO_RMS_POWER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AUDIO_RMS_POWER))

typedef struct _AudioRmsPower      AudioRmsPower;
typedef struct _AudioRmsPowerClass AudioRmsPowerClass;

typedef float Sample;

struct _AudioRmsPower
{
  GstBaseTransform base;
  gint sample_rate;
  gint64 sub_block_length;
  guint block_length;
  guint block_overlap;
  gboolean sub_block_message;
  gboolean analysis_message;
  gfloat trim_level; /* Power level used for finding leading and
			trailing silence */

  /* Generate new timestamps and offsets, changing incoming buffers.
   */
  gboolean regenerate_timestamps;
  gint64 generated_offset;
  
  guint sub_block_sample_count; /* Total number of samples in a sub block */
  guint sub_block_samples_left; /* Number of samples left before the sub block
				   is done */
  gfloat square_acc; /* Sum of squared samples */

  gfloat prefilter_x[4];
  gfloat prefilter_y[4];

  guint power_buffer_max_len; /* Max length for power buffers */
  GstBuffer *current_power_buffer;
  GList *power_buffers; /* List of GstBuffer containing power values
			    for all sub blocks */
};

struct _AudioRmsPowerClass 
{
  GstBaseTransformClass parent_class;
};

#define AUDIO_RMS_POWER_SUB_BLOCK_MESSAGE "sub-block-message"
#define AUDIO_RMS_POWER_SUB_BLOCK_MESSAGE_POWER "power"

#define AUDIO_RMS_POWER_ANALYSIS_MESSAGE "analysis-message"
#define AUDIO_RMS_POWER_ANALYSIS_MESSAGE_LOUDNESS "loudness"
#define AUDIO_RMS_POWER_ANALYSIS_MESSAGE_TRIM_START "trim-start"
#define AUDIO_RMS_POWER_ANALYSIS_MESSAGE_TRIM_END "trim-end"

GType audio_rms_power_get_type (void);

gboolean
audio_rms_power_plugin_init (GstPlugin *plugin);

G_END_DECLS

