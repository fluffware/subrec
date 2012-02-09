#include <subtitle_store.h>
#include <string.h>

#define NO_FILE_TEXT "<none>"

GQuark
subtitle_store_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark =
      g_quark_from_static_string ("subtitle-store-error-quark");
  return error_quark;
}

enum {
  PROP_0 = 0,
  PROP_NO_AUDIO_COLOR,
  PROP_OK_COLOR,
  PROP_WARNING_COLOR,
  PROP_CRITICAL_COLOR,
};
#define DEFAULT_NO_AUDIO_COLOR "#888"
#define DEFAULT_OK_COLOR "#8f8"
#define DEFAULT_WARNING_COLOR "#ff8"
#define DEFAULT_CRITICAL_COLOR "#f88"

#define ITEM_FLAG_TIME_FROM_CHILDREN 0x100
#define ITEM_FLAG_SPOT 0x1
#define ITEM_FLAG_GROUP 0x2
#define ITEM_FLAG_REEL 0x4

struct SubtitleStoreItem
{
  struct SubtitleStoreItem *next; /* NULL if not linked */
  struct SubtitleStoreItem **prevp; /* NULL if not linked */
  struct SubtitleStoreItem *parent;
  struct SubtitleStoreItem *children;
  guint flags;
  gint64 in_ns;
  gint64 out_ns;
  gchar *id;
  gchar *text;
  gchar *filename;
  gint64 duration;
  GtkListStore *filelist;
};

typedef struct SubtitleStoreItem SubtitleStoreItem;

static void
destroy_items(SubtitleStoreItem *item);

static void
destroy_item(SubtitleStoreItem *item)
{
  g_free(item->text);
  g_free(item->filename);
  g_clear_object(&item->filelist);
  destroy_items(item->children);
  g_free(item);
}

static void
destroy_items(SubtitleStoreItem *item)
{
  while (item) {
    SubtitleStoreItem *next = item->next;
    destroy_item(item);
    item = next;
  }	  
}

static void
subtitle_store_finalize(GObject *object)
{
  SubtitleStore *store = SUBTITLE_STORE(object);
  destroy_items(store->items);
  g_free(store->no_audio_color);
  g_free(store->ok_color);
  g_free(store->warning_color);
  g_free(store->critical_color);
}


static GType column_types[] = {
  G_TYPE_INT64, /* in_ms */
  G_TYPE_INT64, /* out_ms */
  G_TYPE_STRING, /* ID */
  G_TYPE_STRING, /* text */
  G_TYPE_STRING, /* filename */
  G_TYPE_INT64, /* duration */
  G_TYPE_STRING, /* color */
  G_TYPE_OBJECT /* file list, set to GTK_TYPE_TREE_MODEL at class init */
};

#define COLUMN_COUNT (sizeof(column_types)/sizeof(column_types[0]))

#define INVALID_RET do {iter->stamp = store->stamp +8329;return FALSE;}while(0)
#define ITER_ITEM(iter) (*((SubtitleStoreItem**)&((iter)->user_data)))
static gint
model_get_n_columns(GtkTreeModel *tree_model)
{
  return COLUMN_COUNT;
}

static GType
model_get_column_type(GtkTreeModel *tree_model, gint index_)
{
  return column_types[index_];
}

static SubtitleStoreItem *
find_nth_item(SubtitleStoreItem *item, guint n)
{
  while(n-- > 0 && item) item = item->next;
  return item;
}

static gboolean
model_get_iter(GtkTreeModel *tree_model, GtkTreeIter  *iter,GtkTreePath  *path)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  gint d = 0;
  gint depth = gtk_tree_path_get_depth(path);
  gint *indices = gtk_tree_path_get_indices(path);
  SubtitleStoreItem *children = store->items;
  SubtitleStoreItem *child = NULL;;
  while(d < depth && children) {
    child = find_nth_item(children, indices[d]);
    if (child == NULL) INVALID_RET;
    children = child->children;
    d++;
  }

  if (d != depth) INVALID_RET;

  iter->stamp = store->stamp;
  ITER_ITEM(iter) = child;
  return TRUE;
}

