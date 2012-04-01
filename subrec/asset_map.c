#include "asset_map.h"
#include <xml_tree_parser.h>
#include <string.h>

#define NAMESPACE_AM "http://www.digicine.com/PROTO-ASDCP-AM-20040311#"

GQuark
asset_map_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("asset-map-error-quark");
  return error_quark;
}

G_DEFINE_TYPE (AssetMap, asset_map, G_TYPE_OBJECT)

enum {
  PROP_0 = 0,
  PROP_ID,
  PROP_PKL
};

static void
asset_map_finalize(GObject *object)
{
  AssetMap *map = ASSET_MAP(object);
  g_tree_unref(map->map);
  g_free(map->id);
  if (map->map_file) g_object_unref(map->map_file);
  G_OBJECT_CLASS (asset_map_parent_class)->finalize (object);
}

static void
asset_map_get_property(GObject    *object,
		       guint       property_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
  AssetMap *map = ASSET_MAP(object);
  switch(property_id) {
  case PROP_PKL:
    {
      GFile *file = NULL;
      if (map->packing_list) file = map->packing_list->file;
      g_value_set_object(value, file);
    }
    break;
  case PROP_ID:
    g_value_set_string (value, map->id);
    break;
  }
}

static void
asset_map_class_init(AssetMapClass *g_class)
{
  GParamSpec *pspec;
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  gobject_class->finalize = asset_map_finalize;
  gobject_class->get_property = asset_map_get_property;

  pspec = g_param_spec_string ("id",
                               "AssetMap Id",
                               "Get ID of this map",
                               "" /* default value */,
                               G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_ID, pspec);

   pspec = g_param_spec_object ("packing-list",
                               "Packing list",
                               "Get packing list file",
				G_TYPE_FILE,
				G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_PKL, pspec);

  
}

static gint
asset_cmp(gconstpointer a, gconstpointer b, gpointer data)
{
  return g_strcmp0((const gchar*)a, (const gchar *)b);
}

static void
destroy_asset(AssetMapAsset *asset)
{
  if (asset->file) g_object_unref(asset->file);
  g_free(asset->id);
  g_free(asset);
}

static void
asset_map_init(AssetMap *instance)
{
  instance->map = g_tree_new_full(asset_cmp, NULL,
				  NULL,
				  (GDestroyNotify)destroy_asset);
  instance->id = NULL;
  instance->map_file = NULL;
  instance->packing_list = NULL;
}



struct ParseCtxt
{
  AssetMap *asset_map;
  GFile *base;
  GTree *map;
  AssetMapAsset *current_asset;
  gboolean is_packing_list;
};

typedef struct ParseCtxt ParseCtxt;

static void
init_parse_ctxt(ParseCtxt *ctxt, GFile *base, AssetMap *asset_map)
{
  ctxt->base = base;
  g_object_ref(base);
  ctxt->asset_map = asset_map;
  ctxt->map = asset_map->map;
  ctxt->current_asset = NULL;
  ctxt->is_packing_list = FALSE;
}

static void
clear_parse_ctxt(ParseCtxt *ctxt)
{
  if (ctxt->base) g_object_unref(ctxt->base);
  ctxt->base = NULL;
  if (ctxt->current_asset) destroy_asset(ctxt->current_asset);
  ctxt->current_asset = NULL;
}

/* Path */

static gboolean
path_end(xmlTextReaderPtr reader, gpointer user_data,
	       gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  g_debug("Path: %s",text);
  ctxt->current_asset->file =
    g_file_resolve_relative_path (ctxt->base, text);
  g_free(text);
  return TRUE;
}

static const XMLTreeParserElement path_elem =
  {
    NAMESPACE_AM,
    "Path",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    path_end
  };

/* Chunk */
static const XMLTreeParserElement *chunk_children[] = {&path_elem, NULL};

static const XMLTreeParserElement chunk_elem =
  {
    NAMESPACE_AM,
    "Chunk",
    chunk_children,
    0,
    NULL,
    NULL
  };

/* ChunkList */
static const XMLTreeParserElement *chunklist_children[] = {&chunk_elem, NULL};

static const XMLTreeParserElement chunklist_elem =
  {
    NAMESPACE_AM,
    "ChunkList",
    chunklist_children,
    0,
    NULL,
    NULL
  };

/* PackingList */
static gboolean
packing_list_end(xmlTextReaderPtr reader, gpointer user_data,
		 gchar *text, GError **error)
{
  ((ParseCtxt*)user_data)->is_packing_list = TRUE;
  return TRUE;
}

