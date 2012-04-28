#include <subtitle_store_io.h>
#include <string.h>
#include <xml_tree_parser.h>

#define NAMESPACE "http://www.fluffware.se/xml/namespace/SubtitleList"

GQuark
subtitle_store_io_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark =
      g_quark_from_static_string ("subtitle-store-io-error-quark");
  return error_quark;
}

static gboolean
write_gstring(GOutputStream *output, GString *buffer, GError **error)
{
  gboolean ret;
  gsize written;
  ret = g_output_stream_write_all (output, buffer->str, buffer->len,
				   &written, NULL, error);
  g_string_truncate(buffer,0);
  return ret;
}

static gboolean
write_string(GOutputStream *output, const gchar *str, GError **error)
{
  gboolean ret;
  gsize written;
  ret = g_output_stream_write_all (output, str, strlen(str),
				   &written, NULL, error);
  return ret;
}

static gboolean
save_subtitles(SubtitleStore *store, GtkTreeIter *iter,
	       GOutputStream *output, GString *buffer, GError **error)
{
  do {
    gint64 in;
    gint64 out;
    gchar *id;
    gchar *text;
    gchar *text_esc;
    const gchar *file;
    gint64 duration;
    GtkTreeModel *files;
    GtkTreeIter child;
    gboolean has_children;
    gtk_tree_model_get(GTK_TREE_MODEL(store), iter,
		       SUBTITLE_STORE_COLUMN_IN, &in,
		       SUBTITLE_STORE_COLUMN_OUT, &out,
		       SUBTITLE_STORE_COLUMN_ID, &id,
		       SUBTITLE_STORE_COLUMN_TEXT, &text,
		       SUBTITLE_STORE_COLUMN_FILES, &files,
		       -1);
    has_children = gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), iter);
    g_string_append(buffer, has_children ? "<Group" : "<Subtitle");
    g_string_append_printf(buffer, " TimeIn=\"%lld\" TimeOut=\"%lld\"",
			   in, out);
    g_string_append_printf(buffer, " id=\"%s\"", id);
    g_string_append(buffer, ">\n");
    text_esc = g_markup_escape_text (text, -1);
    g_string_append_printf(buffer, "<Text>%s</Text>\n", text_esc);
    g_free(text_esc);
    file = subtitle_store_get_filename(store, iter);
    duration = subtitle_store_get_file_duration(store, iter);
    if (file) {
      g_string_append_printf(buffer,
			     "<AudioFile Duration=\"%lld\">%s</AudioFile>\n",
			     duration, file);
    }
    if (files) {
      GtkTreeIter f;
      if (gtk_tree_model_get_iter_first(files, &f)) {
	do {
	  gchar *altfile;
	  gtk_tree_model_get(files, &f,
			     SUBTITLE_STORE_FILES_COLUMN_FILE, &altfile,
			     SUBTITLE_STORE_FILES_COLUMN_DURATION, &duration,
			     -1);
	  if (strcmp(file, altfile) != 0) {
	    g_string_append_printf(buffer,
				   "<AltAudioFile Duration=\"%lld\">%s"
				   "</AltAudioFile>\n", duration, altfile);
	  }
	  g_free(altfile);
	} while(gtk_tree_model_iter_next(files, &f));
      }
      g_object_unref(files);
    }
    g_free(id);
    g_free(text);
    if (!write_gstring(output, buffer, error)) return FALSE;

    
    if (gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &child, iter)) {
      do {
	if (!save_subtitles(store, &child, output, buffer, error)) {
	  return FALSE;
	}
      } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store),&child));
    }
    g_string_append(buffer, has_children ? "</Group>" :"</Subtitle>");
    if (!write_gstring(output, buffer, error)) return FALSE;

  } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store),iter));
  return TRUE;
}

