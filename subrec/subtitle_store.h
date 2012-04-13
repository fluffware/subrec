#ifndef __SUBTITLE_STORE_H__MNFQQQ6EPP__
#define __SUBTITLE_STORE_H__MNFQQQ6EPP__

#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define SUBTITLE_STORE_ERROR (subtitle_store_error_quark())
enum {
  SUBTITLE_STORE_ERROR_FAILED = 1,
};

/*
 * Type macros.
 */
#define SUBTITLE_STORE_TYPE (subtitle_store_get_type ())
#define SUBTITLE_STORE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SUBTITLE_STORE_TYPE, SubtitleStore))
#define IS_SUBTITLE_STORE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SUBTITLE_STORE_TYPE))
#define SUBTITLE_STORE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SUBTITLE_STORE_TYPE, SubtitleStoreClass)) 
#define IS_SUBTITLE_STORE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SUBTITLE_STORE_TYPE))
#define SUBTITLE_STORE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), SUBTITLE_STORE_TYPE, SubtitleStoreClass))

typedef struct _SubtitleStore        SubtitleStore;
typedef struct _SubtitleStoreClass   SubtitleStoreClass;

struct _SubtitleStore
{
  GObject parent_instance;
  
  /* instance members */
  struct SubtitleStoreItem *items;
  gint stamp;

  /* Colors for color column */
  gchar *no_audio_color;
  gchar *ok_color;
  gchar *warning_color;
  gchar *critical_color;
};

struct _SubtitleStoreClass
{
  GObjectClass parent_class;

  /* class members */
};

/* used by SUBTITLE_STORE_TYPE */
GType subtitle_store_get_type (void);

/*
 * Method definitions.
 */

enum {
  SUBTITLE_STORE_COLUMN_IN = 0,
  SUBTITLE_STORE_COLUMN_OUT,
  SUBTITLE_STORE_COLUMN_GLOBAL_IN,
  SUBTITLE_STORE_COLUMN_GLOBAL_OUT,
  SUBTITLE_STORE_COLUMN_ID,
  SUBTITLE_STORE_COLUMN_TEXT,
  SUBTITLE_STORE_COLUMN_FILE,
  SUBTITLE_STORE_COLUMN_FILE_DURATION,
  SUBTITLE_STORE_COLUMN_FILE_COLOR,
  SUBTITLE_STORE_COLUMN_FILES,
};

enum {
  SUBTITLE_STORE_FILES_COLUMN_FILE = 0,
  SUBTITLE_STORE_FILES_COLUMN_DURATION,
};

/*
 * Create a new list from file
 */
SubtitleStore *
subtitle_store_new();

void
subtitle_store_clear_all(SubtitleStore *store);

#define SUBTITLE_STORE_TIME_FROM_CHILDREN 0x1

gboolean
subtitle_store_insert(SubtitleStore *store, gint64 in_ns, gint64 out_ns,
		      const gchar *id, guint flags,
		      GtkTreeIter *parent, GtkTreeIter *iter);

gboolean
subtitle_store_prepend_file(SubtitleStore *store, GtkTreeIter *iter,
			    const gchar *file, gint64 duration);

gboolean
subtitle_store_remove_file(SubtitleStore *store, 
			   GtkTreeIter *iter, const gchar *file);

/* Setting iter to NULL removes all items */
void
subtitle_store_remove(SubtitleStore *store, GtkTreeIter *iter);

gboolean
subtitle_store_set_text(SubtitleStore *store, 
			GtkTreeIter *iter, const gchar *text);

gboolean
subtitle_store_set_file(SubtitleStore *store, GtkTreeIter *iter,
			const gchar *filename, gint64 duration);

gchar *
subtitle_store_get_filename(SubtitleStore *store, GtkTreeIter *iter);

gint64
subtitle_store_get_file_duration(SubtitleStore *store, GtkTreeIter *iter);

#endif /* __SUBTITLE_STORE_H__MNFQQQ6EPP__ */
