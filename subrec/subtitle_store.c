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


struct SubtitleStoreSpot
{
  struct SubtitleStoreSpot *next; /* NULL if not linked */
  struct SubtitleStoreSpot **prevp; /* NULL if not linked */
  gint64 in_ns;
  gint64 out_ns;
  gchar *text;
};

static void
destroy_spot(struct SubtitleStoreSpot *spot)
{
  g_free(spot->text);
  g_free(spot);
}

static void
destroy_spots(struct SubtitleStoreSpot *spot)
{
  while (spot) {
    struct SubtitleStoreSpot *next = spot->next;
    destroy_spot(spot);
    spot = next;
  }	  
}

static void
subtitle_store_finalize(GObject *object)
{
  SubtitleStore *store = SUBTITLE_STORE(object);
  destroy_spots(store->spots);
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

static gboolean
model_get_iter(GtkTreeModel *tree_model, GtkTreeIter  *iter,GtkTreePath  *path)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  gint pos = gtk_tree_path_get_indices(path)[0];
  struct SubtitleStoreSpot *spot = store->spots;
  if (pos >= store->n_spots) return FALSE;
  while(pos-- > 0) spot = spot->next;
  iter->stamp = store->stamp;
  iter->user_data = spot;
  return TRUE;
}

static GtkTreePath *
model_get_path(GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
  gint pos = 0;
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  struct SubtitleStoreSpot *spot = iter->user_data;
  struct SubtitleStoreSpot *s = store->spots;
  g_assert(iter->stamp == store->stamp);
  while(TRUE) {
    g_assert(s != NULL);
    if (s == spot) return gtk_tree_path_new_from_indices(pos, -1);
    s = s->next;
    pos++;
  }
}

static void
model_get_value(GtkTreeModel *tree_model, GtkTreeIter  *iter, gint column,
		GValue *value)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  struct SubtitleStoreSpot *spot = iter->user_data;
  g_assert(iter->stamp == store->stamp);
  switch(column) {
  case SUBTITLE_STORE_COLUMN_IN:
    g_value_init(value, G_TYPE_INT64);
    g_value_set_int64(value, spot->in_ns);
    break;
  case SUBTITLE_STORE_COLUMN_OUT:
    g_value_init(value, G_TYPE_INT64);
    g_value_set_int64(value, spot->out_ns);
    break;
  case SUBTITLE_STORE_COLUMN_TEXT:
    g_value_init(value, G_TYPE_STRING);
    g_value_set_string(value, spot->text);
    break;
  default:
    break;
  }
}

static gboolean
model_iter_next(GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  struct SubtitleStoreSpot *spot = iter->user_data;
  g_assert(iter->stamp == store->stamp);
  if (!spot->next) {
    INVALID_RET;
  }
  iter->user_data = spot->next;
  return TRUE;
}
  
static gboolean
model_iter_children(GtkTreeModel *tree_model, GtkTreeIter  *iter,				    GtkTreeIter  *parent)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  if (parent) {
    INVALID_RET;
  } else {
    if (!store->spots) INVALID_RET;
    iter->stamp = store->stamp;
    iter->user_data = store->spots;
    return TRUE;
  }
}

static gboolean
model_iter_has_child(GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
  return FALSE;
}

static gint
model_iter_n_children(GtkTreeModel *tree_model, GtkTreeIter  *iter)
{
  if (iter) return 0;
  else return SUBTITLE_STORE(tree_model)->n_spots;
}

static gboolean
model_iter_nth_child(GtkTreeModel *tree_model, GtkTreeIter  *iter,
		  GtkTreeIter  *parent, gint n)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  if (parent) {
    INVALID_RET;
  } else {
    struct SubtitleStoreSpot *spot = store->spots;
    if (n >= store->n_spots) INVALID_RET;
    while(n-- > 0) spot = spot->next;
    iter->stamp = store->stamp;
    iter->user_data = spot;
    return TRUE;
  }
}

static gboolean
model_iter_parent(GtkTreeModel *tree_model, GtkTreeIter  *iter,
		  GtkTreeIter  *child)
{
  SubtitleStore *store = SUBTITLE_STORE(tree_model);
  INVALID_RET;
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
  instance->spots = NULL;
  instance->n_spots = 0;
  instance->stamp = g_random_int() + 48978389;
}

SubtitleStore *
subtitle_store_new()
{
  SubtitleStore *store = g_object_new(SUBTITLE_STORE_TYPE, NULL);
  return store;
}

void
subtitle_store_clear_all(SubtitleStore *store)
{
  GtkTreePath *path;
  destroy_spots(store->spots);
  path = gtk_tree_path_new_from_indices(store->n_spots, -1);
  store->spots = NULL;
  store->n_spots = 0;
  while(gtk_tree_path_prev(path)) {
    gtk_tree_model_row_deleted(GTK_TREE_MODEL(store), path);
  }
  gtk_tree_path_free(path);
}

/* Inserts a new spot into the list at the position indicated by the times.
   Rteurns FALSE if it overlaps with an existing spot */
gboolean
subtitle_store_insert(SubtitleStore *store, gint64 in_ns, gint64 out_ns,
		      GtkTreeIter *iter)
{
  GtkTreePath *path;
  gint pos = 0;
  GtkTreeIter iter_local;
  struct SubtitleStoreSpot *new_spot;
  struct SubtitleStoreSpot **spotp = &store->spots;
  if (in_ns >= out_ns) return FALSE;
  while (*spotp) {
    struct SubtitleStoreSpot *spot = *spotp;
    if (in_ns < spot->in_ns) {
      if (out_ns > spot->in_ns) return FALSE;
      break;
    } else {
      if (in_ns < spot->out_ns) return FALSE;
    }
    spotp = &spot->next;
    pos++;
  }
  new_spot = g_new(struct SubtitleStoreSpot, 1);
  new_spot->in_ns = in_ns;
  new_spot->out_ns = out_ns;
  new_spot->text = g_strdup("");
  
  new_spot->next = *spotp;
  new_spot->prevp = spotp;
  if (*spotp) (*spotp)->prevp = &new_spot->next;
  *spotp = new_spot;

  store->n_spots++;
  store->stamp++; /* Invalidate previous stamps */
  if (!iter) iter = &iter_local;
  iter->stamp = store->stamp;
  iter->user_data = new_spot;
  path = gtk_tree_path_new_from_indices(pos, -1);
  gtk_tree_model_row_inserted(GTK_TREE_MODEL(store), path, iter); 
  gtk_tree_path_free(path);
  return TRUE;
}
    
G_DEFINE_TYPE_WITH_CODE(SubtitleStore, subtitle_store, G_TYPE_OBJECT,\
			G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,model_init))