static gboolean
save_reels(SubtitleStore *store, GOutputStream *output, GString *buffer,
	   GError **error)
{
  GtkTreeIter iter;
  g_string_truncate(buffer,0);
  if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
    do {
      gint64 in;
      gint64 out;
      gchar *id;
      gchar *text;
      gchar *text_esc;
      GtkTreeIter child;
      gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
			 SUBTITLE_STORE_COLUMN_IN, &in,
			 SUBTITLE_STORE_COLUMN_OUT, &out,
			 SUBTITLE_STORE_COLUMN_ID, &id,
			 SUBTITLE_STORE_COLUMN_TEXT, &text,
			 -1);
      g_string_append(buffer, "<Reel");
      g_string_append_printf(buffer, " TimeIn=\"%lld\" TimeOut=\"%lld\"",
			     in, out);
      g_string_append_printf(buffer, " id=\"%s\"", id);
      g_string_append(buffer, ">\n");
      text_esc = g_markup_escape_text (text, -1);
      g_string_append_printf(buffer, "<Text>%s</Text>\n", text_esc);
      g_free(text_esc);
      if (!write_gstring(output, buffer, error)) return FALSE;
      g_free(id);
      g_free(text);

      if (gtk_tree_model_iter_children(GTK_TREE_MODEL(store), &child, &iter)) {
	if (!save_subtitles(store, &child, output, buffer, error)) return FALSE;
      }
      
      if (!write_string(output, "</Reel>\n", error)) return FALSE;
    } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
  }
  return TRUE;
}

static gboolean
save_top(SubtitleStore *store, GOutputStream *output,  GError **error)
{
  gboolean ret;
  GString *buffer;
  static const gchar *prolog =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<SubtitleList xmlns=\"" NAMESPACE "\">\n";
   static const gchar *epilog =
     "</SubtitleList>\n";
   if (!write_string(output, prolog, error)) return FALSE;
  buffer = g_string_new("");
  ret = save_reels(store, output, buffer, error);
  g_string_free(buffer, TRUE);
  if (!ret) return FALSE;
  if (!write_string(output, epilog, error)) return FALSE;
  return TRUE;
}

gboolean
subtitle_store_io_save(SubtitleStore *store, GFile *file, GError **error)
{
  GOutputStream *output;
  output = G_OUTPUT_STREAM(g_file_replace(file, NULL, TRUE,
					  G_FILE_CREATE_REPLACE_DESTINATION,
					  NULL, error));
  if (!output) return FALSE;
  if (!save_top(store, output, error)) {
    g_object_unref(output);
    return FALSE;
  }
  if (!g_output_stream_close(output, NULL, error)) {
    g_object_unref(output);
    return FALSE;
  }
  g_object_unref(output);
  return TRUE;
}

typedef struct ParseCtxt {
  SubtitleStore *store;
  GtkTreeIter iter;
} ParseCtxt;

static void
init_parse_ctxt(ParseCtxt *ctxt, SubtitleStore *store)
{
  ctxt->store = store;
}

static void
clear_parse_ctxt(ParseCtxt *ctxt)
{
}

static gint64
parse_int_attr(xmlTextReaderPtr reader, const gchar *attr, GError **error)
{
  gchar * end;
  gint64 v;
  gchar *str = (gchar*)xmlTextReaderGetAttribute(reader, (const xmlChar*)attr);
  if (!str) {
    g_set_error(error, SUBTITLE_STORE_IO_ERROR,
		SUBTITLE_STORE_IO_ERROR_XML_STRUCTURE,
		"Element has no attribute named '%s'", attr);
    return 0;
  }
  v = g_ascii_strtoull(str, &end, 10);
  if (end == str || *end != '\0') {
    xmlFree(str);
    g_set_error(error, SUBTITLE_STORE_IO_ERROR, SUBTITLE_STORE_IO_ERROR_VALUE,
		"Invalid integer value");
    return 0;
  }
  xmlFree(str);
  return v;
}

/* AudioFile */
static gboolean
audio_file_end(xmlTextReaderPtr reader, gpointer user_data,
	 gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  gint64 duration = parse_int_attr(reader, "Duration", error);
  if (*error) return FALSE;
  subtitle_store_set_file(ctxt->store, &ctxt->iter, text, duration);
  g_free(text);
  return TRUE;
}


static const XMLTreeParserElement audio_file_elem =
  {
    NAMESPACE,
    "AudioFile",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    audio_file_end,
  };

/* AltAudioFile */
static gboolean
alt_audio_file_end(xmlTextReaderPtr reader, gpointer user_data,
	 gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  gint64 duration = parse_int_attr(reader, "Duration", error);
  if (*error) return FALSE;
  subtitle_store_prepend_file(ctxt->store, &ctxt->iter, text, duration);
  g_free(text);
  return TRUE;
}


static const XMLTreeParserElement alt_audio_file_elem =
  {
    NAMESPACE,
    "AltAudioFile",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    alt_audio_file_end,
  };


/* Text */
static gboolean
text_end(xmlTextReaderPtr reader, gpointer user_data,
	 gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  subtitle_store_set_text(ctxt->store, &ctxt->iter, text);
  g_free(text);
  return TRUE;
}


