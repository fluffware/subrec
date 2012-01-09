#ifndef __READ_ASSET_MAP_H__AO5EA9AHF9__
#define __READ_ASSET_MAP_H__AO5EA9AHF9__

#include <gio/gio.h>

#define ASSET_MAP_ERROR (asset_map_error_quark())
enum {
  ASSET_MAP_ERROR_CREATE_READER_FAILED = 1,
  ASSET_MAP_ERROR_PARSER,
  ASSET_MAP_ERROR_WRONG_TOP_ELEMENT,
  ASSET_MAP_ERROR_EMPTY_ELEMENT,
  ASSET_MAP_ERROR_NO_ASSET_LIST
};
GQuark
asset_map_error_quark();

gboolean
read_asset_map(GFile *file, GError **error);

#endif /* __READ_ASSET_MAP_H__AO5EA9AHF9__ */
