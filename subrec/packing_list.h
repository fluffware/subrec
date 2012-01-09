#include <glib-object.h>
#include <gio/gio.h>

#define PACKING_LIST_ERROR (packing_list_error_quark())
enum {
  PACKING_LIST_ERROR_CREATE_READER_FAILED = 1,
  PACKING_LIST_ERROR_PARSER,
  PACKING_LIST_ERROR_XML_STRUCTURE,
  PACKING_LIST_ERROR_EMPTY_ELEMENT,
  PACKING_LIST_ERROR_NO_ASSET_LIST,
  PACKING_LIST_ERROR_VALUE
};

/*
 * Type macros.
 */
#define PACKING_LIST_TYPE                  (packing_list_get_type ())
#define PACKING_LIST(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), PACKING_LIST_TYPE, PackingList))
#define IS_PACKING_LIST(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PACKING_LIST_TYPE))
#define PACKING_LIST_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), PACKING_LIST_TYPE, PackingListClass))
#define IS_PACKING_LIST_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), PACKING_LIST_TYPE))
#define PACKING_LIST_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), PACKING_LIST_TYPE, PackingListClass))

typedef struct _PackingList        PackingList;
typedef struct _PackingListClass   PackingListClass;
typedef struct _PackingListAsset   PackingListAsset;

struct _PackingListAsset
{
  gchar *id;
  gchar *hash;
  gchar *type;
  gchar *annotation;
  gsize size;
};
  
struct _PackingList
{
  GObject parent_instance;
  
  /* instance members */
  gchar *id;
  GTree *assets;
  
};

struct _PackingListClass
{
  GObjectClass parent_class;

  /* class members */
};

/* used by PACKING_LIST_TYPE */
GType packing_list_get_type (void);

/*
 * Method definitions.
 */


/*
 * Create a new list from file
 */
PackingList *
packing_list_read(GFile *file, GError **error);

/*
 * The returned array (but not the data pointed to) must be freed by the caller.
 * Result is only valid as long as the packing list is not modified.
 * Array is NULL terminated.
 */

const PackingListAsset **
packing_list_find_asset_with_type(PackingList *list, const gchar *type);
