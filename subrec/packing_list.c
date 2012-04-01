#include "packing_list.h"
#include <xml_tree_parser.h>

#define NAMESPACE_PKL "http://www.digicine.com/PROTO-ASDCP-PKL-20040311#"

GQuark
packing_list_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("packing-list-error-quark");
  return error_quark;
}
G_DEFINE_TYPE (PackingList, packing_list, G_TYPE_OBJECT)

static void
packing_list_finalize(GObject *object)
{
  PackingList *list = PACKING_LIST(object);
  g_tree_unref(list->assets);
  g_free(list->id);
  G_OBJECT_CLASS (packing_list_parent_class)->finalize (object);
}

static void
packing_list_class_init(PackingListClass *g_class)
{
  G_OBJECT_CLASS(g_class)->finalize = packing_list_finalize;
}

static gint
asset_cmp(gconstpointer a, gconstpointer b, gpointer data)
{
  return g_strcmp0(((const PackingListAsset *)a)->id,
		   ((const PackingListAsset *)b)->id);
}

static void
destroy_asset(PackingListAsset *asset)
{
  g_free(asset->id);
  g_free(asset->hash);
  g_free(asset->type);
  g_free(asset->annotation);
  g_free(asset);
}

static void
packing_list_init(PackingList *instance)
{
  instance->assets = g_tree_new_full(asset_cmp, NULL, NULL,
				   (GDestroyNotify)destroy_asset);
  instance->id = NULL;
}


typedef struct ParseCtxt {
  PackingList *list;
  PackingListAsset *current_asset;
} ParseCtxt;

static void
init_parse_ctxt(ParseCtxt *ctxt, PackingList *list)
{
  ctxt->list = list;
  ctxt->current_asset = NULL;
  
}

static void
clear_parse_ctxt(ParseCtxt *ctxt)
{
  if (ctxt->current_asset) destroy_asset(ctxt->current_asset);
}
/* Asset/AnnotationText */

static gboolean
asset_annotation_end(xmlTextReaderPtr reader, gpointer user_data,
	     gchar *text, GError **error)
{
  g_debug("Asset/Annotation: %s",text);
  ((ParseCtxt*)user_data)->current_asset->annotation = text;
  return TRUE;
}

static const XMLTreeParserElement asset_annotation_elem =
  {
    NAMESPACE_PKL,
    "AnnotationText",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_annotation_end,
  };

/* Asset/Type */
static gboolean
asset_type_end(xmlTextReaderPtr reader, gpointer user_data,
	     gchar *text, GError **error)
{
  g_debug("Asset/Type: %s",text);
  ((ParseCtxt*)user_data)->current_asset->type = text;
  return TRUE;
}

static const XMLTreeParserElement asset_type_elem =
  {
    NAMESPACE_PKL,
    "Type",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_type_end,
  };

/* Asset/Hash */
static gboolean
asset_hash_end(xmlTextReaderPtr reader, gpointer user_data,
	     gchar *text, GError **error)
{
  g_debug("Asset/Hash: %s",text);
  ((ParseCtxt*)user_data)->current_asset->hash = text;
  return TRUE;
}

static const XMLTreeParserElement asset_hash_elem =
  {
    NAMESPACE_PKL,
    "Hash",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_hash_end,
  };

/* Asset/Size */
static gboolean
asset_size_end(xmlTextReaderPtr reader, gpointer user_data,
	     gchar *text, GError **error)
{
  gchar *end;
  gsize size = g_ascii_strtoull(text, &end, 10);
  g_free(text);
  if (text == end) {
    g_set_error(error, PACKING_LIST_ERROR, PACKING_LIST_ERROR_VALUE,
		"Invalid size for asset");
    return FALSE;
  }
  ((ParseCtxt*)user_data)->current_asset->size = size;
  return TRUE;
}