static gint
find_item_pos(SubtitleStoreItem *item, SubtitleStoreItem *first)
{
  gint pos = 0;
  while(TRUE) {
    if (first == item) return pos;
    pos++;
    first = first->next;
  }
}

static GtkTreePath *
get_path(SubtitleStore *store, SubtitleStoreItem *item)
{
  GtkTreePath *path;
  path = gtk_tree_path_new();
  while(item) {
    gint pos;
    SubtitleStoreItem *first;
    if (item->parent) {
      first = item->parent->children;
    } else {
      first = store->items;
    }
    pos = find_item_pos(item, first);
    gtk_tree_path_prepend_index(path, pos);
    item = item->parent;
  }
  return path;
}

static GtkTreePath *
model_get_path(GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  SubtitleStoreItem *item = ITER_ITEM(iter);
  g_assert(iter->stamp == store->stamp);

  return get_path(store, item);
}

static gint64
get_in_ns(SubtitleStoreItem *item)
{
  if ((item->flags & SUBTITLE_STORE_TIME_FROM_CHILDREN)
      && item->children) {
    return get_in_ns(item->children);
  } else {
    return item->in_ns;
  }
}

static gint64
get_out_ns(SubtitleStoreItem *item)
{
  if ((item->flags & SUBTITLE_STORE_TIME_FROM_CHILDREN)
      && item->children) {
    SubtitleStoreItem *child = item->children;
    while(child->next) child = child->next;
    return get_out_ns(child);
  } else {
    return item->out_ns;
  }
}

static void
model_get_value(GtkTreeModel *tree_model, GtkTreeIter  *iter, gint column,
		GValue *value)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  SubtitleStoreItem *item = ITER_ITEM(iter);
  g_assert(iter->stamp == store->stamp);
  switch(column) {
  case SUBTITLE_STORE_COLUMN_IN:
    g_value_init(value, G_TYPE_INT64);
    g_value_set_int64(value, get_in_ns(item));
    break;
  case SUBTITLE_STORE_COLUMN_OUT:
    g_value_init(value, G_TYPE_INT64);
    g_value_set_int64(value, get_out_ns(item));
    break;
  case SUBTITLE_STORE_COLUMN_ID:
    g_value_init(value, G_TYPE_STRING);
    g_value_set_string(value, item->id);
    break;
  case SUBTITLE_STORE_COLUMN_TEXT:
    {
      g_value_init(value, G_TYPE_STRING);
      g_value_set_string(value, item->text ? item->text : "");
    }
    break;
  case SUBTITLE_STORE_COLUMN_FILE:
    g_value_init(value, G_TYPE_STRING);
    g_value_set_string(value, item->filename ? item->filename : NO_FILE_TEXT);
    break;
  case SUBTITLE_STORE_COLUMN_FILE_DURATION:
    g_value_init(value, G_TYPE_INT64);
    g_value_set_int64(value, item->duration);
    break;
  case SUBTITLE_STORE_COLUMN_FILE_COLOR:
    g_value_init(value, G_TYPE_STRING);
    if (!item->filename) {
      g_value_set_string(value, store->no_audio_color);
    } else {
      if (item->out_ns - item->in_ns >= item->duration) {
	g_value_set_string(value, store->ok_color);
      } else {
	SubtitleStoreItem *next_item = item->next;
	if (next_item && next_item->in_ns < item->in_ns + item->duration) {
	  g_value_set_string(value, store->critical_color);
	} else {
	  g_value_set_string(value, store->warning_color);
	}
      }
    }
    break;
  case SUBTITLE_STORE_COLUMN_FILES:
    g_value_init(value, GTK_TYPE_TREE_MODEL);
    g_value_set_object(value, item->filelist);
    break;
    
  default:
    break;
  }
}

