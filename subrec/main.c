#include <gtk/gtk.h>
#include <gst/gst.h>
#include <stdio.h>
#include <stdlib.h>
#include <builderutils.h>
#include <libxml/xmlreader.h>
#include <asset_map.h>
#include <packing_list.h>
#include <composition_playlist.h>
#include <dcsubtitle.h>
#include <subtitle_store.h>
#include <gtkcellrenderertime.h>

#define SUBREC_ERROR (subrec_error_quark())
enum {
  SUBREC_ERROR_CREATE_ELEMENT_FAILED = 1,
  SUBREC_ERROR_LINK_FAILED,
  SUBREC_ERROR_CREATE_WIDGET_FAILED
};

GQuark
subrec_error_quark()
{
  static GQuark error_quark = 0;
  if (error_quark == 0)
    error_quark = g_quark_from_static_string ("subrec-error-quark");
  return error_quark;
}

G_MODULE_EXPORT void
main_win_destroyed(GtkWidget *object, gpointer user_data)
{
  gtk_main_quit();
}

G_MODULE_EXPORT void
quit_action_cb(GtkAction *action, gpointer user_data)
{
  gtk_main_quit();
}



typedef struct
{
  GtkWidget *main_win;
  GtkFileChooserDialog *load_dialog;
  SubtitleStore *subtitle_store;
  AssetMap *asset_map;
  PackingList *packing_list;
  CompositionPlaylist *cpl;
} AppContext;

static void
app_init(AppContext *app)
{
  app->main_win = NULL;
  app->load_dialog = NULL;
  app->asset_map = NULL;
  app->packing_list = NULL;
  app->cpl = NULL;
}

static void
app_destroy(AppContext *app)
{
  g_clear_object(&app->main_win);
  g_clear_object(&app->asset_map);
  g_clear_object(&app->packing_list);
  g_clear_object(&app->cpl);
  g_clear_object(&app->subtitle_store);
}

static void
load_dialog_destroyed(GtkWidget *object, AppContext *app)
{
  app->load_dialog = NULL;
}

static GFile *
get_cpl(AppContext *app)
{
  const PackingListAsset **cpl_assets;
  GFile *file = NULL;
  cpl_assets = packing_list_find_asset_with_type(app->packing_list,
						"text/xml;asdcpKind=CPL");
  if (cpl_assets && cpl_assets[0]) {
    file = asset_map_get_file(app->asset_map,  cpl_assets[0]->id);
  }
  g_free(cpl_assets);
  return file;
}

static void
load_dialog_response(GtkDialog *dialog,
		     gint response_id, AppContext *app)
{
  GError *error = NULL;
  gtk_widget_hide(GTK_WIDGET(dialog));
  if (response_id == GTK_RESPONSE_ACCEPT) {
    gchar *uri;
    GFile *file;    
    file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
    uri = g_file_get_uri(file);
    g_debug("File: %s", uri);
    g_free(uri);
    g_clear_object(&app->asset_map);
    app->asset_map = asset_map_read(file, &error);
    g_object_unref(file);
    if (!app->asset_map) {
      g_warning("Failed to load asset map: %s", error->message);
      g_error_free(error);
      return;
    }
    
    g_object_get(app->asset_map, "packing-list", &file, NULL);
    uri = g_file_get_uri(file);
    g_debug("Packing list: %s", uri);
    g_free(uri);
    g_clear_object(&app->packing_list);
    app->packing_list = packing_list_read(file, &error);
    g_object_unref(file);
    if (!app->packing_list) {
      g_warning("Failed to load packing list: %s", error->message);
      g_error_free(error);
      return;
    }

    file = get_cpl(app);
    if (!file) {
      g_warning("No CPL found");
      return;
    }

    g_clear_object(&app->cpl);
    app->cpl = composition_playlist_read(file, &error);
    g_object_unref(file);
    if (!app->cpl) {
      g_warning("Failed to load composition playlist: %s", error->message);
      g_error_free(error);
      return;
    }
    subtitle_store_remove(app->subtitle_store, NULL);
    {
      gint64 reel_pos = 0;
      GtkTreeIter reel_iter;
      GList *reels = composition_playlist_get_reels(app->cpl);
      while (reels) {
	CompositionPlaylistReel *reel = (CompositionPlaylistReel*)reels->data;
	GList *assets = reel->assets;
	g_debug("Reel: %s", reel->id);
	while(assets) {
	  GList *spots;
	  CompositionPlaylistAsset *asset =
	    (CompositionPlaylistAsset*)assets->data;
	  if (asset->type == AssetTypeSubtitleTrack) {
	    gint64 duration;
	    DCSubtitle *sub;
	    GFile *file = asset_map_get_file(app->asset_map, asset->id);
	    gchar *uri = g_file_get_uri(file);
	    g_debug("Asset: %s File: %s", asset->id, uri);
	    g_free(uri);
	    duration = asset->duration * asset->edit_rate.denom * 1000000000LL / asset->edit_rate.num;
	    subtitle_store_insert(app->subtitle_store,
				  reel_pos, reel_pos + duration,
				  0, NULL, &reel_iter);
	    reel_pos += duration;
	    sub = dcsubtitle_read(file, &error);
	    
	    g_object_unref(file);

	    if (!sub) {
	      g_warning("Failed to load subtitle: %s", error->message);
	      g_error_free(error);
	      return;
	    }
	    spots = dcsubtitle_get_spots(sub);
	    while(spots) {
	      DCSubtitleSpot *spot = spots->data;
	      subtitle_store_insert(app->subtitle_store,
				    spot->time_in * 1000000LL,
				    spot->time_out * 1000000LL,
				    0, &reel_iter, NULL);
	      spots = spots->next;
	    }
	    g_object_unref(sub);
	  }
	  assets = assets->next;
	}
	reels = reels->next;
      }
    }
  }
  
}

