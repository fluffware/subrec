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
#include <subtitle_store_io.h>
#include <gtkcellrenderertime.h>
#include <clip_recorder.h>
#include <about_dialog.h>
#include <string.h>
#include <math.h>

#define SUBTITLE_LIST_FILENAME "SUBTITLES.xml"
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

#define dB(x) (powf(10.0,((x)/10.0)))
#define DEFAULT_SILENCE_LEVEL dB(-30)
#define DEFAULT_NORMAL_LEVEL dB(-10)
#define DEFAULT_TOP_LEVEL dB(-3)

typedef struct
{
  GtkWidget *main_win;
  GtkMessageDialog *error_dialog;
  GtkFileChooserDialog *load_dialog;
  GtkFileChooserDialog *working_directory_dialog;
  GtkAction *play_action;
  GtkAction *record_action;
  GtkAction *stop_action;
  GtkActionGroup *global_actions;
  GtkActionGroup *subtitle_actions;
  GtkActionGroup *record_actions;
  GtkImage *red_lamp;
  GtkImage *yellow_lamp;
  GtkImage *green_lamp;
  SubtitleStore *subtitle_store;
  GtkTreePath *active_subtitle;
  AssetMap *asset_map;
  PackingList *packing_list;
  CompositionPlaylist *cpl;
  GtkTreeView *subtitle_list_view;
  GtkTreeSelection *subtitle_selection;
  GtkTextBuffer *subtitle_text_buffer;
  GtkTextView *subtitle_text_view;
  ClipRecorder *recorder;
  guint record_timer;
  GFile *recorded_file;
  GFile *working_directory;
} AppContext;

static void
app_init(AppContext *app)
{
  app->main_win = NULL;
  app->error_dialog = NULL;
  app->load_dialog = NULL;
  app->working_directory_dialog = NULL;
  app->global_actions = NULL;
  app->asset_map = NULL;
  app->packing_list = NULL;
  app->cpl = NULL;
  app->subtitle_store = NULL;
  app->active_subtitle = NULL;
  app->subtitle_list_view = NULL;
  app->subtitle_selection = NULL;
  app->subtitle_text_buffer = NULL;
  app->subtitle_text_view = NULL;
  app->recorder = NULL;
  app->record_timer = 0;
  app->recorded_file = NULL;
  app->working_directory = NULL;
  app->global_actions = NULL;
  app->subtitle_actions = NULL;
  app->record_actions = NULL;
}

static void
app_destroy(AppContext *app)
{
  if (app->record_timer != 0) {
    g_source_remove(app->record_timer);
    app->record_timer = 0;
  }
  g_clear_object(&app->main_win);
  g_clear_object(&app->global_actions);
  g_clear_object(&app->subtitle_actions);
  g_clear_object(&app->record_actions);
  g_clear_object(&app->asset_map);
  g_clear_object(&app->packing_list);
  g_clear_object(&app->cpl);
  g_clear_object(&app->subtitle_store);
  if (app->active_subtitle) {
    gtk_tree_path_free(app->active_subtitle);
    app->active_subtitle = NULL;
  }
  g_clear_object(&app->subtitle_text_buffer);
  g_clear_object(&app->recorder);
  g_clear_object(&app->recorded_file);
  g_clear_object(&app->working_directory);
}

static void
error_dialog_destroyed(GtkWidget *object, AppContext *app)
{
  app->error_dialog = NULL;
}

static void
error_dialog_response(GtkDialog *dialog,
		      gint response_id, AppContext *app)
{
  gtk_widget_hide(GTK_WIDGET(dialog));
}

static GtkMessageDialog *
get_message_dialog(AppContext *app)
{
  if (!app->error_dialog) {
    GtkWidget *dialog =
      gtk_message_dialog_new(GTK_WINDOW(app->main_win),
			     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			     GTK_MESSAGE_ERROR,
			     GTK_BUTTONS_OK,
			     "Error");
    g_signal_connect(dialog, "destroy",
		    G_CALLBACK(error_dialog_destroyed), app);
    g_signal_connect(dialog, "response",
		     G_CALLBACK(error_dialog_response), app);
    app->error_dialog =GTK_MESSAGE_DIALOG(dialog);
  }
  return app->error_dialog;
}

