#ifndef __SUBTITLE_STORE_IO_H__IZZADYMJIF__
#define __SUBTITLE_STORE_IO_H__IZZADYMJIF__
#include <subtitle_store.h>
#include <gio/gio.h>
#define SUBTITLE_STORE_IO_ERROR subtitle_store_io_error_quark()
enum {
  SUBTITLE_STORE_IO_ERROR_FAILED = 1,
  SUBTITLE_STORE_IO_ERROR_XML_STRUCTURE,
  SUBTITLE_STORE_IO_ERROR_VALUE,
  SUBTITLE_STORE_IO_ERROR_READER,
};
gboolean
subtitle_store_io_save(SubtitleStore *store, GFile *file, GError **error);

gboolean
subtitle_store_io_load(SubtitleStore *store, GFile *file, GError **error);
 
#endif /* __SUBTITLE_STORE_IO_H__IZZADYMJIF__ */
