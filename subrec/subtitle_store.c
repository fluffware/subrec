#include <subtitle_store.h>

GQuark
subtitle_store_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark =
      g_quark_from_static_string ("subtitle-store-error-quark");
  return error_quark;
}

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
  gchar *text;
};

typedef struct SubtitleStoreItem SubtitleStoreItem;

static void
destroy_items(SubtitleStoreItem *item);

static void
destroy_item(SubtitleStoreItem *item)
{
  g_free(item->text);
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
}


static const GType column_types[] = {
  G_TYPE_INT64, /* in_ms */
  G_TYPE_INT64, /* out_ms */
  G_TYPE_STRING
};

#define COLUMN_COUNT (sizeof(column_types)/sizeof(column_types[0]))

#define INVALID_RET do {iter->stamp = store->stamp +8329;return FALSE;}while(0)
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
  iter->user_data = child;
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
  SubtitleStoreItem *item = iter->user_data;
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
  SubtitleStoreItem *item = iter->user_data;
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
  case SUBTITLE_STORE_COLUMN_TEXT:
    g_value_init(value, G_TYPE_STRING);
    g_value_set_string(value, item->text);
    break;
  default:
    break;
  }
}

static gboolean
model_iter_next(GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  struct SubtitleStoreItem *item = iter->user_data;
  g_assert(iter->stamp == store->stamp);
  if (!item->next) {
    INVALID_RET;
  }
  iter->user_data = item->next;
  return TRUE;
}
  
static gboolean
model_iter_children(GtkTreeModel *tree_model, GtkTreeIter  *iter,				    GtkTreeIter  *parent)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  if (parent) {
    SubtitleStoreItem *item = parent->user_data;
    if (!item->children) INVALID_RET;
    iter->stamp = store->stamp;
    iter->user_data = item->children;
    return TRUE;
  } else {
    if (!store->items) INVALID_RET;
    iter->stamp = store->stamp;
    iter->user_data = store->items;
    return TRUE;
  }
}

static gboolean
model_iter_has_child(GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  SubtitleStoreItem *item = iter->user_data;
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
    item = iter->user_data;
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
    item = parent->user_data;
    g_assert(parent->stamp == store->stamp);
    item = item->children;
  } else {
    item = store->items;
  }
  item = find_nth_item(item, n);
  if (!item) INVALID_RET;
  iter->stamp = store->stamp;
  iter->user_data = item;
  return TRUE;
}

static gboolean
model_iter_parent(GtkTreeModel *tree_model, GtkTreeIter  *iter,
		  GtkTreeIter  *child)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  SubtitleStoreItem *item = child->user_data;
  g_assert(child->stamp == store->stamp);
  if (!item->parent) INVALID_RET;
  iter->stamp = store->stamp;
  iter->user_data = item->parent;
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
subtitle_store_class_init(SubtitleStoreClass *g_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);
  gobject_class->finalize = subtitle_store_finalize;
 
}

static void
subtitle_store_init(SubtitleStore *instance)
{
  instance->items = NULL;
  instance->stamp = g_random_int() + 48978389;
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
		      guint flags,
		      GtkTreeIter *parent, GtkTreeIter *iter)
{
  GtkTreePath *path;
  GtkTreeIter iter_local;
  SubtitleStoreItem *new_item;
  SubtitleStoreItem **itemp;
  SubtitleStoreItem *parent_item = NULL;
  if (parent) {
    parent_item = parent->user_data;
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
  new_item->text = g_strdup("");
  
  new_item->next = *itemp;
  new_item->prevp = itemp;
  if (*itemp) (*itemp)->prevp = &new_item->next;
  *itemp = new_item;
  new_item->parent = parent_item;
  new_item->children = NULL;
  
  if (!iter) iter = &iter_local;
  iter->stamp = store->stamp;
  iter->user_data = new_item;
  path = model_get_path(GTK_TREE_MODEL(store), iter);
  gtk_tree_model_row_inserted(GTK_TREE_MODEL(store), path, iter); 
  gtk_tree_path_free(path);
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
    SubtitleStoreItem *child = item->children;
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
    SubtitleStoreItem *item = iter->user_data;
    GtkTreePath *path = get_path(store, item);
    remove_item(store, path, item);
    gtk_tree_path_free(path);
  } else {
    GtkTreePath *path = gtk_tree_path_new_from_indices(0, -1);
    remove_items(store, path, store->items);
    gtk_tree_path_free(path);
  }
}