static const XMLTreeParserElement asset_size_elem =
  {
    NAMESPACE_PKL,
    "Size",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_size_end,
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
    NAMESPACE_PKL,
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
  ctxt->current_asset = g_new(PackingListAsset, 1);
  ctxt->current_asset->id = NULL;
  ctxt->current_asset->hash = NULL;
  ctxt->current_asset->type = NULL;
  ctxt->current_asset->annotation = NULL;
  ctxt->current_asset->size = 0;
  return TRUE;
}

static gboolean
asset_end(xmlTextReaderPtr reader, gpointer user_data,
	    gchar *text, GError **error)
{
   ParseCtxt *ctxt = user_data;
  if (!ctxt->current_asset->id) {
    g_set_error(error, PACKING_LIST_ERROR, PACKING_LIST_ERROR_XML_STRUCTURE,
		"No 'Id' element in asset");
    return FALSE;
  }
  if (!ctxt->current_asset->type) {
    g_set_error(error, PACKING_LIST_ERROR, PACKING_LIST_ERROR_XML_STRUCTURE,
		"No type for asset");
    return FALSE;
  }
  g_tree_insert(ctxt->list->assets, ctxt->current_asset, ctxt->current_asset);
  ctxt->current_asset = NULL;
  return TRUE;
}

static const XMLTreeParserElement *asset_children[] =
  {&asset_id_elem, &asset_size_elem, &asset_hash_elem,
   &asset_type_elem, &asset_annotation_elem, NULL};

static const XMLTreeParserElement asset_elem =
  {
    NAMESPACE_PKL,
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
  return TRUE;
}

static gboolean
asset_list_end(xmlTextReaderPtr reader, gpointer user_data,
	       gchar *text, GError **error)
{
  return TRUE;
}


static const XMLTreeParserElement *asset_list_children[] = {&asset_elem, NULL};

static const XMLTreeParserElement asset_list_elem =
  {
    NAMESPACE_PKL,
    "AssetList",
    asset_list_children,
    0,
    asset_list_start,
    asset_list_end
  };

/* PackingList/Id */
static gboolean
packing_list_id_end(xmlTextReaderPtr reader, gpointer user_data,
	       gchar *text, GError **error)
{
  g_debug("PackingList/Id: %s",text);
  ((ParseCtxt*)user_data)->list->id = text;
  return TRUE;
}

static const XMLTreeParserElement packing_list_id_elem =
  {
    NAMESPACE_PKL,
    "Id",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    packing_list_id_end
  };

/* PackingList */
static const XMLTreeParserElement *packing_list_children[] = {&asset_list_elem, &packing_list_id_elem, NULL};

static const XMLTreeParserElement packing_list_elem =
  {
    NAMESPACE_PKL,
    "PackingList",
    packing_list_children,
    0,
    NULL,
    NULL
  };

static const XMLTreeParserElement *top_elements[] = {&packing_list_elem, NULL};

PackingList *
packing_list_read(GFile *file, GError **error)
{
  gboolean ret;
  ParseCtxt ctxt;
  PackingList *list = g_object_new (PACKING_LIST_TYPE, NULL);
  init_parse_ctxt(&ctxt, list);
  ret = xml_tree_parser_parse_file(file, top_elements, &ctxt, error);
  clear_parse_ctxt(&ctxt);
  if (!ret) {
    g_object_unref(list);
    return NULL;
  }
  return list;
}

struct TypeTraverseCtxt
{
  GPtrArray *array;
  const gchar *type;
};

static gboolean
type_traverse(gpointer key, gpointer value, gpointer data)
{
  struct TypeTraverseCtxt *ctxt = data;
  if (g_strcmp0(ctxt->type, ((PackingListAsset*)value)->type) == 0)
    g_ptr_array_add(ctxt->array, value);
  return FALSE;
}
  
const PackingListAsset **
packing_list_find_asset_with_type(PackingList *list, const gchar *type)
{
  struct TypeTraverseCtxt ctxt;
  ctxt.array = g_ptr_array_new();
  ctxt.type = type;
  g_tree_foreach(list->assets, type_traverse, &ctxt);
  g_ptr_array_add(ctxt.array, NULL);
  return (const PackingListAsset**)g_ptr_array_free(ctxt.array, FALSE);
  
}

