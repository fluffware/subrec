#include "read_asset_map.h"
#include <libxml/xmlreader.h>
#include <string.h>
#define NAMESPACE_AM "http://www.digicine.com/PROTO-ASDCP-AM-20040311#"
GQuark
asset_map_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("subrec-error-quark");
  return error_quark;
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
    g_set_error(error, ASSET_MAP_ERROR, ASSET_MAP_ERROR_PARSER,
		"%s:%d: %s", xmlTextReaderLocatorBaseURI(locator),
		xmlTextReaderLocatorLineNumber(locator), msg);
  }
}

static int
move_to_element(xmlTextReaderPtr reader)
{
  int ret;
  while((ret = xmlTextReaderRead(reader)) == 1
	&& xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT);
  return ret;
}

static int
move_to_named_element(xmlTextReaderPtr reader, const char *name)
{
  int ret;
  while((ret = xmlTextReaderRead(reader)) == 1
	&& xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT
	&& strcmp((const char*)xmlTextReaderConstLocalName(reader),name) != 0);
  return ret;
}

static void
process_element(xmlTextReaderPtr reader, gboolean start, GError **error)
{
  if (strcmp((const char*)xmlTextReaderConstNamespaceUri(reader),
	     NAMESPACE_AM) == 0) {
    if (start)
      g_debug("<%s>",  xmlTextReaderConstLocalName(reader));
    else
      g_debug("</%s>",  xmlTextReaderConstLocalName(reader));
  }
}

static int
process_doc(xmlTextReaderPtr reader, GError **error)
{
  int ret;
#if 0
  /* Move to first element */
  ret = move_to_element(reader);
  if (ret != 1) {
    return ret;
  }

  if (strcmp((const char*)xmlTextReaderConstLocalName(reader),"AssetMap")!=0
      || strcmp((const char*)xmlTextReaderConstNamespaceUri(reader),
		NAMESPACE_AM) != 0) {
    g_set_error(error, ASSET_MAP_ERROR, ASSET_MAP_ERROR_WRONG_TOP_ELEMENT,
		"Invalid top element '%s'",xmlTextReaderConstLocalName(reader));
    return -1;
  }
#endif
  while((ret = xmlTextReaderRead(reader)) == 1) {
    switch(xmlTextReaderNodeType(reader)) {
    case XML_READER_TYPE_ELEMENT:
      process_element(reader, TRUE, error);
      break;
    case XML_READER_TYPE_END_ELEMENT:
      process_element(reader, FALSE, error);
      break;
    default:
      
      g_debug("Type: %d name %s", xmlTextReaderNodeType(reader), xmlTextReaderConstLocalName(reader));
    }
  }
  return ret;
}

gboolean
read_asset_map(GFile *file, GError **error)
{

  int ret;
  xmlTextReaderPtr reader;
  GFileInputStream *in = g_file_read(file, NULL, error);
  if (!in) return FALSE;
  reader = xmlReaderForIO(read_cb, close_cb, in, g_file_get_uri(file), NULL, 0);
  if (!reader) {
    g_object_unref(in);
    g_set_error(error, ASSET_MAP_ERROR,
              ASSET_MAP_ERROR_CREATE_READER_FAILED,
              "Failed to create XML reader");
    return FALSE;
  }
  xmlTextReaderSetErrorHandler(reader, error_cb, error);
  ret = process_doc(reader, error);
  xmlFreeTextReader(reader);
  g_object_unref(in);
  if (ret == -1) {
    return FALSE;
  }
  return TRUE;
}