G_MODULE_EXPORT void
open_assetmap_action_cb(GtkAction *action, AppContext *app)
{
  if (!app->load_dialog) {
    app->load_dialog =
      GTK_FILE_CHOOSER_DIALOG(
			      gtk_file_chooser_dialog_new("Load ASSETMAP",
							  GTK_WINDOW(app->main_win),
							  GTK_FILE_CHOOSER_ACTION_OPEN,
							  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
							  NULL));
    g_signal_connect(app->load_dialog, "destroy",
		     G_CALLBACK(load_dialog_destroyed), app);
    g_signal_connect(app->load_dialog, "response",
		     G_CALLBACK(load_dialog_response), app);
    
  }
  gtk_widget_show(GTK_WIDGET(app->load_dialog));
}

static GObject *
find_object(GtkBuilder *builder, const gchar *name, GError **err)
{
  GObject *obj = gtk_builder_get_object(builder, name);
  if (obj) return obj;
  g_set_error(err, SUBREC_ERROR,
              SUBREC_ERROR_CREATE_WIDGET_FAILED,
              "Failed to get object '%s'", name);
  return NULL;
}

#define FIND_OBJECT(name) find_object(builder, name, err)

static gboolean
setup_subtitle_list(AppContext *app, GtkBuilder *builder, GError **err)
{
  GtkCellRenderer *render;
  GtkTreeViewColumn *column;
  GtkTreeView *viewer;
  app->subtitle_store = subtitle_store_new();
  subtitle_store_insert(app->subtitle_store, 1298982, 8192229002, 0, NULL, NULL);
  viewer = GTK_TREE_VIEW(FIND_OBJECT("subtitle_list"));
  if (!viewer) return FALSE;
  gtk_tree_view_set_model(viewer, GTK_TREE_MODEL(app->subtitle_store));
  gtk_tree_view_set_headers_visible(viewer, TRUE);
  /* In column */
  render = gtk_cell_renderer_time_new();
  column =
    gtk_tree_view_column_new_with_attributes("In", render,
					     "time", SUBTITLE_STORE_COLUMN_IN,
					     NULL);
  gtk_tree_view_append_column(viewer, column);

  /* In column */
  column =
    gtk_tree_view_column_new_with_attributes("Out", render,
					     "time", SUBTITLE_STORE_COLUMN_OUT,
					     NULL);
  gtk_tree_view_append_column(viewer, column);
  {
    GtkTreeIter iter;
    subtitle_store_insert(app->subtitle_store,  29827923793, 98902000202, 0, NULL, NULL);
    subtitle_store_insert(app->subtitle_store, 9192229002, 29827923793, 0, NULL, &iter);
    subtitle_store_insert(app->subtitle_store, 0, 1000000000, SUBTITLE_STORE_TIME_FROM_CHILDREN, &iter, &iter);
    subtitle_store_insert(app->subtitle_store, 1000000000,  5000000000, 0, &iter, NULL);
    subtitle_store_insert(app->subtitle_store, 0000000000,  1000000000, 0, &iter, NULL);
  }
  return TRUE;
}

static gboolean
create_main_window(AppContext *app, GError **err)
{
  GtkBuilder *builder = gtk_builder_new();

  if (!builderutils_add_from_file(builder, "main_win.ui", err)) {
    g_object_unref(builder);
    return FALSE;
  }
  
  app->main_win = GTK_WIDGET(FIND_OBJECT("mainwin"));
  if (!app->main_win) {
    g_object_unref(builder);
    return FALSE;
  }
  g_object_ref(app->main_win);

  if (!setup_subtitle_list(app, builder, err)) {
    g_object_unref(builder);
    return FALSE;
  }
  gtk_builder_connect_signals(builder, app);
 

  g_object_unref(builder);
  return TRUE;
}


int
main(int argc, char **argv)
{
  GError *error = NULL;
  AppContext app;
  LIBXML_TEST_VERSION
  app_init(&app);
  /* gst_init(&argc, &argv); */
  gtk_init(&argc, &argv);

  if (!create_main_window(&app, &error)) {
    app_destroy(&app);
    g_error("%s", error->message);
  }
  gtk_widget_show(app.main_win);
  gtk_main();
  g_debug("Exiting");
  app_destroy(&app);
  return EXIT_SUCCESS;
}
