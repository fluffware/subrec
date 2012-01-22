#include "dcsubtitle.h"
#include <xml_tree_parser.h>
#include <string.h>

#define NAMESPACE_DCS NULL

GQuark
dcsubtitle_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark =
      g_quark_from_static_string ("dcsubtitle-error-quark");
  return error_quark;
}

G_DEFINE_TYPE (DCSubtitle, dcsubtitle, G_TYPE_OBJECT)

enum {
  PROP_0 = 0,
  PROP_ID,
};

static void
destroy_text(DCSubtitleText *text)
{
  g_free(text->text);
  g_free(text);
}

static void
destroy_spot(DCSubtitleSpot *spot)
{
  g_list_free_full(spot->text, (GDestroyNotify)destroy_text);
  g_free(spot);
}

static void
dcsubtitle_finalize(GObject *object)
{
  DCSubtitle *sub = DCSUBTITLE(object);
  g_list_free_full(sub->spots, (GDestroyNotify)destroy_spot);
  g_free(sub->id);
  g_free(sub->language);
}

static void
dcsubtitle_get_property(GObject    *object,
				  guint       property_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  DCSubtitle *sub = DCSUBTITLE(object);
  switch(property_id) {
  case PROP_ID:
    g_value_set_string (value, sub->id);
    break;
  }
}
static void
dcsubtitle_class_init(DCSubtitleClass *g_class)
{
  GParamSpec *pspec;
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  gobject_class->finalize = dcsubtitle_finalize;
  gobject_class->get_property = dcsubtitle_get_property;

  pspec = g_param_spec_string ("id",
                               "DCSubtitle Id",
                               "Get ID of this DCSubtitle",
                               "" /* default value */,
                               G_PARAM_READABLE);
  g_object_class_install_property (gobject_class, PROP_ID, pspec);


}

static void
dcsubtitle_init(DCSubtitle *instance)
{
  instance->id = NULL;
  instance->spots = NULL;
}


typedef struct ParseCtxt {
  DCSubtitle *sub;
  DCSubtitleSpot *current_spot;
  DCSubtitleText *current_text;
} ParseCtxt;

static void
init_parse_ctxt(ParseCtxt *ctxt, DCSubtitle *sub)
{
  ctxt->sub = sub;
  ctxt->current_spot = NULL;
  ctxt->current_text = NULL;
}

static void
clear_parse_ctxt(ParseCtxt *ctxt)
{
  if (ctxt->current_spot) destroy_spot(ctxt->current_spot);
  if (ctxt->current_text) destroy_text(ctxt->current_text);
}

static gint
parse_time_attr(xmlTextReaderPtr reader, const gchar *attr, GError **error)
{
  glong ms;
  xmlChar *timestr = xmlTextReaderGetAttribute(reader, (const xmlChar*)attr);
  const gchar *start = (const gchar *)timestr;
  gchar * end;
  if (!timestr) {
    g_set_error(error, DCSUBTITLE_ERROR,
		DCSUBTITLE_ERROR_XML_STRUCTURE,
		"Element has no attribute named '%s'", attr);
    return 0;
  }
  ms = (3600 * 1000) * g_ascii_strtoull(start, &end, 10);
  if (end == start || *end != ':') {
    xmlFree(timestr);
    g_set_error(error, DCSUBTITLE_ERROR,
		DCSUBTITLE_ERROR_VALUE,
		"Invalid time value");
    return 0;
  }
  start = end + 1;
  ms += (60 * 1000) * g_ascii_strtoull(start, &end, 10);
  if (end == start || *end != ':') {
    xmlFree(timestr);
    g_set_error(error, DCSUBTITLE_ERROR,
		DCSUBTITLE_ERROR_VALUE,
		"Invalid time value");
    return 0;
  }
  start = end + 1;
  ms += (1000) * g_ascii_strtoull(start, &end, 10);
  if (end == start || *end != ':') {
    xmlFree(timestr);
    g_set_error(error, DCSUBTITLE_ERROR,
		DCSUBTITLE_ERROR_VALUE,
		"Invalid time value");
    return 0;
  }
  start = end + 1;
  ms += 4 * g_ascii_strtoull(start, &end, 10);
  if (end == start || *end != '\0') {
    xmlFree(timestr);
    g_set_error(error, DCSUBTITLE_ERROR,
		DCSUBTITLE_ERROR_VALUE,
		"Invalid time value");
    return 0;
  }
  
  xmlFree(timestr);
  return ms;
}