static gboolean
model_iter_next(GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  struct SubtitleStoreItem *item = ITER_ITEM(iter);
  g_assert(iter->stamp == store->stamp);
  if (!item->next) {
    INVALID_RET;
  }
  ITER_ITEM(iter) = item->next;
  return TRUE;
}
  
static gboolean
model_iter_children(GtkTreeModel *tree_model, GtkTreeIter  *iter,				    GtkTreeIter  *parent)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  if (parent) {
    SubtitleStoreItem *item = ITER_ITEM(parent);
    if (!item->children) INVALID_RET;
    iter->stamp = store->stamp;
    ITER_ITEM(iter) = item->children;
    return TRUE;
  } else {
    if (!store->items) INVALID_RET;
    iter->stamp = store->stamp;
    ITER_ITEM(iter) = store->items;
    return TRUE;
  }
}

static gboolean
model_iter_has_child(GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  SubtitleStoreItem *item = ITER_ITEM(iter);
  g_assert(iter->stamp == store->stamp);
  return item->children != NULL;
}

static gint
model_iter_n_children(GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
  gint n = 0;
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  SubtitleStoreItem *item;
  if (iter) {
    item = ITER_ITEM(iter);
    g_assert(iter->stamp == store->stamp);
    item = item->children;
  } else {
    item = store->items;
  }
  while(item) {
    n++;
    item = item->next;
  }
  return n;
}

static gboolean
model_iter_nth_child(GtkTreeModel *tree_model, GtkTreeIter  *iter,
		  GtkTreeIter  *parent, gint n)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  SubtitleStoreItem *item;
  if (parent) {
    item = ITER_ITEM(parent);
    g_assert(parent->stamp == store->stamp);
    item = item->children;
  } else {
    item = store->items;
  }
  item = find_nth_item(item, n);
  if (!item) INVALID_RET;
  iter->stamp = store->stamp;
  ITER_ITEM(iter) = item;
  return TRUE;
}

static gboolean
model_iter_parent(GtkTreeModel *tree_model, GtkTreeIter  *iter,
		  GtkTreeIter  *child)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  SubtitleStoreItem *item = ITER_ITEM(child);
  g_assert(child->stamp == store->stamp);
  if (!item->parent) INVALID_RET;
  iter->stamp = store->stamp;
  ITER_ITEM(iter) = item->parent;
  return TRUE;
}

static void
model_ref_node (GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
}

static void
model_unref_node (GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
}

static void
model_init(gpointer g_iface, gpointer iface_data)
{
  GtkTreeModelIface *iface = g_iface;
  iface->get_n_columns = model_get_n_columns;
  iface->get_column_type = model_get_column_type;
  iface->get_iter = model_get_iter;
  iface->get_path = model_get_path;
  iface->get_value = model_get_value;
  iface->iter_next = model_iter_next;
  iface->iter_children = model_iter_children;
  iface->iter_has_child = model_iter_has_child;
  iface->iter_n_children = model_iter_n_children;
  iface->iter_nth_child = model_iter_nth_child;
  iface->iter_parent = model_iter_parent;
  iface->ref_node = model_ref_node;
  iface->unref_node = model_unref_node;
}