/* error is cleared */
static void
show_error(AppContext *app, const gchar *title, GError **error)
{
  if (!get_message_dialog(app)) {
    g_warning("%s: %s", title, (*error)->message);
  } else {
    g_object_set(app->error_dialog, "text", title, NULL); 
  
    gtk_message_dialog_format_secondary_text(app->error_dialog,
					     "%s", (*error)->message);
    g_clear_error(error);
    gtk_widget_show(GTK_WIDGET(app->error_dialog));
  }
}

static void
show_error_msg(AppContext *app, const gchar *title, const char *msg)
{
  if (!get_message_dialog(app)) {
    g_debug("%s: %s", title, msg);
  } else {
    g_object_set(app->error_dialog, "text", title, NULL); 
    gtk_message_dialog_format_secondary_text(app->error_dialog,
					     "%s", msg);
    gtk_widget_show(GTK_WIDGET(app->error_dialog));
  }
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
      show_error(app, "Failed to load asset map", &error);
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
      show_error(app, "Failed to load packing list", &error);
      return;
    }

    file = get_cpl(app);
    if (!file) {
      show_error_msg(app,"No CPL found" , "No CPL found in assetmap");
      return;
    }

    g_clear_object(&app->cpl);
    app->cpl = composition_playlist_read(file, &error);
    g_object_unref(file);
    if (!app->cpl) {
      show_error(app, "Failed to load composition playlist", &error);
      return;
    }
    subtitle_store_remove(app->subtitle_store, NULL);
    {
      gint64 reel_pos = 0;
      guint reel_number = 1;
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
	    gchar *reel_id;
	    gchar *uri = g_file_get_uri(file);
	    g_debug("Asset: %s File: %s", asset->id, uri);
	    g_free(uri);
	    reel_id = strrchr(reel->id, ':');
	    if (!reel_id) {
	      reel_id = reel->id;
	    } else {
	      reel_id++;
	    }
	    duration = asset->duration * asset->edit_rate.denom * 1000000000LL / asset->edit_rate.num;
	    subtitle_store_insert(app->subtitle_store,
				  reel_pos, reel_pos + duration,
				  reel_id,
				  0, NULL, &reel_iter);
	    reel_pos += duration;
	    reel_number++;
	    
	    sub = dcsubtitle_read(file, &error);
	    
	    g_object_unref(file);

	    if (!sub) {
	      show_error(app, "Failed to load subtitle", &error);
	      return;
	    }
	    spots = dcsubtitle_get_spots(sub);
	    while(spots) {
	      GString *text_buffer;
	      GtkTreeIter spot_iter;
	      GList *texts;
	      DCSubtitleSpot *spot = spots->data;
	      gchar id[10];
	      g_snprintf(id, sizeof(id), "%d", spot->spot_number);
	      subtitle_store_insert(app->subtitle_store,
				    spot->time_in * 1000000LL,
				    spot->time_out * 1000000LL,
				    id,
				    0, &reel_iter, &spot_iter);
	      texts = spot->text;
	      text_buffer = g_string_new("");
	      while(texts) {
		DCSubtitleText *text = texts->data;
		g_string_append(text_buffer, text->text);
		texts = texts->next;
		if (texts) g_string_append_c(text_buffer, '\n');
	      }
	      subtitle_store_set_text(app->subtitle_store, &spot_iter,
				      text_buffer->str);
	      g_string_free(text_buffer, TRUE);
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

static void
working_directory_dialog_destroyed(GtkWidget *object, AppContext *app)
{
  app->working_directory_dialog = NULL;
}

static GFile *
get_list_file(AppContext *app)
{
  if (!app->working_directory) return NULL;
  return g_file_resolve_relative_path(app->working_directory,
				      SUBTITLE_LIST_FILENAME);
}

static void
save_list(AppContext *app)
{
  GError *error = NULL;
  if (app->subtitle_store || app->working_directory) {
    gboolean ret;
    GFile *save_file = get_list_file(app);
    ret = subtitle_store_io_save(app->subtitle_store, save_file, &error);
    g_object_unref(save_file);
    if (!ret) {
      show_error(app,"Failed to save subtitle list", &error);
      return;
    }
  }
}

static void
working_directory_dialog_response(GtkDialog *dialog,
		     gint response_id, AppContext *app)
{
  gtk_widget_hide(GTK_WIDGET(dialog));
  if (response_id == GTK_RESPONSE_ACCEPT) {
    GFile *load_file;
    app->working_directory=gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
    load_file = get_list_file(app);
    if (g_file_query_exists(load_file, NULL)) {
      GError *error = NULL;
      gboolean ret;
      subtitle_store_remove(app->subtitle_store, NULL);
      ret = subtitle_store_io_load(app->subtitle_store, load_file, &error);
      g_object_unref(load_file);
      if (!ret) {
	show_error(app, "Failed to save subtitle list", &error);
	return;
      }
    } else {
      g_object_unref(load_file);
    }
  }
}
  
G_MODULE_EXPORT void
save_action_activate_cb(GtkAction *action, AppContext *app)
{
  if (!app->working_directory) {
    show_error_msg(app, "No working directory set",
		   "Select a directory using the menu");
    return;
  }
  save_list(app);
}

G_MODULE_EXPORT void
set_working_directory_action_activate_cb(GtkAction *action, AppContext *app)
{
  if (!app->working_directory_dialog) {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new("Working directory",
					 GTK_WINDOW(app->main_win),
					 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					 GTK_STOCK_CANCEL,
					 GTK_RESPONSE_CANCEL,
					 GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					 NULL);
    g_signal_connect(dialog, "destroy",
		     G_CALLBACK(working_directory_dialog_destroyed), app);
    g_signal_connect(dialog, "response",
		     G_CALLBACK(working_directory_dialog_response), app);
    app->working_directory_dialog = GTK_FILE_CHOOSER_DIALOG(dialog);
  }
  gtk_widget_show(GTK_WIDGET(app->working_directory_dialog));
}

G_MODULE_EXPORT void
subtitle_selection_changed_cb(GtkTreeSelection *treeselection, AppContext *app)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GList *selected =
    gtk_tree_selection_get_selected_rows(treeselection, &model);
  if (selected) {
    gchar *text;
    gtk_tree_model_get_iter(model, &iter, selected->data);
    gtk_tree_model_get(model, &iter, SUBTITLE_STORE_COLUMN_TEXT, &text, -1);
    gtk_text_buffer_set_text(app->subtitle_text_buffer, text, -1);
    g_free(text);
    
    if (app->active_subtitle) {
      gtk_tree_path_free(app->active_subtitle);
    }
    app->active_subtitle = gtk_tree_path_copy(selected->data);
  }
  g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
}

G_MODULE_EXPORT void
play_action_activate_cb(GtkAction *action, gpointer user_data)
{
  AppContext *app = user_data;
  GError *error = NULL;
  GtkTreeIter iter;
  gchar *filename;
  GFile *file;
  if (!app->active_subtitle) return;
  if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(app->subtitle_store), &iter,
			       app->active_subtitle)) return;
  filename = subtitle_store_get_filename(app->subtitle_store, &iter);
  if (!filename) return;
  if (!app->working_directory) return;
  file = g_file_resolve_relative_path(app->working_directory, filename);
  if (!clip_recorder_play(app->recorder, file, &error)) {
    show_error(app, "Failed to start playback", &error);
  }
  g_clear_error(&error);
  g_object_unref(file);
}

