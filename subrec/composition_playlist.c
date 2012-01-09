#include "composition_playlist.h"
#include <xml_tree_parser.h>
#include <string.h>

#define NAMESPACE_CPL "http://www.digicine.com/PROTO-ASDCP-CPL-20040511#"

GQuark
composition_playlist_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark =
      g_quark_from_static_string ("composition-playlist-error-quark");
  return error_quark;
}

G_DEFINE_TYPE (CompositionPlaylist, composition_playlist, G_TYPE_OBJECT)

enum {
  PROP_0 = 0,
  PROP_ID,
};

static void
destroy_asset(CompositionPlaylistAsset *asset)
{
  g_free(asset->id);
  g_free(asset->annotation);
  g_free(asset);
}

static void
destroy_reel(CompositionPlaylistReel *reel)
{
  g_free(reel->id);
  g_free(reel->annotation);
  g_list_free_full(reel->assets, (GDestroyNotify)destroy_asset);
  g_free(reel);
}
static void
composition_playlist_finalize(GObject *object)
{
  CompositionPlaylist *cpl = COMPOSITION_PLAYLIST(object);
  g_list_free_full(cpl->reels, (GDestroyNotify)destroy_reel);
  g_free(cpl->id);
}

static void
composition_playlist_get_property(GObject    *object,
				  guint       property_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  CompositionPlaylist *cpl = COMPOSITION_PLAYLIST(object);
  switch(property_id) {
  case PROP_ID:
    g_value_set_string (value, cpl->id);
    break;
  }
}

static void
composition_playlist_class_init(CompositionPlaylistClass *g_class)
{
  GParamSpec *pspec;
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  gobject_class->finalize = composition_playlist_finalize;
  gobject_class->get_property = composition_playlist_get_property;

  pspec = g_param_spec_string ("id",
                               "CompositionPlaylist Id",
                               "Get ID of this playlist",
                               "" /* default value */,
                               G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_ID, pspec);


}

static void
composition_playlist_init(CompositionPlaylist *instance)
{
  instance->reels = NULL;
  instance->id = NULL;
}


typedef struct ParseCtxt {
  CompositionPlaylist *cpl;
  
  CompositionPlaylistReel *current_reel;
  CompositionPlaylistAsset *current_asset;
} ParseCtxt;

static void
init_parse_ctxt(ParseCtxt *ctxt, CompositionPlaylist *cpl)
{
  ctxt->cpl = cpl;
  ctxt->current_asset = NULL;
  ctxt->current_reel = NULL;
  
}

static void
clear_parse_ctxt(ParseCtxt *ctxt)
{
  if (ctxt->current_asset) destroy_asset(ctxt->current_asset);
  if (ctxt->current_asset) destroy_reel(ctxt->current_reel);
}

/* Common asset handling */

static const CompositionPlaylistRatio rate_24 = {24,1};

static gboolean
asset_common_start(xmlTextReaderPtr reader, gpointer user_data, int asset_type, GError **error)
{
  ParseCtxt *ctxt = user_data;
  CompositionPlaylistAsset *asset;
  g_assert(ctxt->current_asset == NULL);
  asset = g_new(CompositionPlaylistAsset, 1);
  ctxt->current_asset = asset;
  asset->type = asset_type;
  asset->id = NULL;
  asset->intrinsic_duration = 0;
  asset->duration = 0;
  asset->edit_rate = rate_24;
  asset->entry_point = 0;
  asset->annotation = NULL;
  return TRUE;
}

static gboolean
asset_common_end(xmlTextReaderPtr reader, gpointer user_data,
	       gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  g_assert(ctxt->current_reel != NULL);
  g_assert(ctxt->current_asset);
  ctxt->current_reel->assets =
    g_list_append(ctxt->current_reel->assets, ctxt->current_asset);
  ctxt->current_asset = NULL;
  return TRUE;
}
/* Asset/Id */

static gboolean
asset_id_end(xmlTextReaderPtr reader, gpointer user_data,
	     gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->current_asset->id = text;
  return TRUE;
}

static const XMLTreeParserElement asset_id_elem =
  {
    NAMESPACE_CPL,
    "Id",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_id_end
  };

/* Asset/IntrinsicDuration */

static gboolean
asset_intrinsic_duration_end(xmlTextReaderPtr reader, gpointer user_data,
			     gchar *text, GError **error)
{
  gchar *end;
  gsize size = g_ascii_strtoull(text, &end, 10);
  g_free(text);
  if (text == end) {
    g_set_error(error, COMPOSITION_PLAYLIST_ERROR,
		COMPOSITION_PLAYLIST_ERROR_VALUE,
		"Invalid intrinsic duration for asset");
    return FALSE;
  }
  ((ParseCtxt*)user_data)->current_asset->intrinsic_duration = size;
  return TRUE;
}

static const XMLTreeParserElement asset_intrinsic_duration_elem =
  {
    NAMESPACE_CPL,
    "IntrinsicDuration",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_intrinsic_duration_end
  };

/* Asset/Duration */