static gint
parse_int_attr(xmlTextReaderPtr reader, const gchar *attr, GError **error)
{
  gchar * end;
  glong v;
  gchar *str = (gchar*)xmlTextReaderGetAttribute(reader, (const xmlChar*)attr);
  if (!str) {
    g_set_error(error, DCSUBTITLE_ERROR,
		DCSUBTITLE_ERROR_XML_STRUCTURE,
		"Element has no attribute named '%s'", attr);
    return 0;
  }
  v = g_ascii_strtoull(str, &end, 10);
  if (end == str || *end != '\0') {
    xmlFree(str);
    g_set_error(error, DCSUBTITLE_ERROR, DCSUBTITLE_ERROR_VALUE,
		"Invalid integer value");
    return 0;
  }
  xmlFree(str);
  return v;
}

static gdouble
parse_double_attr(xmlTextReaderPtr reader, const gchar *attr, GError **error)
{
  gchar * end;
  gdouble v;
  gchar *str = (gchar*)xmlTextReaderGetAttribute(reader, (const xmlChar*)attr);
  if (!str) {
    g_set_error(error, DCSUBTITLE_ERROR,
		DCSUBTITLE_ERROR_XML_STRUCTURE,
		"Element has no attribute named '%s'", attr);
    return 0;
  }
  v = g_ascii_strtod(str, &end);
  if (end == str || *end != '\0') {
    xmlFree(str);
    g_set_error(error, DCSUBTITLE_ERROR, DCSUBTITLE_ERROR_VALUE,
		"Invalid float value");
    return 0;
  }
  xmlFree(str);
  return v;
}

typedef struct {
  const gchar *enum_str;
  gint value;
} EnumMap;

static gboolean
parse_enum_attr(xmlTextReaderPtr reader, const gchar *attr,
		const EnumMap *map, gint *value, GError **error)
{
  gchar *str = (gchar*)xmlTextReaderGetAttribute(reader, (const xmlChar*)attr);
  if (!str) {
    g_set_error(error, DCSUBTITLE_ERROR,
		DCSUBTITLE_ERROR_XML_STRUCTURE,
		"Element has no attribute named '%s'", attr);
    return FALSE;
  }
  while(map->enum_str) {
    if (strcmp(map->enum_str, str) == 0) {
      *value = map->value;
      break;
    }
    map++;
  }
  xmlFree(str);
  return TRUE;
}
/* Text */

static const EnumMap dir_enums[] = {
  {"horizontal", SUBTITLE_DIR_HORIZONTAL},
  {NULL,0}
};

static const EnumMap halign_enums[] = {
  {"left", SUBTITLE_HALIGN_LEFT},
  {"center", SUBTITLE_HALIGN_CENTER},
  {"right", SUBTITLE_HALIGN_RIGHT},
  {NULL,0}
};

static const EnumMap valign_enums[] = {
  {"top", SUBTITLE_VALIGN_TOP},
  {"center", SUBTITLE_VALIGN_CENTER},
  {"bottom", SUBTITLE_VALIGN_BOTTOM},
  {NULL,0}
};


static gboolean
text_start(xmlTextReaderPtr reader, gpointer user_data, GError **error)
{
  gint value;
  ParseCtxt *ctxt = user_data;
  DCSubtitleText *text = g_new(DCSubtitleText, 1);
  g_assert(ctxt->current_text == NULL);
  ctxt->current_text = text;
  text->text = NULL;
  text->flags = 0;
  text->hpos = 0.0;
  text->vpos = 0.0;

  value = SUBTITLE_DIR_UNKNOWN;
  if (!parse_enum_attr(reader, "Direction", dir_enums, &value, error)) {
    if ((*error)->code != DCSUBTITLE_ERROR_XML_STRUCTURE) return FALSE;
    g_clear_error(error);
    value = SUBTITLE_DIR_UNKNOWN;
  }
  text->flags |= value;

  value = SUBTITLE_HALIGN_UNKNOWN;
  if (!parse_enum_attr(reader, "HAlign", halign_enums, &value, error)) {
    if ((*error)->code != DCSUBTITLE_ERROR_XML_STRUCTURE) return FALSE;
    g_clear_error(error);
    value = SUBTITLE_HALIGN_UNKNOWN;
  }
  text->flags |= value;

  value = SUBTITLE_VALIGN_UNKNOWN;
  if (!parse_enum_attr(reader, "VAlign", valign_enums, &value, error)) {
    if ((*error)->code != DCSUBTITLE_ERROR_XML_STRUCTURE) return FALSE;
    g_clear_error(error);
    value = SUBTITLE_VALIGN_UNKNOWN;
  }
  text->flags |= value;

  text->vpos = parse_double_attr(reader, "VPosition",error);
  if (*error) {
    if ((*error)->code != DCSUBTITLE_ERROR_XML_STRUCTURE) return FALSE;
    g_clear_error(error);
    text->vpos = 0.0;
  }
  
  text->hpos = parse_double_attr(reader, "HPosition",error);
  if (*error) {
     if ((*error)->code != DCSUBTITLE_ERROR_XML_STRUCTURE) return FALSE;
    g_clear_error(error);
    text->hpos = 0.0;
  }
  
  /* g_debug("Text flags %08x , (%f, %f)", text->flags, text->hpos, text->vpos); */
  return TRUE;
}

