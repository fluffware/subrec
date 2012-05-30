#ifndef __BLOCKED_SEEK_H__RXPV53T5BE__
#define __BLOCKED_SEEK_H__RXPV53T5BE__
#include <gst/gst.h>

typedef struct _BlockedSeek BlockedSeek;

BlockedSeek *
blocked_seek_new(void);
void
blocked_seek_destroy(BlockedSeek *bs);

gboolean
blocked_seek_start(BlockedSeek *bs, GstElement *element, GstPad *pad,
		   GstFormat format,
		   GstSeekFlags seek_flags,
		   GstSeekType start_type, GstClockTime start,
		   GstSeekType stop_type, GstClockTime stop);
#endif /* __BLOCKED_SEEK_H__RXPV53T5BE__ */
