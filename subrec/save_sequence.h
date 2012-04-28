#ifndef __SAVE_SEQUENCE_H__VM70XRUTTB__
#define __SAVE_SEQUENCE_H__VM70XRUTTB__


#include <subtitle_store.h>
#include <gst/gst.h>

#define SAVE_SEQUENCE_ERROR (save_sequence_error_quark())
enum {
  SAVE_SEQUENCE_ERROR_CREATE_ELEMENT_FAILED = 1,
  SAVE_SEQUENCE_ERROR_LINK_FAILED,
  SAVE_SEQUENCE_ERROR_FILE_NOT_FOUND,
  SAVE_SEQUENCE_ERROR_PIPELINE,
};
gboolean
save_sequence(SubtitleStore *subtitles, GFile *working_directory,
	      GstClockTime start, GstClockTime end,
	      GError **err);

#endif /* __SAVE_SEQUENCE_H__VM70XRUTTB__ */