static void
select_next_subtitle(AppContext *app)
{
  GtkTreeIter iter;
  GtkTreeIter child;
  GtkTreeModel *model;
  GtkTreePath *path;
  if (!gtk_tree_selection_get_selected (app->subtitle_selection,
					&model, &iter)) {
    return;
  }
  child = iter;
  if (!gtk_tree_model_iter_next(model, &iter)) {
    GtkTreeIter parent;
    if (!gtk_tree_model_iter_parent(model, &parent, &child)) return;
    if (!gtk_tree_model_iter_next(model, &parent)) return;
    if (!gtk_tree_model_iter_children(model, &iter, &parent)) return;
  }
  /* gtk_tree_selection_select_iter (app->subtitle_selection, &iter); */
  path = gtk_tree_model_get_path(model, &iter);
  gtk_tree_view_expand_to_path(app->subtitle_list_view, path);
  gtk_tree_view_set_cursor(app->subtitle_list_view, path, NULL, FALSE);
  gtk_tree_path_free(path);
}

G_MODULE_EXPORT void
stop_action_activate_cb(GtkAction *action, gpointer user_data)
{
  AppContext *app = user_data;
  GError *error = NULL;
  if (!clip_recorder_stop(app->recorder, &error)) {
    show_error(app, "Failed to stop recording/playback", &error);
  }

  if (app->record_timer) {
    g_source_remove(app->record_timer);
    app->record_timer = 0;
  }
  gtk_widget_set_sensitive(GTK_WIDGET(app->subtitle_text_view), TRUE);
  gtk_widget_set_state(GTK_WIDGET(app->subtitle_text_view), GTK_STATE_NORMAL);
}

