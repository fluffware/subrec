#include "save_sequence.h"

static gboolean
build_sequence_children(SubtitleStore *subtitles, GtkTreeIter *iter,
		       GstClockTime start, GstClockTime end,
		       GError **err)
{
  GstClockTime in;
  GstClockTime out;
  do {
    GtkTreeIter child;
    if (gtk_tree_model_iter_children (GTK_TREE_MODEL(subtitles), &child, iter)) {
      if (!build_sequence_children(subtitles, &child, start, end,err)) {
	return FALSE;
      }
    } else {
      gtk_tree_model_get(GTK_TREE_MODEL(subtitles), iter,
			 SUBTITLE_STORE_COLUMN_GLOBAL_IN, &in,
			 SUBTITLE_STORE_COLUMN_GLOBAL_OUT, &out,
			 -1);
    }
  } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(subtitles), iter));
  return TRUE;
}

static gboolean
build_sequence(SubtitleStore *subtitles, GstClockTime start, GstClockTime end,
	       GError **err)
{
  GtkTreeIter top;
  if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(subtitles), &top)) return TRUE;
  return build_sequence_children(subtitles, &top, start, end, err);

 }
      
gboolean
save_sequence(SubtitleStore *subtitles,
	      GstClockTime start, GstClockTime end,
	      GError **err)
{
  return build_sequence(subtitles, start, end, err);
}
 