static const XMLTreeParserElement packing_list_elem =
  {
    NAMESPACE_AM,
    "PackingList",
    NULL,
    0,
    NULL,
    packing_list_end
  };

/* Asset/Id */
static gboolean
asset_id_end(xmlTextReaderPtr reader, gpointer user_data,
	     gchar *text, GError **error)
{
  g_debug("Asset/Id: %s",text);
  ((ParseCtxt*)user_data)->current_asset->id = text;
  return TRUE;
}

static const XMLTreeParserElement asset_id_elem =
  {
    NAMESPACE_AM,
    "Id",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_id_end,
  };

/* Asset */
static gboolean
asset_start(xmlTextReaderPtr reader, gpointer user_data,
	    GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->current_asset = g_new(AssetMapAsset, 1);
  ctxt->current_asset->id = NULL;
  ctxt->current_asset->file = NULL;
  ctxt->is_packing_list = FALSE;
  return TRUE;
}

static gboolean
asset_end(xmlTextReaderPtr reader, gpointer user_data,
	    gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  if (!ctxt->current_asset->id) {
    g_set_error(error, ASSET_MAP_ERROR, ASSET_MAP_ERROR_XML_STRUCTURE,
		"No 'Id' element in asset");
    return FALSE;
  }
  if (!ctxt->current_asset->file) {
    g_set_error(error, ASSET_MAP_ERROR, ASSET_MAP_ERROR_XML_STRUCTURE,
		"No path for asset");
    return FALSE;
  }
  g_tree_insert(ctxt->map, ctxt->current_asset->id, ctxt->current_asset);
  if (ctxt->is_packing_list) {
    ctxt->asset_map->packing_list = ctxt->current_asset;
    ctxt->is_packing_list = FALSE;
  }
  ctxt->current_asset = NULL;
  return TRUE;
}

static const XMLTreeParserElement *asset_children[] =
  {&chunklist_elem, &asset_id_elem, &packing_list_elem, NULL};

static const XMLTreeParserElement asset_elem =
  {
    NAMESPACE_AM,
    "Asset",
    asset_children,
    0,
    asset_start,
    asset_end
  };

/* AssetList */
static gboolean
asset_list_start(xmlTextReaderPtr reader, gpointer user_data,
		 GError **error)
{
  g_debug("<AssetList>");
  return TRUE;
}

static gboolean
asset_list_end(xmlTextReaderPtr reader, gpointer user_data,
	       gchar *text, GError **error)
{
  g_debug("</AssetList>");
  return TRUE;
}


static const XMLTreeParserElement *asset_list_children[] = {&asset_elem, NULL};

static const XMLTreeParserElement asset_list_elem =
  {
    NAMESPACE_AM,
    "AssetList",
    asset_list_children,
    0,
    asset_list_start,
    asset_list_end
  };

/* AssetMap/Id */
static gboolean
asset_map_id_end(xmlTextReaderPtr reader, gpointer user_data,
	       gchar *text, GError **error)
{
  g_debug("AssetMap/Id: %s",text);
  ((ParseCtxt*)user_data)->asset_map->id = text;
  return TRUE;
}

static const XMLTreeParserElement asset_map_id_elem =
  {
    NAMESPACE_AM,
    "Id",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_map_id_end
  };

/* AssetMap */
static const XMLTreeParserElement *asset_map_children[] = {&asset_list_elem, &asset_map_id_elem, NULL};

static const XMLTreeParserElement asset_map_elem =
  {
    NAMESPACE_AM,
    "AssetMap",
    asset_map_children,
    0,
    NULL,
    NULL
  };

static const XMLTreeParserElement *top_elements[] = {&asset_map_elem, NULL};


static gboolean
read_asset_map(AssetMap *map, GFile *file, GError **error)
{
  
  gboolean ret;
  GFile *base = g_file_get_parent(file);
  ParseCtxt ctxt;
  init_parse_ctxt(&ctxt, base, map);
  g_object_unref(base);
  ret = xml_tree_parser_parse_file(file, top_elements, &ctxt, error);
  clear_parse_ctxt(&ctxt);
  if (!ret) {
    return FALSE;
  }
  return TRUE;
}

AssetMap *
asset_map_read(GFile *map_file, GError **error)
{
  AssetMap *map = g_object_new (ASSET_MAP_TYPE, NULL);
  if (!read_asset_map(map, map_file, error)) {
    g_object_unref(map);
    return NULL;
  }
  return map;
}

GFile *
asset_map_get_file(AssetMap *map, const gchar *id)
{
  AssetMapAsset *asset = g_tree_lookup(map->map, id);
  if (!asset || !asset->file) return NULL;
  g_object_ref(asset->file);
  return asset->file;
}