#define CLIP_PREFIX "spot_"

static GFile *
create_clip_name(GtkTreeModel *model, GtkTreePath *path, GFile *dir)
{
  GtkTreeIter iter;
  GtkTreeIter parent;
  GFile *file;
  gchar  *spot_id;
  gchar  *reel_id;
  guint version = 1;
  if (!gtk_tree_model_get_iter(model, &iter, path)) return NULL;
  gtk_tree_model_get(model, &iter,
		     SUBTITLE_STORE_COLUMN_ID, &spot_id,
		     -1);
  while(gtk_tree_model_iter_parent(model, &parent, &iter)) {
    iter = parent;
  }
  gtk_tree_model_get(model, &iter,
		     SUBTITLE_STORE_COLUMN_ID, &reel_id,
		     -1);
  while(TRUE) {
    gchar *filename;
    filename = g_strdup_printf("%s_%s_%d.wav",
			       reel_id,spot_id, version);
    file = g_file_resolve_relative_path (dir, filename);
    g_free(filename);
    if (!g_file_query_exists(file, NULL)) break;
    g_object_unref(file);
    version++;
  }
  g_free(spot_id);
  g_free(reel_id);
  return file;
}

G_MODULE_EXPORT void
record_action_activate_cb(GtkAction *action, gpointer user_data)
{
  AppContext *app = user_data;
  GError *error = NULL;
  GFile *file;
  if (!app->working_directory) {
    show_error_msg(app, "No working directory set",
		   "Select a directory using the menu before recording");
    return;
  }
  /* Ignore reels */
  if (!app->active_subtitle 
      || gtk_tree_path_get_depth(app->active_subtitle) < 2) return;
  
  file = create_clip_name(GTK_TREE_MODEL(app->subtitle_store),
			  app->active_subtitle, app->working_directory);
  if (!file) return;
  if (!clip_recorder_record(app->recorder, file, &error)) {
    show_error(app, "Failed to start recording", &error);
  }
  g_clear_error(&error);
  app->recorded_file = file;
  gtk_widget_set_state(GTK_WIDGET(app->subtitle_text_view), GTK_STATE_ACTIVE);
}

static void
recording_cb(ClipRecorder *recorder, AppContext *app)
{
  gtk_action_group_set_sensitive(app->global_actions, FALSE);
  gtk_action_group_set_sensitive(app->subtitle_actions, FALSE);
  gtk_action_group_set_sensitive(app->record_actions, TRUE);
  g_debug("Recording");
}