static gboolean
text_end(xmlTextReaderPtr reader, gpointer user_data,
	     gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->current_text->text = text;
  /* g_debug("Text: '%s'", text); */
  ctxt->current_spot->text =
    g_list_append(ctxt->current_spot->text, ctxt->current_text);
  ctxt->current_text = NULL;
  return TRUE;
}

static const XMLTreeParserElement text_elem =
  {
    NAMESPACE_DCS,
    "Text",
    NULL,
    XML_TREE_PARSER_TEXT,
    text_start,
    text_end
  };

/* Subtitle */

static gboolean
subtitle_start(xmlTextReaderPtr reader, gpointer user_data, GError **error)
{
  ParseCtxt *ctxt = user_data;
  DCSubtitleSpot *spot = g_new(DCSubtitleSpot, 1);
  g_assert(ctxt->current_spot == NULL);
  ctxt->current_spot = spot;
  spot->text = NULL;
  
  spot->time_in = parse_time_attr(reader, "TimeIn", error);
  if (*error) {
    return FALSE;
  }
  
  spot->time_out = parse_time_attr(reader, "TimeOut", error);
  if (*error) {
    return FALSE;
  }

  spot->fade_up_time = parse_int_attr(reader, "FadeUpTime",error);
  if (*error) {
    if ((*error)->code != DCSUBTITLE_ERROR_XML_STRUCTURE) return FALSE;
    g_clear_error(error);
    spot->fade_up_time = 0;
  }

  spot->fade_down_time = parse_int_attr(reader, "FadeDownTime",error);
  if (*error) {
    if ((*error)->code != DCSUBTITLE_ERROR_XML_STRUCTURE) return FALSE;
    g_clear_error(error);
    spot->fade_down_time = 0;
  }

  spot->spot_number = parse_int_attr(reader, "SpotNumber",error);
  if (*error) {
    return FALSE;
  }
  
  
  /* g_debug("Time %d %d", ctxt->current_spot->time_in, ctxt->current_spot->time_out); */
  return TRUE;
}

static gboolean
subtitle_end(xmlTextReaderPtr reader, gpointer user_data,
	     gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->sub->spots = g_list_append(ctxt->sub->spots, ctxt->current_spot);
  ctxt->current_spot = NULL;
  return TRUE;
}

static const XMLTreeParserElement *subtitle_children[] =
  {&text_elem, NULL};
    
static const XMLTreeParserElement subtitle_elem =
  {
    NAMESPACE_DCS,
    "Subtitle",
    subtitle_children,
    0,
    subtitle_start,
    subtitle_end
  };

/* Font */

static const XMLTreeParserElement *font_children[] =
  {&subtitle_elem, NULL};
    
static const XMLTreeParserElement font_elem =
  {
    NAMESPACE_DCS,
    "Font",
    font_children,
    0,
    NULL,
    NULL
  };

/* Language */

static gboolean
language_end(xmlTextReaderPtr reader, gpointer user_data,
		 gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->sub->language = text;
  return TRUE;
}



static const XMLTreeParserElement language_elem =
  {
    NAMESPACE_DCS,
    "Language",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    language_end
  };

/* SubtitleID */

static gboolean
subtitle_id_end(xmlTextReaderPtr reader, gpointer user_data,
		 gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  ctxt->sub->id = text;
  return TRUE;
}



static const XMLTreeParserElement subtitle_id_elem =
  {
    NAMESPACE_DCS,
    "SubtitleID",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    subtitle_id_end
  };

/* DCSubtitle */

static const XMLTreeParserElement *dcsubtitle_children[] =
  {&subtitle_id_elem, &language_elem, &font_elem, NULL};
    
static const XMLTreeParserElement dcsubtitle_elem =
  {
    NAMESPACE_DCS,
    "DCSubtitle",
    dcsubtitle_children,
    0,
    NULL,
    NULL
  };

static const XMLTreeParserElement *top_elements[] =
  {&dcsubtitle_elem, NULL};

DCSubtitle *
dcsubtitle_read(GFile *file, GError **error)
{
  gboolean ret;
  ParseCtxt ctxt;
  DCSubtitle *sub = g_object_new (DCSUBTITLE_TYPE, NULL);
  init_parse_ctxt(&ctxt, sub);
  ret = xml_tree_parser_parse_file(file, top_elements, &ctxt, error);
  clear_parse_ctxt(&ctxt);
  if (!ret) {
    g_object_unref(sub);
    return NULL;
  }
  return sub;
}

GList *
dcsubtitle_get_spots(DCSubtitle *sub)
{
  return sub->spots;
}
