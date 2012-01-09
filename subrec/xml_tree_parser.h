#ifndef __XML_TREE_PARSER_H__K0LVOQCCDH__
#define __XML_TREE_PARSER_H__K0LVOQCCDH__

#include <glib.h>
#include <gio/gio.h>
#include <libxml/xmlreader.h>

#define XML_TREE_PARSER_TEXT 1

#define XML_TREE_PARSER_ERROR xml_tree_parser_error_quark()
enum {
  XML_TREE_PARSER_ERROR_NO_TOP_ELEMENT = 1,
  XML_TREE_PARSER_ERROR_INVALID_TOP_ELEMENT,
  XML_TREE_PARSER_ERROR_READER,
};

GQuark
xml_tree_parser_error_quark();

typedef struct XMLTreeParserElement XMLTreeParserElement;

struct XMLTreeParserElement
{
  const gchar *namespace;
  const gchar *element_name;
  const struct XMLTreeParserElement * const *child_parsers;
  guint flags;
  gboolean (*start_handler)(xmlTextReaderPtr reader, gpointer user_data,
			    GError **error);

  /* The text must be freed by the callback */
  gboolean (*end_handler)(xmlTextReaderPtr reader, gpointer user_data,
			  gchar *text, GError **error);
  
};


gboolean
xml_tree_parser_parse(xmlTextReaderPtr reader,
		      const XMLTreeParserElement * const *top_elements,
		      gpointer user_data, GError **error);

/* Parse file */
gboolean
xml_tree_parser_parse_file(GFile *file,
			   const XMLTreeParserElement * const *top_elements,
			   gpointer user_data,
			   GError **error);
#endif /* __XML_TREE_PARSER_H__K0LVOQCCDH__ */