static void
playing_cb(ClipRecorder *recorder, AppContext *app)
{
  gtk_action_group_set_sensitive(app->subtitle_actions, FALSE);
  gtk_action_group_set_sensitive(app->record_actions, TRUE);
  g_debug("Playing");
}

static void
stopped_cb(ClipRecorder *recorder, AppContext *app)
{
  if (app->recorded_file && app->active_subtitle) {
    GtkTreeIter iter;
    gchar *name = g_file_get_basename(app->recorded_file);
    GstClockTimeDiff duration = clip_recorder_recorded_length(app->recorder);
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(app->subtitle_store), &iter,                             app->active_subtitle)) {
      subtitle_store_set_file(app->subtitle_store, &iter, name, duration);
    }
    g_free(name);
  }
  g_clear_object(&app->recorded_file);
  gtk_action_group_set_sensitive(app->record_actions, FALSE);
  gtk_action_group_set_sensitive(app->subtitle_actions, TRUE);
  gtk_action_group_set_sensitive(app->global_actions, TRUE);
  select_next_subtitle(app);

  g_debug("Stopped");
}

static gboolean
record_timeout (gpointer user_data) {
  AppContext *app = user_data;
  gtk_widget_set_sensitive(GTK_WIDGET(app->subtitle_text_view), FALSE);
  gtk_widget_set_state(GTK_WIDGET(app->subtitle_text_view),
		       GTK_STATE_INSENSITIVE);
  return FALSE;
}

static void
record_power_cb(ClipRecorder *recorder, gdouble power , AppContext *app)
{
  /* g_debug("record_power_cb: %lf, %f, %f, %f", power, DEFAULT_SILENCE_LEVEL, DEFAULT_NORMAL_LEVEL, DEFAULT_TOP_LEVEL); */
  if (power > DEFAULT_SILENCE_LEVEL) {
    if (app->record_timer == 0) {
      if (app->active_subtitle) {
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(app->subtitle_store),
				    &iter, app->active_subtitle)) {
	  gint64 in;
	  gint64 out;
	  gtk_tree_model_get(GTK_TREE_MODEL(app->subtitle_store), &iter,
			     SUBTITLE_STORE_COLUMN_IN, &in,
			     SUBTITLE_STORE_COLUMN_OUT, &out, -1);
	  app->record_timer = g_timeout_add((out - in)/1000000,
					    record_timeout, app);
	}
      }
    }
    
  }
  gtk_widget_set_state(GTK_WIDGET(app->green_lamp),
		       ((power > DEFAULT_SILENCE_LEVEL)
			? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL));
  gtk_widget_set_state(GTK_WIDGET(app->yellow_lamp),
		       ((power > DEFAULT_NORMAL_LEVEL)
			? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL));
  gtk_widget_set_state(GTK_WIDGET(app->red_lamp),
		       ((power > DEFAULT_TOP_LEVEL)
			? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL));
}

G_MODULE_EXPORT void
expand_all_action_activate_cb(GtkAction *action, AppContext *app)
{
  gtk_tree_view_expand_all(app->subtitle_list_view);
}


G_MODULE_EXPORT void
collapse_all_action_activate_cb(GtkAction *action, AppContext *app)
{
  gtk_tree_view_collapse_all(app->subtitle_list_view);
}