static gboolean
asset_duration_end(xmlTextReaderPtr reader, gpointer user_data,
		   gchar *text, GError **error)
{
  gchar *end;
  gsize size = g_ascii_strtoull(text, &end, 10);
  g_free(text);
  if (text == end) {
    g_set_error(error, COMPOSITION_PLAYLIST_ERROR,
		COMPOSITION_PLAYLIST_ERROR_VALUE,
		"Invalid duration for asset");
    return FALSE;
  }
  ((ParseCtxt*)user_data)->current_asset->duration = size;
  return TRUE;
}

static const XMLTreeParserElement asset_duration_elem =
  {
    NAMESPACE_CPL,
    "Duration",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_duration_end
  };

static gboolean
parse_ratio(gchar *str, CompositionPlaylistRatio *ratio, GError **error)
{
  gchar *end;
  char *s = strchr(str, ' ');
  if (!s) {
    g_set_error(error, COMPOSITION_PLAYLIST_ERROR,
		COMPOSITION_PLAYLIST_ERROR_VALUE,
		"No space in edit rate for asset");
    return FALSE;
  }
  *s++ = '\0';
  ratio->num = g_ascii_strtoull(str, &end, 10);
  if (str == end) {
    g_set_error(error, COMPOSITION_PLAYLIST_ERROR,
		COMPOSITION_PLAYLIST_ERROR_VALUE,
		"Invalid numerator");
    return FALSE;
  }
  
  ratio->denom = g_ascii_strtoull(s, &end, 10);
  if (s == end) {
    g_set_error(error, COMPOSITION_PLAYLIST_ERROR,
		COMPOSITION_PLAYLIST_ERROR_VALUE,
		"Invalid denominator");
    return FALSE;
  }
  return TRUE;
}

/* Asset/EditRate */

static gboolean
asset_edit_rate_end(xmlTextReaderPtr reader, gpointer user_data,
		   gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  if (!parse_ratio(text, &ctxt->current_asset->edit_rate, error)) {
    g_free(text);
    g_prefix_error(error, "Invalid edit rate for asset: ");
    return FALSE;
  }
  g_free(text);
  return TRUE;
}

static const XMLTreeParserElement asset_edit_rate_elem =
  {
    NAMESPACE_CPL,
    "EditRate",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_edit_rate_end
  };

/* Asset/EntryPoint */

static gboolean
asset_entry_point_end(xmlTextReaderPtr reader, gpointer user_data,
		      gchar *text, GError **error)
{
  gchar *end;
  gsize size = g_ascii_strtoull(text, &end, 10);
  g_free(text);
  if (text == end) {
    g_set_error(error, COMPOSITION_PLAYLIST_ERROR,
		COMPOSITION_PLAYLIST_ERROR_VALUE,
		"Invalid entry point for asset");
    return FALSE;
  }
  ((ParseCtxt*)user_data)->current_asset->entry_point = size;
  return TRUE;
}

static const XMLTreeParserElement asset_entry_point_elem =
  {
    NAMESPACE_CPL,
    "EntryPoint",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_entry_point_end
  };

/* Asset/Annotation */

static gboolean
asset_annotation_end(xmlTextReaderPtr reader, gpointer user_data,
		     gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->current_asset->annotation = text;
  return TRUE;
}

static const XMLTreeParserElement asset_annotation_elem =
  {
    NAMESPACE_CPL,
    "AnnotationText",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    asset_annotation_end
  };


/* MainPicture */
static gboolean
main_picture_start(xmlTextReaderPtr reader, gpointer user_data, GError **error)
{
  return asset_common_start(reader, user_data, AssetTypePictureTrack, error);
}
  
static const XMLTreeParserElement *main_picture_children[] =
  {&asset_id_elem, &asset_intrinsic_duration_elem, &asset_duration_elem,
   &asset_edit_rate_elem, &asset_entry_point_elem, &asset_annotation_elem};

static const XMLTreeParserElement main_picture_elem =
  {
    NAMESPACE_CPL,
    "MainPicture",
    main_picture_children,
    0,
    main_picture_start,
    asset_common_end
  };

/* MainSound */
static gboolean
main_sound_start(xmlTextReaderPtr reader, gpointer user_data, GError **error)
{
  return asset_common_start(reader, user_data, AssetTypeSoundTrack, error);
}
  
static const XMLTreeParserElement *main_sound_children[] =
  {&asset_id_elem, &asset_intrinsic_duration_elem, &asset_duration_elem,
   &asset_edit_rate_elem, &asset_entry_point_elem, &asset_annotation_elem};

static const XMLTreeParserElement main_sound_elem =
  {
    NAMESPACE_CPL,
    "MainSound",
    main_sound_children,
    0,
    main_sound_start,
    asset_common_end
  };

/* MainSubtitle */
static gboolean
main_subtitle_start(xmlTextReaderPtr reader, gpointer user_data, GError **error)
{
  return asset_common_start(reader, user_data, AssetTypeSubtitleTrack, error);
}
  
