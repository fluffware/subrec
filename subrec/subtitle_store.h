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
  struct SubtitleStoreSpot *spots;
  guint n_spots;
  gint stamp;
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
  SUBTITLE_STORE_COLUMN_TEXT
};

/*
 * Create a new list from file
 */
SubtitleStore *
subtitle_store_new();

void
subtitle_store_clear_all(SubtitleStore *store);

gboolean
subtitle_store_insert(SubtitleStore *store, gint64 in_ns, gint64 out_ns,
		      GtkTreeIter *iter);