G_MODULE_EXPORT void
about_action_activate_cb(GtkAction *action, AppContext *app)
{
  show_about_dialog(GTK_WINDOW(app->main_win));
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

static void file_changed(GtkCellRendererCombo *combo, gchar *path_str,
	    GtkTreeIter *new_iter, gpointer user_data)
{
  AppContext *app = user_data;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
  if (!path) return;
  if (gtk_tree_model_get_iter(GTK_TREE_MODEL(app->subtitle_store),
			      &iter,path)) {
    GtkTreeModel *files;
    gchar *new_text;
    gint64 new_duration;

    gtk_tree_model_get(GTK_TREE_MODEL(app->subtitle_store), &iter,
		       SUBTITLE_STORE_COLUMN_FILES, &files, -1);
    gtk_tree_model_get(files, new_iter,
		       SUBTITLE_STORE_FILES_COLUMN_FILE, &new_text,
		       SUBTITLE_STORE_FILES_COLUMN_DURATION, &new_duration,
		       -1);
    subtitle_store_set_file(app->subtitle_store, &iter,
			    new_text, new_duration);
  }
}

static gboolean
search_equal(GtkTreeModel *model, gint column, const gchar *key,
	     GtkTreeIter *iter, gpointer search_data)
{
  if (column == SUBTITLE_STORE_COLUMN_TEXT) {
    gchar *text;
    gchar *found;
    gchar *case_key;
    gchar *case_text;
    gtk_tree_model_get(model, iter, SUBTITLE_STORE_COLUMN_TEXT, &text, -1);
    case_text = g_utf8_casefold(text, -1);
    g_free(text);
    case_key = g_utf8_casefold(key, -1);
    found = strstr(case_text, case_key);
    g_free(case_key);
    g_free(case_text);
    return !found;
  }
  return TRUE;
}

static gboolean
setup_subtitle_list(AppContext *app, GtkBuilder *builder, GError **err)
{
  GtkCellRenderer *render;
  GtkTreeViewColumn *column;
  GtkTreeView *viewer;
  app->subtitle_store = subtitle_store_new();
 
  viewer = GTK_TREE_VIEW(FIND_OBJECT("subtitle_list"));
  if (!viewer) return FALSE;
  gtk_tree_view_set_model(viewer, GTK_TREE_MODEL(app->subtitle_store));
  gtk_tree_view_set_headers_visible(viewer, TRUE);
  /* In column */
  render = gtk_cell_renderer_time_new();
  g_object_set(render, "yalign", 0.0, NULL);
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

  /* Text column */
  render = gtk_cell_renderer_text_new();
  g_object_set(render, "yalign", 0.0, NULL);
  column =
    gtk_tree_view_column_new_with_attributes("Text", render,
					     "text", SUBTITLE_STORE_COLUMN_TEXT,
					     NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column(viewer, column);

  /* File column */
  render = gtk_cell_renderer_combo_new();
  g_object_set(render,
	       "yalign", 0.0,
	       "has-entry", FALSE,
	       "text-column",SUBTITLE_STORE_FILES_COLUMN_FILE,
	       "editable", TRUE,
	       NULL);
  g_signal_connect(render, "changed", (GCallback)file_changed, app);
  column =
    gtk_tree_view_column_new_with_attributes("Audio file", render,
					     "text", SUBTITLE_STORE_COLUMN_FILE,
					     "model",
					     SUBTITLE_STORE_COLUMN_FILES,
					     NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column(viewer, column);
  
  /* Duration */
  render = gtk_cell_renderer_time_new();
  g_object_set(render, "yalign", 0.0, NULL);
  column =
    gtk_tree_view_column_new_with_attributes("Audio duration", render,
					     "time",
					     SUBTITLE_STORE_COLUMN_FILE_DURATION,
					     "cell-background",
					     SUBTITLE_STORE_COLUMN_FILE_COLOR,
					     NULL);
  gtk_tree_view_append_column(viewer, column);
  
  gtk_tree_view_set_search_column(viewer, SUBTITLE_STORE_COLUMN_TEXT);
  gtk_tree_view_set_search_equal_func(viewer, search_equal, NULL, NULL);
  
  return TRUE;
}

static void
sync_proxies_with_action(GtkAction *action)
{
  GSList *proxies = gtk_action_get_proxies(action);
  while(proxies) {
    if (GTK_IS_ACTIVATABLE(proxies->data)) {
      gtk_activatable_sync_action_properties(GTK_ACTIVATABLE(proxies->data),
					     action);
    }
    proxies = proxies->next;
  }
}

static GtkAction *
add_action(AppContext *app, GtkBuilder *builder, GtkAccelGroup *ag,
	   GtkActionGroup *group,  const gchar *name, const gchar *accelerator,
	   GError **err)
{
 GtkAction *action = GTK_ACTION(FIND_OBJECT(name));
 if (!action) return NULL;
 gtk_action_group_add_action_with_accel(group, action, accelerator);
 gtk_action_set_accel_group(action, ag);
 gtk_action_connect_accelerator(action);
 sync_proxies_with_action(action);
 return action;
}




static gboolean
setup_actions(AppContext *app, GtkBuilder *builder, GtkAccelGroup *ag,
	      GError **err)
{
  app->global_actions = gtk_action_group_new("global_actions");
  if (!add_action(app, builder, ag, app->global_actions,
		  "quit_action", "<Control>q",err)) {
    return FALSE;
  }
  if (!add_action(app, builder, ag, app->global_actions,
		  "save_action", "<Control>s",err)) {
    return FALSE;
  }
  if (!add_action(app, builder, ag, app->global_actions,
		  "set_working_directory_action", "<Shift>s",err)) {
    return FALSE;
  }
  if (!add_action(app, builder, ag, app->global_actions,
		  "open_assetmap_action", "<Control>o",err)) {
    return FALSE;
  }
  if (!add_action(app, builder, ag, app->global_actions,
		  "expand_all_action", "<Control>e",err)) {
    return FALSE;
  }
  if (!add_action(app, builder, ag, app->global_actions,
		  "collapse_all_action", "<Shift><Control>e",err)) {
    return FALSE;
  }

  app->subtitle_actions = gtk_action_group_new("subtitle_actions");
  
  if (!(app->play_action = add_action(app, builder, ag, app->subtitle_actions,
				      "play_clip", "Return",err))) {
    return FALSE;
  }
  if (!(app->record_action = add_action(app, builder, ag, app->subtitle_actions,
					"record_clip", "space",err))) {
    return FALSE;
  }

  app->record_actions = gtk_action_group_new("record_actions");
  
  if (!(app->stop_action = add_action(app, builder, ag, app->record_actions,
				      "stop_clip", "space",err))) {
    return FALSE;
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
  app->global_actions = gtk_action_group_new("global_actions");
  
  app->main_win = GTK_WIDGET(FIND_OBJECT("subrec"));
  g_assert(app->main_win != NULL);
  g_object_ref(app->main_win);

  if (!setup_subtitle_list(app, builder, err)) {
    g_object_unref(builder);
    return FALSE;
  }
  
  app->subtitle_list_view = GTK_TREE_VIEW(FIND_OBJECT("subtitle_list"));
  g_assert(app->subtitle_list_view != NULL);

  app->subtitle_selection =gtk_tree_view_get_selection(app->subtitle_list_view);
  g_signal_connect(app->subtitle_selection, "changed",
		   (GCallback)subtitle_selection_changed_cb, app);
  app->subtitle_text_buffer = GTK_TEXT_BUFFER(FIND_OBJECT("subtitle_text"));
  g_assert(app->subtitle_text_buffer != NULL);
  g_object_ref(app->subtitle_text_buffer);
  
  app->subtitle_text_view = GTK_TEXT_VIEW(FIND_OBJECT("subtitle_textview"));
  g_assert(app->subtitle_text_view);

  app->red_lamp = GTK_IMAGE(FIND_OBJECT("red_lamp"));
  g_assert(app->red_lamp);
  app->yellow_lamp = GTK_IMAGE(FIND_OBJECT("yellow_lamp"));
  g_assert(app->red_lamp);
  app->green_lamp = GTK_IMAGE(FIND_OBJECT("green_lamp"));
  g_assert(app->green_lamp);
#if GTK_CHECK_VERSION(3,0,0)
  {
    GdkRGBA color;
    color.red = 0.5;
    color.green = 1.0;
    color.blue = 0.5;
    color.alpha = 1.0;
    gtk_widget_override_background_color(GTK_WIDGET(app->subtitle_text_view),
					 GTK_STATE_FLAG_ACTIVE,
					 &color);
    color.red = 1.0;
    color.green = 1.0;
    color.blue = 0.5;
    gtk_widget_override_background_color(GTK_WIDGET(app->subtitle_text_view),
					 GTK_STATE_FLAG_INSENSITIVE,
					 &color);
  }
#else
  {
    GdkColor color;
    GdkColormap *colormap = gdk_colormap_get_system();
    color.red = 32768;
    color.green = 65535;
    color.blue = 32768;
    gdk_colormap_alloc_color(colormap, &color, FALSE, TRUE);
    g_object_unref(colormap);
    gtk_widget_modify_base(GTK_WIDGET(app->subtitle_text_view), GTK_STATE_ACTIVE,
			   &color);
    color.red = 65535;
    color.green = 65535;
    color.blue = 32768;
    gdk_colormap_alloc_color(colormap, &color, FALSE, TRUE);
    g_object_unref(colormap);
    gtk_widget_modify_base(GTK_WIDGET(app->subtitle_text_view), GTK_STATE_INSENSITIVE,
			   &color);
    
  }
#endif

  {
    GtkAccelGroup *ag = gtk_accel_group_new();
    if (!setup_actions(app, builder, ag, err)) {
      g_object_unref(builder);
      g_object_unref(ag);
      return FALSE;
    }
    gtk_window_add_accel_group (GTK_WINDOW (app->main_win), ag);
    g_object_unref(ag);
  }

  
 

  
  gtk_builder_connect_signals(builder, app);
 

  g_object_unref(builder);
  return TRUE;
}

static gboolean
setup_recorder(AppContext *app, GError **error)
{
  app->recorder = clip_recorder_new();
  g_object_set(app->recorder, "trim-level", DEFAULT_SILENCE_LEVEL, NULL);
  g_signal_connect(app->recorder, "recording", (GCallback)recording_cb, app);
  g_signal_connect(app->recorder, "playing", (GCallback)playing_cb, app);
  g_signal_connect(app->recorder, "stopped", (GCallback)stopped_cb, app);
  g_signal_connect(app->recorder, "power", (GCallback)record_power_cb, app);
  return TRUE;
}

#define USE_STYLE 0
#if USE_STYLE
static void
setup_style(AppContext *app)
{
  static const gchar style_str[] =
    " GtkTreeView * {\n"
    " font: Sans 20;\n"
    " background-color: #890000;\n"
    " color: #ff00ea;\n"
    " }\n";
  GtkStyleContext *context;
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider, style_str, -1, NULL);
  context = gtk_widget_get_style_context(app->main_win); 
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), 
				 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}

static void
print_widget_tree(GtkWidget *widget, guint indent)
{
  guint i;
  for (i = 0; i < indent; i++) putchar(' ');
  printf("Class: '%s' Name '%s'\n", G_OBJECT_TYPE_NAME(widget), gtk_widget_get_name(widget));
  if (GTK_IS_CONTAINER(widget)) {
    GList *child = gtk_container_get_children(GTK_CONTAINER(widget));
    while(child) {
      print_widget_tree(child->data, indent + 2);
      child = child->next;
    }
  }
}
#endif

int
main(int argc, char **argv)
{
  GError *error = NULL;
  AppContext app;
  LIBXML_TEST_VERSION
  app_init(&app);
  gst_init(&argc, &argv);
  gtk_init(&argc, &argv);
  if (!setup_recorder(&app, &error)) {
    app_destroy(&app);
    g_error("%s", error->message);
  }
  if (!create_main_window(&app, &error)) {
    app_destroy(&app);
    g_error("%s", error->message);
  }

#if USE_STYLE
  print_widget_tree(app.main_win, 0);
  setup_style(&app);
#endif
  gtk_widget_show(app.main_win);
  gtk_main();
  g_debug("Exiting");
  app_destroy(&app);
  return EXIT_SUCCESS;
}
