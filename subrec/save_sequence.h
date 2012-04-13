#ifndef __SAVE_SEQUENCE_H__VM70XRUTTB__
#define __SAVE_SEQUENCE_H__VM70XRUTTB__


#include <subtitle_store.h>
#include <gst/gst.h>

gboolean
save_sequence(SubtitleStore *subtitles,
	      GstClockTime start, GstClockTime end,
	      GError **err);

#endif /* __SAVE_SEQUENCE_H__VM70XRUTTB__ */