static void
subtitle_store_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  SubtitleStore *store = SUBTITLE_STORE(object);
  switch (prop_id) {
  case PROP_NO_AUDIO_COLOR:
    g_free(store->no_audio_color);
    store->no_audio_color = g_value_dup_string(value);
    break;
  case PROP_OK_COLOR:
    g_free(store->ok_color);
    store->ok_color = g_value_dup_string(value);
    break;
  case PROP_WARNING_COLOR:
    g_free(store->warning_color);
    store->warning_color = g_value_dup_string(value);
    break;
  case PROP_CRITICAL_COLOR:
    g_free(store->critical_color);
    store->critical_color = g_value_dup_string(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
subtitle_store_get_property (GObject * object, guint prop_id,
			     GValue * value, GParamSpec * pspec)
{
  SubtitleStore *store = SUBTITLE_STORE(object);
  switch (prop_id) {
  case PROP_NO_AUDIO_COLOR:
    g_value_set_string(value, store->no_audio_color);  
    break;
  case PROP_OK_COLOR:
    g_value_set_string(value, store->ok_color);  
    break;
  case PROP_WARNING_COLOR:
    g_value_set_string(value, store->warning_color);  
    break;
  case PROP_CRITICAL_COLOR:
    g_value_set_string(value, store->critical_color);  
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
subtitle_store_class_init(SubtitleStoreClass *g_class)
{
  GParamSpec *pspec;
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  gobject_class->finalize = subtitle_store_finalize;
  gobject_class->set_property = subtitle_store_set_property;
  gobject_class->get_property = subtitle_store_get_property;
  
  column_types[SUBTITLE_STORE_COLUMN_FILES] = GTK_TYPE_TREE_MODEL;
  
  pspec =  g_param_spec_string ("no-audio-color",
				"Color when no audio present",
				"Show this color when no audio is present.",
				DEFAULT_NO_AUDIO_COLOR,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_NO_AUDIO_COLOR, pspec);

  pspec =  g_param_spec_string ("ok-color",
				"Color for acceptable duration",
				"Show this color when the duration is shorter "
				"than the spot.",
				DEFAULT_OK_COLOR,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_OK_COLOR, pspec);

  pspec =  g_param_spec_string ("warning-color",
				"Color for long duration",
				"Show this color when the duration is longer "
				"than the spot, but won't overlap following "
				"spot.",
				DEFAULT_WARNING_COLOR,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_WARNING_COLOR, pspec);

  pspec =  g_param_spec_string ("critical-color",
				"Color for unacceptable duration",
				"Show this color when the duration will "
				"overlap following spot.",
				DEFAULT_CRITICAL_COLOR,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property(gobject_class, PROP_CRITICAL_COLOR, pspec);

}

static void
subtitle_store_init(SubtitleStore *instance)
{
  instance->items = NULL;
  instance->stamp = g_random_int() + 48978389;

  instance->no_audio_color = g_strdup(DEFAULT_NO_AUDIO_COLOR);
  instance->ok_color = g_strdup(DEFAULT_OK_COLOR);
  instance->warning_color = g_strdup(DEFAULT_WARNING_COLOR);
  instance->critical_color = g_strdup(DEFAULT_CRITICAL_COLOR);
}

SubtitleStore *
subtitle_store_new()
{
  SubtitleStore *store = g_object_new(SUBTITLE_STORE_TYPE, NULL);
  return store;
}
#if 0
void
subtitle_store_clear_all(SubtitleStore *store)
{
  GtkTreePath *path;
  destroy_items(store->spots);
  path = gtk_tree_path_new_from_indices(store->n_spots, -1);
  store->spots = NULL;
  store->n_spots = 0;
  while(gtk_tree_path_prev(path)) {
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(store), path);
  }
  gtk_tree_path_free(path);
}
#endif

/* Inserts a new spot into the list at the position indicated by the times.
   Returns FALSE if it overlaps with an existing spot */
gboolean
subtitle_store_insert(SubtitleStore *store, gint64 in_ns, gint64 out_ns,
		      const gchar *id, guint flags,
		      GtkTreeIter *parent, GtkTreeIter *iter)
{
  GtkTreePath *path;
  GtkTreeIter iter_local;
  SubtitleStoreItem *new_item;
  SubtitleStoreItem **itemp;
  SubtitleStoreItem *parent_item = NULL;
  if (parent) {
    parent_item = ITER_ITEM(parent);
    itemp = &parent_item->children;
  } else {
    itemp = &store->items;
  }
  if (in_ns >= out_ns) return FALSE;
  while (*itemp) {
    struct SubtitleStoreItem *item = *itemp;
    if (in_ns < item->in_ns) {
      if (out_ns > item->in_ns) return FALSE;
      break;
    } else {
      if (in_ns < item->out_ns) return FALSE;
    }
    itemp = &item->next;
  }
  new_item = g_new(struct SubtitleStoreItem, 1);
  new_item->flags = flags;
  new_item->in_ns = in_ns;
  new_item->out_ns = out_ns;
  new_item->id = g_strdup(id);
  new_item->text = NULL;
  new_item->filename = NULL;
  new_item->duration = 0;
  new_item->filelist = NULL;
  
  new_item->next = *itemp;
  new_item->prevp = itemp;
  if (*itemp) (*itemp)->prevp = &new_item->next;
  *itemp = new_item;
  new_item->parent = parent_item;
  new_item->children = NULL;
  
  if (!iter) iter = &iter_local;
  iter->stamp = store->stamp;
  ITER_ITEM(iter) = new_item;
  path = model_get_path(GTK_TREE_MODEL(store), iter);
  gtk_tree_model_row_inserted(GTK_TREE_MODEL(store), path, iter); 
  gtk_tree_path_free(path);
  return TRUE;
}

gboolean
subtitle_store_set_text(SubtitleStore *store, 
			GtkTreeIter *iter, const gchar *text)
{
  GtkTreePath *path;
  SubtitleStoreItem *item = ITER_ITEM(iter);
  g_free(item->text);
  item->text = g_strdup(text);
  path = model_get_path(GTK_TREE_MODEL(store), iter);
  gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, iter); 
  gtk_tree_path_free(path);
  return TRUE;
}

struct FileSearchCtxt
{
  const gchar *str;
  gboolean found;
  GtkTreeIter *iter;
};
  
  
static gboolean
file_search_func(GtkTreeModel *model, GtkTreePath *path,
		 GtkTreeIter *iter, gpointer data)
{
  gchar *str;
  struct FileSearchCtxt *ctxt = data;
  gtk_tree_model_get(model, iter, 0, &str, -1);
  if (strcmp(str, ctxt->str) == 0) {
    g_free(str);
    if (ctxt->iter) *(ctxt->iter) = *iter;
    ctxt->found = TRUE;
    return TRUE;
  }
  g_free(str);
  return FALSE;
}

static gboolean
file_search(GtkListStore *store, const gchar *str, GtkTreeIter *iter)
{
  struct FileSearchCtxt ctxt;
  if (!store) return FALSE;
  ctxt.str = str;
  ctxt.found = FALSE;
  ctxt.iter = iter;
  gtk_tree_model_foreach(GTK_TREE_MODEL(store), file_search_func, &ctxt);
  return ctxt.found;
}
      
gboolean
subtitle_store_prepend_file(SubtitleStore *store, GtkTreeIter *iter,
			    const gchar *filename, gint64 duration)
{
  SubtitleStoreItem *item;
  GtkTreeIter list_iter;
  GtkTreePath *path;
  g_assert(iter->stamp == store->stamp);
  item = ITER_ITEM(iter);
  if (!item->filelist) {
    item->filelist = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT64);
  } else {
    if (file_search(item->filelist, filename, NULL)) return FALSE;
  }
  gtk_list_store_prepend(item->filelist, &list_iter);
  gtk_list_store_set(item->filelist, &list_iter,
		     SUBTITLE_STORE_FILES_COLUMN_FILE, filename,
		     SUBTITLE_STORE_FILES_COLUMN_DURATION, duration,
		     -1);
  if (!item->filename) {
    g_free(item->filename);
    item->filename = g_strdup(filename);
    item->duration = duration;
  }
  /* Signal row changed */
  path = model_get_path(GTK_TREE_MODEL(store), iter);
  gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, iter);
  gtk_tree_path_free(path);
  return TRUE;
}

gboolean
subtitle_store_remove_file(SubtitleStore *store, 
			   GtkTreeIter *iter, const gchar *file)
{
  SubtitleStoreItem *item;
  GtkTreeIter list_iter;
  GtkTreePath *path;
  g_assert(iter->stamp == store->stamp);
  item = ITER_ITEM(iter);
  if (!item->filelist) return FALSE;
  if (!file_search(item->filelist, file, &list_iter)) return FALSE;
  gtk_list_store_remove(item->filelist, &list_iter);
  if (gtk_tree_model_iter_n_children (GTK_TREE_MODEL(item->filelist), NULL)
      == 0) {
    g_clear_object(&item->filelist);
    g_free(item->filename);
    item->filename = NULL;
  } else {
    if (strcmp(item->filename, file) == 0) {
      /* Set item->filename to first file in list */
      g_free(item->filename);
      gtk_tree_model_get_iter_first(GTK_TREE_MODEL(item->filelist), &list_iter);
      gtk_tree_model_get(GTK_TREE_MODEL(item->filelist), &list_iter,
			 SUBTITLE_STORE_FILES_COLUMN_FILE, &item->filename,
			 -1);
    }
  }
  
  /* Signal row changed */
  path = model_get_path(GTK_TREE_MODEL(store), iter);
  gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, iter);
  gtk_tree_path_free(path);
  return TRUE;
}

