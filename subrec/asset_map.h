#ifndef __ASSET_MAP_H__VDT97KBNF1__
#define __ASSET_MAP_H__VDT97KBNF1__

#include <glib-object.h>
#include <gio/gio.h>

#define ASSET_MAP_ERROR (asset_map_error_quark())
enum {
  ASSET_MAP_ERROR_CREATE_READER_FAILED = 1,
  ASSET_MAP_ERROR_PARSER,
  ASSET_MAP_ERROR_XML_STRUCTURE,
  ASSET_MAP_ERROR_EMPTY_ELEMENT,
  ASSET_MAP_ERROR_NO_ASSET_LIST
};

/*
 * Type macros.
 */
#define ASSET_MAP_TYPE                  (asset_map_get_type ())
#define ASSET_MAP(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), ASSET_MAP_TYPE, AssetMap))
#define IS_ASSET_MAP(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ASSET_MAP_TYPE))
#define ASSET_MAP_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), ASSET_MAP_TYPE, AssetMapClass))
#define IS_ASSET_MAP_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), ASSET_MAP_TYPE))
#define ASSET_MAP_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), ASSET_MAP_TYPE, AssetMapClass))

typedef struct _AssetMap        AssetMap;
typedef struct _AssetMapClass   AssetMapClass;
typedef struct _AssetMapAsset   AssetMapAsset;

struct _AssetMapAsset
{
  gchar *id;
  GFile *file;
};
  
struct _AssetMap
{
  GObject parent_instance;
  
  /* instance members */
  GFile *map_file;
  GTree *map;
  gchar *id;
  AssetMapAsset *packing_list;
};

struct _AssetMapClass
{
  GObjectClass parent_class;

  /* class members */
};

/* used by ASSET_MAP_TYPE */
GType asset_map_get_type (void);

/*
 * Method definitions.
 */


/*
 * Create a new map from file
 */
AssetMap *
asset_map_read(GFile *map_file, GError **error);

GFile *
asset_map_get_file(AssetMap *map, const gchar *id);

#endif /* __ASSET_MAP_H__VDT97KBNF1__ */
