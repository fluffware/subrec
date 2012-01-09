#include "xml_tree_parser.h"

GQuark
xml_tree_parser_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("xml-tree-parser-error-quark");
  return error_quark;
}

#define ERROR -1
#define EOD 0
#define OK 1
#define NOMATCH 2

struct ParseCtxt
{
  gpointer user_data;
  GString *text;
};
  
/* Returns a matching element if found, NULL if not.
   error is set when an error was encountered */
static const XMLTreeParserElement *
match_element(xmlTextReaderPtr reader, const XMLTreeParserElement * const *parsers, GError **error)
{
  const gchar *element_ns=(const gchar*)xmlTextReaderConstNamespaceUri(reader);
  const gchar *name = (const gchar*)xmlTextReaderConstLocalName(reader);
  while(*parsers) {
    const XMLTreeParserElement *parser = *parsers++;
    if (g_strcmp0 (element_ns, parser->namespace) == 0
	&& g_strcmp0 (name, parser->element_name) == 0) {
      return parser;
    }
  }
  return NULL;
}

static gchar *
get_text(struct ParseCtxt *ctxt)
{
  if (!ctxt->text) return NULL;
  gchar *text = g_string_free(ctxt->text, FALSE);
  ctxt->text = NULL;
  return text;
}

/* The reader must be positioned at element start when this function
   is called.  The position will be after the corresponding element
   end when exiting without error.  */

static int
parse_element(xmlTextReaderPtr reader,
	      const XMLTreeParserElement * const *parsers,
	      struct ParseCtxt *ctxt, 
	      GError **error)
{
  int ret;
  const XMLTreeParserElement *parser;
  g_assert(xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT);
  if (parsers && (parser = match_element(reader, parsers, error))) {
    /* g_debug("Processing child %s", xmlTextReaderConstLocalName(reader)); */
    if (parser->flags & XML_TREE_PARSER_TEXT) {
      if (ctxt->text) {
	g_error("Parent of %s is already collecting text",
		xmlTextReaderConstLocalName(reader));
      }
      if (!parser->end_handler) {
	g_error("Element %s collects text but has no end handler",
		xmlTextReaderConstLocalName(reader));
      }
      ctxt->text = g_string_new("");
    }
    if (parser->start_handler) {
      if (!parser->start_handler(reader, ctxt->user_data,
				 error)) {
	g_assert(error != NULL);
	return ERROR;
      }
    }
    if (!xmlTextReaderIsEmptyElement(reader)) {
      if (parser->child_parsers) {
	/* Parse children */
	int parent_depth = xmlTextReaderDepth(reader);
	ret = xmlTextReaderRead(reader);
	if (ret != OK) return ret;
	while(xmlTextReaderDepth(reader) > parent_depth) {
	  switch(xmlTextReaderNodeType(reader)) {
	  case XML_READER_TYPE_ELEMENT:
	    {
	      ret=parse_element(reader, parser->child_parsers, ctxt, error);
	      if (ret != OK && ret != NOMATCH) {
		return ret;
	      }
	    }
	    break;
	  case XML_READER_TYPE_END_ELEMENT:
	  
	    if ((ret = xmlTextReaderRead(reader)) != OK) {
	      return ret;
	    }
	    break;
	  case XML_READER_TYPE_TEXT:
	  case XML_READER_TYPE_SIGNIFICANT_WHITESPACE :
	    if (ctxt->text) {
	      g_string_append(ctxt->text,
			      (const gchar*)xmlTextReaderConstValue(reader));
	    }
	    if ((ret = xmlTextReaderRead(reader)) != OK) {
	      return ret;
	    }
	    break;
	  default:
	    if ((ret = xmlTextReaderRead(reader)) != OK) {
	      return ret;
	    }
	  }
	}
   
      
      } else {
	/* Skip children */
	int parent_depth = xmlTextReaderDepth(reader);
	ret = xmlTextReaderRead(reader);
	if (ret != OK) return ret;
	while(xmlTextReaderDepth(reader) > parent_depth) {
	  switch(xmlTextReaderNodeType(reader)) {
	  case XML_READER_TYPE_TEXT:
	  case XML_READER_TYPE_SIGNIFICANT_WHITESPACE :
	    if (ctxt->text) {
	      g_string_append(ctxt->text,
			      (const gchar*)xmlTextReaderConstValue(reader));
	    }
	    break;
	  default:
	    break;
	  }
	  if ((ret = xmlTextReaderNext(reader)) != OK) {
	    return ret;
	  }
	
	}
      }
      g_assert(xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT);
    }
    if (parser->end_handler) {
      if (!parser->end_handler(reader, ctxt->user_data,
			       get_text(ctxt), error)) {
	g_assert(error != NULL);
	return ERROR;
      }
    }
    return xmlTextReaderRead(reader);
  } else {
     /* g_debug("Not processing child %s", xmlTextReaderConstLocalName(reader)); */
    if ((ret = xmlTextReaderNext(reader)) == ERROR) {
      return ret;
    }
    return NOMATCH;
  }
}