static const XMLTreeParserElement text_elem =
  {
    NAMESPACE,
    "Text",
    NULL,
    XML_TREE_PARSER_TEXT,
    NULL,
    text_end,
  };

/* Subtitle */
static gboolean
subtitle_start(xmlTextReaderPtr reader, gpointer user_data, GError **error)
{
  GtkTreeIter iter;
  gboolean ret;
  ParseCtxt *ctxt = user_data;
  gint64 in;
  gint64 out;
  gchar *id;
  in = parse_int_attr(reader, "TimeIn", error);
  if (*error) return FALSE;
  out = parse_int_attr(reader, "TimeOut", error);
  if (*error) return FALSE;
  id = (gchar*)xmlTextReaderGetAttribute(reader, (const xmlChar*)"id");
  if (!id) {
    g_set_error(error, SUBTITLE_STORE_IO_ERROR,
		SUBTITLE_STORE_IO_ERROR_XML_STRUCTURE,
		"Subtitle has no attribute named 'id'");
    return FALSE;
  }
  ret = subtitle_store_insert(ctxt->store, in, out, id, 0, &ctxt->iter, &iter);
  g_free(id);
  if (!ret) {
    g_set_error(error, SUBTITLE_STORE_IO_ERROR,
		SUBTITLE_STORE_IO_ERROR_FAILED,
		"Failed to insert reel");
  }
  ctxt->iter = iter;
  return ret;
}

static gboolean
subtitle_end(xmlTextReaderPtr reader, gpointer user_data,
	 gchar *text, GError **error)
{
  ParseCtxt *ctxt = user_data;
  GtkTreeIter parent;
  gtk_tree_model_iter_parent(GTK_TREE_MODEL(ctxt->store), &parent, &ctxt->iter);
  ctxt->iter = parent;
  return TRUE;
}

static const XMLTreeParserElement *subtitle_children[] =
  {&text_elem, &audio_file_elem, &alt_audio_file_elem, NULL};

static const XMLTreeParserElement subtitle_elem =
  {
    NAMESPACE,
    "Subtitle",
    subtitle_children,
    0,
    subtitle_start,
    subtitle_end
  };

/* Reel */

static gboolean
reel_start(xmlTextReaderPtr reader, gpointer user_data, GError **error)
{
  gboolean ret;
  ParseCtxt *ctxt = user_data;
  gint64 in;
  gint64 out;
  gchar *id;
  in = parse_int_attr(reader, "TimeIn", error);
  if (*error) return FALSE;
  out = parse_int_attr(reader, "TimeOut", error);
  if (*error) return FALSE;
  id = (gchar*)xmlTextReaderGetAttribute(reader, (const xmlChar*)"id");
  if (!id) {
    g_set_error(error, SUBTITLE_STORE_IO_ERROR,
		SUBTITLE_STORE_IO_ERROR_XML_STRUCTURE,
		"Reel has no attribute named 'id'");
    return FALSE;
  }
  ret = subtitle_store_insert(ctxt->store, in, out, id, 0, NULL, &ctxt->iter);
  g_free(id);
  if (!ret) {
    g_set_error(error, SUBTITLE_STORE_IO_ERROR,
		SUBTITLE_STORE_IO_ERROR_FAILED,
		"Failed to insert reel");
  }
  return ret;
}

static const XMLTreeParserElement *reel_children[] =
  {&subtitle_elem, &text_elem, NULL};

static const XMLTreeParserElement reel_elem =
  {
    NAMESPACE,
    "Reel",
    reel_children,
    0,
    reel_start,
    NULL
  };

static const XMLTreeParserElement *subtitle_list_children[] =
  {&reel_elem, NULL};
    
static const XMLTreeParserElement subtitle_list_elem =
  {
    NAMESPACE,
    "SubtitleList",
    subtitle_list_children,
    0,
    NULL,
    NULL
  };

static const XMLTreeParserElement *top_elements[] =
  {&subtitle_list_elem, NULL};

gboolean
subtitle_store_io_load(SubtitleStore *store, GFile *file, GError **error)
{
  gboolean ret;
  ParseCtxt ctxt;
  init_parse_ctxt(&ctxt, store);
  ret = xml_tree_parser_parse_file(file, top_elements, &ctxt, error);
  clear_parse_ctxt(&ctxt);
  return ret;
}