gboolean
subtitle_store_set_file(SubtitleStore *store, GtkTreeIter *iter,
			const gchar *filename, gint64 duration)
{
  GtkTreePath *path;
  SubtitleStoreItem *item = ITER_ITEM(iter);
  g_free(item->filename);
  item->filename = NULL;
  if (!file_search(item->filelist, filename, NULL)) {
    subtitle_store_prepend_file(store, iter, filename, duration);
  } else {
    item->filename = g_strdup(filename);
    item->duration = duration;
    path = model_get_path(GTK_TREE_MODEL(store), iter);
    gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, iter); 
    gtk_tree_path_free(path);
  }
  return TRUE;
}

G_DEFINE_TYPE_WITH_CODE(SubtitleStore, subtitle_store, G_TYPE_OBJECT,\
			G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,model_init))

static void
unlink_item(SubtitleStoreItem *item)
{
  if (item->next) {
    item->next->prevp = item->prevp;
  }
  *item->prevp = item->next;
}

static void
remove_item(SubtitleStore *store, GtkTreePath *path, SubtitleStoreItem *item);

static void
remove_items(SubtitleStore *store, GtkTreePath *path, SubtitleStoreItem *first_child)
{
  SubtitleStoreItem *child = first_child;
  gint64 l  = 0;
  while(child) {
    l++;
    child=child->next;
  }
  SubtitleStoreItem **children = g_new(SubtitleStoreItem*, l);
  child = first_child;
  l = 0;
  while(child) {
    children[l++] = child;
    gtk_tree_path_next(path);
    child=child->next;
  }
  while(l-- > 0) {
    gtk_tree_path_prev(path);
    remove_item(store, path, children[l]);
  }
}

static void
remove_item(SubtitleStore *store, GtkTreePath *path, SubtitleStoreItem *item)
{
  if (item->children) {
    gtk_tree_path_down(path);
    remove_items(store, path, item->children);
    gtk_tree_path_up(path);
  }
  unlink_item(item);
  destroy_item(item);
  gtk_tree_model_row_deleted(GTK_TREE_MODEL(store), path);
  
}

void
subtitle_store_remove(SubtitleStore *store, GtkTreeIter *iter)
{
  if (iter) {
    SubtitleStoreItem *item = ITER_ITEM(iter);
    GtkTreePath *path = get_path(store, item);
    remove_item(store, path, item);
    gtk_tree_path_free(path);
  } else {
    GtkTreePath *path = gtk_tree_path_new_from_indices(0, -1);
    remove_items(store, path, store->items);
    gtk_tree_path_free(path);
  }
}

gchar *
subtitle_store_get_filename(SubtitleStore *store, GtkTreeIter *iter)
{
  SubtitleStoreItem *item = ITER_ITEM(iter);
  return item->filename;
}

gint64
subtitle_store_get_file_duration(SubtitleStore *store, GtkTreeIter *iter)
{
  SubtitleStoreItem *item = ITER_ITEM(iter);
  return item->duration;
}