static void error_cb(void * arg, const char * msg, 
				   xmlParserSeverities severity, 
				   xmlTextReaderLocatorPtr locator)
{
  if (severity == XML_PARSER_SEVERITY_VALIDITY_WARNING
      || severity == XML_PARSER_SEVERITY_WARNING) {
    g_warning("Parser warning: %s:%d: %s", xmlTextReaderLocatorBaseURI(locator),
	      xmlTextReaderLocatorLineNumber(locator), msg);
  } else {
    GError **error = arg;
    g_set_error(error, XML_TREE_PARSER_ERROR, XML_TREE_PARSER_ERROR_READER,
		"%s:%d: %s", xmlTextReaderLocatorBaseURI(locator),
		xmlTextReaderLocatorLineNumber(locator), msg);
  }
}

gboolean
xml_tree_parser_parse(xmlTextReaderPtr reader,
		      const XMLTreeParserElement * const *top_elements,
		      gpointer user_data,
		      GError **error)
{
  GError *xml_error = NULL;
  int ret;
  struct ParseCtxt ctxt;
  ctxt.user_data = user_data;
  ctxt.text = NULL;
  xmlTextReaderSetErrorHandler(reader, error_cb, &xml_error);
  /* Find top element */
  while((ret = xmlTextReaderRead(reader)) == OK) {
    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      ret = parse_element(reader, top_elements, &ctxt, &xml_error);
      if (ret == OK) {
	g_assert(xml_error == NULL);
	break;
      } else if (ret == ERROR) {
	if (xml_error) {
	  g_propagate_error(error, xml_error);
	  return FALSE;
	} else {
	  g_set_error(error, XML_TREE_PARSER_ERROR,
		      XML_TREE_PARSER_ERROR_READER,
		      "XML reader failed");
	  return FALSE;
	}
      } else if (ret == NOMATCH) {
	g_set_error(error, XML_TREE_PARSER_ERROR,
		    XML_TREE_PARSER_ERROR_NO_TOP_ELEMENT,
		    "Invalid top element");
	return FALSE;
      }
    }
  }
  if (ret == ERROR) {
    g_set_error(error, XML_TREE_PARSER_ERROR,
		XML_TREE_PARSER_ERROR_READER,
		"Reader failed");
    return FALSE;
  }
  return TRUE;
}

static int
read_cb(void * context, char * buffer, int len)
{
  GInputStream *in = context;
  return g_input_stream_read(in, buffer, len, NULL, NULL);
}

static int
close_cb(void * context)
{
  GInputStream *in = context;
  return g_input_stream_close(in, NULL, NULL) ? 0 : -1;
}

gboolean
xml_tree_parser_parse_file(GFile *file,
			   const XMLTreeParserElement * const *top_elements,
			   gpointer user_data,
			   GError **error)
{
  gchar *uri;
  xmlTextReaderPtr reader;
  gboolean ret;
  GFileInputStream *in = g_file_read(file, NULL, error);
  if (!in) return FALSE;
  uri = g_file_get_uri(file);
  reader = xmlReaderForIO(read_cb, close_cb, in, uri, NULL, 0);
  g_free(uri);
  if (!reader) {
    g_object_unref(in);
    g_set_error(error, XML_TREE_PARSER_ERROR,
              XML_TREE_PARSER_ERROR_READER,
              "Failed to create XML reader");
    return FALSE;
  }

  ret = xml_tree_parser_parse(reader, top_elements, user_data, error);
  xmlFreeTextReader(reader);
  g_object_unref(in);
  return ret;
}