static const XMLTreeParserElement *main_subtitle_children[] =
  {&asset_id_elem, &asset_intrinsic_duration_elem, &asset_duration_elem,
   &asset_edit_rate_elem, &asset_entry_point_elem, &asset_annotation_elem};

static const XMLTreeParserElement main_subtitle_elem =
  {
    NAMESPACE_CPL,
    "MainSubtitle",
    main_subtitle_children,
    0,
    main_subtitle_start,
    asset_common_end
  };


/* AssetList */

static gboolean
asset_list_start(xmlTextReaderPtr reader, gpointer user_data, GError **error)
{
  return TRUE;
}

static gboolean
asset_list_end(xmlTextReaderPtr reader, gpointer user_data,
	       gchar *text, GError **error)
{
  return TRUE;
}

static const XMLTreeParserElement *asset_list_children[] =
  {/*&main_markers_elem,*/ &main_picture_elem, &main_sound_elem, &main_subtitle_elem /*, &projector_data_elem*/, NULL};
    
static const XMLTreeParserElement asset_list_elem =
  {
    NAMESPACE_CPL,
    "AssetList",
    asset_list_children,
    0,
    asset_list_start,
    asset_list_end
  };

/* Reel/AnnotationText */

static gboolean
reel_annotation_end(xmlTextReaderPtr reader, gpointer user_data,
		    gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->current_reel->annotation = text;
  return TRUE;
}

static const XMLTreeParserElement reel_annotation_elem =
  {
    NAMESPACE_CPL,
    "AnnotationText",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    reel_annotation_end
  };

/* Reel/Id */

static gboolean
reel_id_end(xmlTextReaderPtr reader, gpointer user_data,
		 gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->current_reel->id = text;
  return TRUE;
}

static const XMLTreeParserElement reel_id_elem =
  {
    NAMESPACE_CPL,
    "Id",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    reel_id_end
  };

/* Reel */

static gboolean
reel_start(xmlTextReaderPtr reader, gpointer user_data, GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->current_reel = g_new(CompositionPlaylistReel, 1);
  ctxt->current_reel->id = NULL;
  ctxt->current_reel->annotation = NULL;
  ctxt->current_reel->assets = NULL;
  return TRUE;
}

static gboolean
reel_end(xmlTextReaderPtr reader, gpointer user_data,
	 gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  if (!ctxt->current_reel->id) {
    g_set_error(error, COMPOSITION_PLAYLIST_ERROR,
		COMPOSITION_PLAYLIST_ERROR_XML_STRUCTURE,
		"No 'Id' element in reel");
    return FALSE;
  }
  ctxt->cpl->reels = g_list_append(ctxt->cpl->reels, ctxt->current_reel);
  ctxt->current_reel = NULL;
  return TRUE;
}

static const XMLTreeParserElement *reel_children[] =
  {&reel_id_elem, &reel_annotation_elem, &asset_list_elem, NULL};
    
static const XMLTreeParserElement reel_elem =
  {
    NAMESPACE_CPL,
    "Reel",
    reel_children,
    0,
    reel_start,
    reel_end
  };


/* ReelList */

static gboolean
reel_list_start(xmlTextReaderPtr reader, gpointer user_data, GError **error)
{
  return TRUE;
}

static gboolean
reel_list_end(xmlTextReaderPtr reader, gpointer user_data,
	      gchar *text, GError **error)
{
  return TRUE;
}

static const XMLTreeParserElement *reel_list_children[] =
  {&reel_elem, NULL};
    
static const XMLTreeParserElement reel_list_elem =
  {
    NAMESPACE_CPL,
    "ReelList",
    reel_list_children,
    0,
    reel_list_start,
    reel_list_end
  };

/* CompositionPlaylist/Id */

static gboolean
composition_playlist_id_end(xmlTextReaderPtr reader, gpointer user_data,
			    gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->cpl->id = text;
  return TRUE;
}

static const XMLTreeParserElement composition_playlist_id_elem =
  {
    NAMESPACE_CPL,
    "Id",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    composition_playlist_id_end
  };

/* CompositionPlaylist */

static const XMLTreeParserElement *composition_playlist_children[] =
  {&reel_list_elem, &composition_playlist_id_elem, NULL};
    
static const XMLTreeParserElement composition_playlist_elem =
  {
    NAMESPACE_CPL,
    "CompositionPlaylist",
    composition_playlist_children,
    0,
    NULL,
    NULL
  };

static const XMLTreeParserElement *top_elements[] =
  {&composition_playlist_elem, NULL};

CompositionPlaylist *
composition_playlist_read(GFile *file, GError **error)
{
  gboolean ret;
  ParseCtxt ctxt;
  CompositionPlaylist *cpl = g_object_new (COMPOSITION_PLAYLIST_TYPE, NULL);
  init_parse_ctxt(&ctxt, cpl);
  ret = xml_tree_parser_parse_file(file, top_elements, &ctxt, error);
  clear_parse_ctxt(&ctxt);
  if (!ret) {
    g_object_unref(cpl);
    return NULL;
  }
  return cpl;
}

GList *
composition_playlist_get_reels(CompositionPlaylist *cpl)
{
  return cpl->reels;
}
