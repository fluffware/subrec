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
#include <preferences_dialog.h>
#include <preferences.h>
#include <save_sequence.h>
#include <string.h>
#include <math.h>
#include <glib/gi18n.h>

#define SUBTITLE_LIST_FILENAME "SUBTITLES.xml"
#define SUBREC_ERROR (subrec_error_quark())
enum {
  SUBREC_ERROR_NO_WORK_DIR = 1,
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





#define dB(x) (powf(10.0,((x)/10.0)))
#define DEFAULT_TOP_LEVEL dB(-3)

typedef struct
{
  GtkApplication *application;
  GSettings *settings;
  GMenuModel *app_menu;
  GMenuModel *win_menu;
  GtkFileChooserDialog *working_directory_dialog;  
} AppContext;

static void
app_init(AppContext *app)
{
  app->application = NULL;
  app->settings = NULL;
  app->app_menu = NULL;
  app->win_menu = NULL;
  app->working_directory_dialog = NULL;
}

static void
app_destroy(AppContext *app)
{
  g_clear_object(&app->working_directory_dialog);
  destroy_preferences_dialog();
  g_clear_object(&app->settings);
  g_clear_object(&app->app_menu);
  g_clear_object(&app->win_menu);
}

typedef struct 
{
  AppContext *app_ctxt;
  GtkWidget *main_win;
  GtkMessageDialog *error_dialog;
  GtkFileChooserDialog *load_dialog;
  GtkFileChooserDialog *save_sequence_dialog;
  GtkDialog *save_sequence_progress;
  GtkProgressBar *save_sequence_progress_bar;
  guint save_sequence_progress_timer;
  GAction *play_action;
  GAction *record_action;
  GAction *stop_action;
  GActionGroup *app_actions;
  GActionGroup *instance_actions;
  GActionGroup *subtitle_actions;
  GActionGroup *record_actions;
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
  double normal_level;
  GFile *working_directory;
  SaveSequence *save_sequence;
} InstanceContext;

static InstanceContext *
instance_create(AppContext *app_ctxt)
{
  InstanceContext *inst = g_new(InstanceContext, 1);
  inst->app_ctxt = app_ctxt;
  inst->main_win = NULL;
  inst->error_dialog = NULL;
  inst->load_dialog = NULL;
  inst->save_sequence_dialog = NULL;
  inst->save_sequence_progress = NULL;
  inst->save_sequence_progress_timer = 0;
  inst->asset_map = NULL;
  inst->packing_list = NULL;
  inst->cpl = NULL;
  inst->subtitle_store = NULL;
  inst->active_subtitle = NULL;
  inst->subtitle_list_view = NULL;
  inst->subtitle_selection = NULL;
  inst->subtitle_text_buffer = NULL;
  inst->subtitle_text_view = NULL;
  inst->recorder = NULL;
  inst->record_timer = 0;
  inst->recorded_file = NULL;
  inst->working_directory = NULL;
  inst->save_sequence = NULL;
  inst->app_actions = NULL;
  inst->instance_actions = NULL;
  inst->subtitle_actions = NULL;
  inst->record_actions = NULL;
  return inst;
}

static void
instance_free(InstanceContext *inst)
{
  if (inst->record_timer != 0) {
    g_source_remove(inst->record_timer);
    inst->record_timer = 0;
  }
  if (inst->save_sequence_progress_timer != 0) {
    g_source_remove(inst->save_sequence_progress_timer);
    inst->save_sequence_progress_timer = 0;
  }
  g_clear_object(&inst->main_win);
  g_clear_object(&inst->app_actions);
  g_clear_object(&inst->instance_actions);
  g_clear_object(&inst->subtitle_actions);
  g_clear_object(&inst->record_actions);
  g_clear_object(&inst->asset_map);
  g_clear_object(&inst->packing_list);
  g_clear_object(&inst->cpl);
  g_clear_object(&inst->subtitle_store);
  if (inst->active_subtitle) {
    gtk_tree_path_free(inst->active_subtitle);
    inst->active_subtitle = NULL;
  }
  g_clear_object(&inst->subtitle_text_buffer);
  g_clear_object(&inst->recorder);
  g_clear_object(&inst->save_sequence);
  g_clear_object(&inst->recorded_file);
  g_clear_object(&inst->working_directory);
  g_free(inst);
  g_debug("Instance freed");
}


static void
error_dialog_destroyed(GtkWidget *object, InstanceContext *inst)
{
  inst->error_dialog = NULL;
}

static void
error_dialog_response(GtkDialog *dialog,
		      gint response_id, InstanceContext *inst)
{
  gtk_widget_hide(GTK_WIDGET(dialog));
}

static GtkMessageDialog *
get_message_dialog(InstanceContext *inst)
{
  if (!inst->error_dialog) {
    GtkWidget *dialog =
      gtk_message_dialog_new(GTK_WINDOW(inst->main_win),
			     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			     GTK_MESSAGE_ERROR,
			     GTK_BUTTONS_OK,
			     "Error");
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    g_signal_connect(dialog, "destroy",
		    G_CALLBACK(error_dialog_destroyed), inst);
    g_signal_connect(dialog, "response",
		     G_CALLBACK(error_dialog_response), inst);
    inst->error_dialog =GTK_MESSAGE_DIALOG(dialog);
  }
  return inst->error_dialog;
}

/* error is cleared */
static void
show_error(InstanceContext *inst, const gchar *title, GError **error)
{
  if (!get_message_dialog(inst)) {
    g_warning("%s: %s", title, (*error)->message);
  } else {
    g_object_set(inst->error_dialog, "text", title, NULL); 
  
    gtk_message_dialog_format_secondary_text(inst->error_dialog,
					     "%s", (*error)->message);
    g_clear_error(error);
    gtk_widget_show(GTK_WIDGET(inst->error_dialog));
  }
}

static void
show_error_msg(InstanceContext *inst, const gchar *title, const char *msg)
{
  if (!get_message_dialog(inst)) {
    g_debug("%s: %s", title, msg);
  } else {
    g_object_set(inst->error_dialog, "text", title, NULL); 
    gtk_message_dialog_format_secondary_text(inst->error_dialog,
					     "%s", msg);
    gtk_widget_show(GTK_WIDGET(inst->error_dialog));
  }
}


static void
load_dialog_destroyed(GtkWidget *object, InstanceContext *inst)
{
  inst->load_dialog = NULL;
}

static GFile *
get_cpl(InstanceContext *inst)
{
  const PackingListAsset **cpl_assets;
  GFile *file = NULL;
  cpl_assets = packing_list_find_asset_with_type(inst->packing_list,
						"text/xml;asdcpKind=CPL");
  if (cpl_assets && cpl_assets[0]) {
    file = asset_map_get_file(inst->asset_map,  cpl_assets[0]->id);
  }
  g_free(cpl_assets);
  return file;
}

static void
load_dialog_response(GtkDialog *dialog,
		     gint response_id, InstanceContext *inst)
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
    g_clear_object(&inst->asset_map);
    inst->asset_map = asset_map_read(file, &error);
    g_object_unref(file);
    if (!inst->asset_map) {
      show_error(inst, "Failed to load asset map", &error);
      return;
    }
    
    g_object_get(inst->asset_map, "packing-list", &file, NULL);
    uri = g_file_get_uri(file);
    g_debug("Packing list: %s", uri);
    g_free(uri);
    g_clear_object(&inst->packing_list);
    inst->packing_list = packing_list_read(file, &error);
    g_object_unref(file);
    if (!inst->packing_list) {
      show_error(inst, "Failed to load packing list", &error);
      return;
    }

    file = get_cpl(inst);
    if (!file) {
      show_error_msg(inst,"No CPL found" , "No CPL found in assetmap");
      return;
    }

    g_clear_object(&inst->cpl);
    inst->cpl = composition_playlist_read(file, &error);
    g_object_unref(file);
    if (!inst->cpl) {
      show_error(inst, "Failed to load composition playlist", &error);
      return;
    }
    subtitle_store_remove(inst->subtitle_store, NULL);
    {
      gint64 reel_pos = 0;
      guint reel_number = 1;
      GtkTreeIter reel_iter;
      GList *reels = composition_playlist_get_reels(inst->cpl);
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
	    GFile *file = asset_map_get_file(inst->asset_map, asset->id);
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
	    duration = (asset->duration * asset->edit_rate.denom * 1000000000LL
			+ asset->edit_rate.num / 2) / asset->edit_rate.num;
	    subtitle_store_insert(inst->subtitle_store,
				  reel_pos, reel_pos + duration,
				  reel_id,
				  0, NULL, &reel_iter);
	    reel_pos += duration;
	    reel_number++;
	    
	    sub = dcsubtitle_read(file, &error);
	    
	    g_object_unref(file);

	    if (!sub) {
	      show_error(inst, "Failed to load subtitle", &error);
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
	      subtitle_store_insert(inst->subtitle_store,
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
	      subtitle_store_set_text(inst->subtitle_store, &spot_iter,
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


static void
activate_close (GSimpleAction *simple,
               GVariant      *parameter,
               gpointer user_data)
{
  InstanceContext *inst = user_data;
  gtk_window_close(GTK_WINDOW(inst->main_win));
}

static void
activate_quit (GSimpleAction *simple,
               GVariant      *parameter,
               gpointer user_data)
{
  AppContext *app_ctxt = user_data;
  g_application_quit(G_APPLICATION(app_ctxt->application));
}


static void
activate_import_assetmap(GSimpleAction *action,
		       GVariant      *parameter,
		       gpointer user_data)
{
   InstanceContext *inst = user_data;
  if (!inst->load_dialog) {
    inst->load_dialog =
      GTK_FILE_CHOOSER_DIALOG(
			      gtk_file_chooser_dialog_new("Load ASSETMAP",
							  GTK_WINDOW(inst->main_win),
							  GTK_FILE_CHOOSER_ACTION_OPEN,
							  _("Cancel"), GTK_RESPONSE_CANCEL,
							  _("Open"), GTK_RESPONSE_ACCEPT,
							  NULL));
    g_signal_connect(inst->load_dialog, "destroy",
		     G_CALLBACK(load_dialog_destroyed), inst);
    g_signal_connect(inst->load_dialog, "response",
		     G_CALLBACK(load_dialog_response), inst);
    
  }
  gtk_widget_show(GTK_WIDGET(inst->load_dialog));
}

static void
working_directory_dialog_destroyed(GtkWidget *object, AppContext *app_ctxt)
{
  app_ctxt->working_directory_dialog = NULL;
}

static GFile *
get_list_file(InstanceContext *inst)
{
  if (!inst->working_directory) return NULL;
  return g_file_get_child(inst->working_directory,
			  SUBTITLE_LIST_FILENAME);
}

static void
save_list(InstanceContext *inst)
{
  GError *error = NULL;
  if (inst->subtitle_store || inst->working_directory) {
    gboolean ret;
    GFile *save_file = get_list_file(inst);
    ret = subtitle_store_io_save(inst->subtitle_store, save_file, &error);
    g_object_unref(save_file);
    if (!ret) {
      show_error(inst,"Failed to save subtitle list", &error);
      return;
    }
  }
}

static gboolean
set_working_directory(InstanceContext *inst, GFile *wd, GError **err)
{
  GFile *subtitle_file;
  gboolean ret = TRUE;
  GFile *file;
  if (!g_file_query_exists(wd, NULL)) {
    g_set_error(err, SUBREC_ERROR, SUBREC_ERROR_NO_WORK_DIR,
		"Working directory not found");
    return FALSE;
  }
  inst->working_directory = wd;
  g_object_ref(inst->working_directory);
  file = get_list_file(inst);
  subtitle_store_remove(inst->subtitle_store, NULL);
  subtitle_file = g_file_get_child (wd, "SUBTITLES.xml");
  if (g_file_query_exists(subtitle_file, NULL)) {
    ret = subtitle_store_io_load(inst->subtitle_store, file, err);
    g_object_unref(file);
    if (!ret)  {
      inst->working_directory = NULL;
    }
  }
  g_object_unref(subtitle_file);
  return ret;
}

static void
working_directory_dialog_response(GtkDialog *dialog,
		     gint response_id, AppContext *app_ctxt)
{
  gtk_widget_hide(GTK_WIDGET(dialog));
  if (response_id == GTK_RESPONSE_ACCEPT) {
    GFile *wd;
    wd = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));

    g_application_open(G_APPLICATION(app_ctxt->application), &wd, 1, "");
    g_object_unref(wd);
  }
}

static void
activate_save(GSimpleAction *action,
		      GVariant      *parameter,
		      gpointer user_data)
{
  InstanceContext *inst = user_data;
  if (!inst->working_directory) {
    show_error_msg(inst, "No working directory set",
		   "Select a directory using the menu");
    return;
  }
  save_list(inst);
}

static void
export_dialog_destroy(GtkWidget *widget, InstanceContext *inst)
{
  inst->save_sequence_dialog = NULL;
}


void
save_sequence_progress_dialog_response_cb(GtkDialog *dialog,
					  gint response_id, InstanceContext *inst)
{
  
}

static void
save_sequence_progress_stop(InstanceContext *inst)
{
  gtk_widget_hide(GTK_WIDGET(inst->save_sequence_progress));
  if (inst->save_sequence_progress_timer != 0) {
    g_source_remove(inst->save_sequence_progress_timer);
    inst->save_sequence_progress_timer = 0;
  }
}


static void
save_sequence_run_error_cb(SaveSequence *sseq, GError *err, InstanceContext *inst)
{
  save_sequence_progress_stop(inst);
  show_error_msg(inst, "Save sequence error", err->message);
}

static void
save_sequence_done_cb(SaveSequence *sseq, InstanceContext *inst)
{
  save_sequence_progress_stop(inst);
}

static gboolean
save_sequence_progress_timeout(gpointer user_data) {
  InstanceContext *inst = user_data;
  gdouble p = save_sequence_progress(inst->save_sequence);
  gtk_progress_bar_set_fraction(inst->save_sequence_progress_bar, p);
  return TRUE;
}

static void
export_dialog_response(GtkDialog *dialog, gint response_id, InstanceContext *inst)
{
  GtkTreeIter iter;
  GFile *save_file;
  GError *err = NULL;
  GstClockTime out;

  if (response_id != GTK_RESPONSE_ACCEPT) return;
  if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL(inst->subtitle_store),
				     &iter)) {
    GtkTreeIter last = iter;
    
    save_file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(dialog));
					  
    if (!inst->save_sequence) {
      inst->save_sequence = save_sequence_new(&err);
      if (!inst->save_sequence) {
	show_error(inst, "Failed create sequence saving object", &err);
	return;
      }
      g_signal_connect(inst->save_sequence, "run-error",
		       G_CALLBACK(save_sequence_run_error_cb), inst);
      g_signal_connect(inst->save_sequence, "done",
		       G_CALLBACK(save_sequence_done_cb), inst);
    }
    while(gtk_tree_model_iter_next(GTK_TREE_MODEL(inst->subtitle_store),
				   &iter)) {
      last = iter;
    }
    gtk_tree_model_get(GTK_TREE_MODEL(inst->subtitle_store), &last,
		       SUBTITLE_STORE_COLUMN_GLOBAL_OUT, &out, -1);

    
    if (!save_sequence(inst->save_sequence, save_file,
		       inst->subtitle_store, inst->working_directory,
		       0, out,
		       &err)) {
      g_object_unref(save_file);
      show_error(inst, "Failed to save audio sequence", &err);
      return;
    }
    g_object_unref(save_file);
  }
  gtk_widget_hide(GTK_WIDGET(dialog));
  gtk_widget_show(GTK_WIDGET(inst->save_sequence_progress));
  inst->save_sequence_progress_timer =
    g_timeout_add(100, save_sequence_progress_timeout, inst);
}

G_MODULE_EXPORT void
export_audio_action_activate_cb(GtkAction *action, InstanceContext *inst)
{
  GtkTreeIter iter;
  if (!inst->working_directory) {
    show_error_msg(inst, "No working directory set",
		   "Select a directory using the menu");
    return;
  }
  if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL(inst->subtitle_store),
				      &iter)) {
    show_error_msg(inst, "Empty list", "No recordings in list");
  }
  if (!inst->save_sequence_dialog) {
    GtkWidget *save_dialog =
      gtk_file_chooser_dialog_new("Export audio sequence",
				  GTK_WINDOW(inst->main_win),
				  GTK_FILE_CHOOSER_ACTION_SAVE,
				  _("Cancel"), GTK_RESPONSE_CANCEL,
				  _("Save"), GTK_RESPONSE_ACCEPT,
				  NULL);
    g_signal_connect(save_dialog, "response",
		     G_CALLBACK(export_dialog_response), inst);
    g_signal_connect(save_dialog, "destroy",
		     G_CALLBACK(export_dialog_destroy), inst);
    inst->save_sequence_dialog = GTK_FILE_CHOOSER_DIALOG(save_dialog);
  }
  gtk_widget_show(GTK_WIDGET(inst->save_sequence_dialog));
}

G_MODULE_EXPORT void
subtitle_selection_changed_cb(GtkTreeSelection *treeselection, InstanceContext *inst)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GList *selected =
    gtk_tree_selection_get_selected_rows(treeselection, &model);
  if (selected) {
    gchar *text;
    gtk_tree_model_get_iter(model, &iter, selected->data);
    gtk_tree_model_get(model, &iter, SUBTITLE_STORE_COLUMN_TEXT, &text, -1);
    gtk_text_buffer_set_text(inst->subtitle_text_buffer, text, -1);
    g_free(text);
    
    if (inst->active_subtitle) {
      gtk_tree_path_free(inst->active_subtitle);
    }
    inst->active_subtitle = gtk_tree_path_copy(selected->data);
  }
  g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);
}

static void
activate_play(GSimpleAction *action,
	      GVariant      *parameter,
	      gpointer user_data)
{
  InstanceContext *inst = user_data;
  GError *error = NULL;
  GtkTreeIter iter;
  const gchar *filename;
  GFile *file;
  if (!inst->active_subtitle) return;
  if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(inst->subtitle_store), &iter,
			       inst->active_subtitle)) return;
  filename = subtitle_store_get_filename(inst->subtitle_store, &iter);
  if (!filename) return;
  if (!inst->working_directory) return;
  file = g_file_get_child(inst->working_directory, filename);
  if (!clip_recorder_play(inst->recorder, file, &error)) {
    show_error(inst, "Failed to start playback", &error);
  }
  g_clear_error(&error);
  g_object_unref(file);
}

static void
select_next_subtitle(InstanceContext *inst)
{
  GtkTreeIter iter;
  GtkTreeIter child;
  GtkTreeModel *model;
  GtkTreePath *path;
  if (!gtk_tree_selection_get_selected (inst->subtitle_selection,
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
  /* gtk_tree_selection_select_iter (inst->subtitle_selection, &iter); */
  path = gtk_tree_model_get_path(model, &iter);
  gtk_tree_view_expand_to_path(inst->subtitle_list_view, path);
  gtk_tree_view_set_cursor(inst->subtitle_list_view, path, NULL, FALSE);
  gtk_tree_path_free(path);
}

static void
activate_stop(GSimpleAction *action,
	      GVariant      *parameter,
	      gpointer user_data)
{
  InstanceContext *inst = user_data;
  GError *error = NULL;
  if (!clip_recorder_stop(inst->recorder, &error)) {
    show_error(inst, "Failed to stop recording/playback", &error);
  }

  if (inst->record_timer) {
    g_source_remove(inst->record_timer);
    inst->record_timer = 0;
  }
  gtk_widget_set_sensitive(GTK_WIDGET(inst->subtitle_text_view), TRUE);
  gtk_widget_unset_state_flags(GTK_WIDGET(inst->subtitle_text_view), GTK_STATE_FLAG_ACTIVE);
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

static void
activate_record(GSimpleAction *action,
		GVariant      *parameter,
		gpointer user_data)
{
  InstanceContext *inst = user_data;
  GError *error = NULL;
  GFile *file;
  if (!inst->working_directory) {
    show_error_msg(inst, "No working directory set",
		   "Select a directory using the menu before recording");
    return;
  }
  /* Ignore reels */
  if (!inst->active_subtitle 
      || gtk_tree_path_get_depth(inst->active_subtitle) < 2) return;
  
  file = create_clip_name(GTK_TREE_MODEL(inst->subtitle_store),
			  inst->active_subtitle, inst->working_directory);
  if (!file) return;
  if (!clip_recorder_record(inst->recorder, file, &error)) {
    show_error(inst, "Failed to start recording", &error);
  }
  g_clear_error(&error);
  inst->normal_level =  g_settings_get_double(inst->app_ctxt->settings, PREF_NORMAL_LEVEL);
  inst->recorded_file = file;
  gtk_widget_set_state_flags(GTK_WIDGET(inst->subtitle_text_view),
			     GTK_STATE_FLAG_ACTIVE, TRUE);
}
static void
action_group_set_enable(GActionGroup *group, gboolean enable)
{
  gchar **names;
  gchar **n;
  g_return_if_fail(G_IS_ACTION_MAP(group));
  names = g_action_group_list_actions (group);
  n = names;
  while(*n) {
    GAction *action = g_action_map_lookup_action (G_ACTION_MAP(group), *n);
    if (G_IS_SIMPLE_ACTION(action)) {
      g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enable);
    }
    n++;
  }
  g_strfreev (names);
}

static void
recording_cb(ClipRecorder *recorder, InstanceContext *inst)
{
  action_group_set_enable(inst->instance_actions, FALSE);
  action_group_set_enable(inst->subtitle_actions, FALSE);
  action_group_set_enable(inst->record_actions, TRUE);
  g_debug("Recording");
}


static void
playing_cb(ClipRecorder *recorder, InstanceContext *inst)
{
  action_group_set_enable(inst->instance_actions, FALSE);
  action_group_set_enable(inst->subtitle_actions, FALSE);
  action_group_set_enable(inst->record_actions, TRUE);
  g_debug("Playing");
}

static void
stopped_cb(ClipRecorder *recorder, InstanceContext *inst)
{
  if (inst->recorded_file && inst->active_subtitle) {
    GtkTreeIter iter;
    gchar *name = g_file_get_basename(inst->recorded_file);
    GstClockTimeDiff duration = clip_recorder_recorded_length(inst->recorder);
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(inst->subtitle_store), &iter,                             inst->active_subtitle)) {
      subtitle_store_set_file(inst->subtitle_store, &iter, name, duration);
    }
    g_free(name);
  }
  g_clear_object(&inst->recorded_file);
  action_group_set_enable(inst->instance_actions, TRUE);
  action_group_set_enable(inst->subtitle_actions, TRUE);
  action_group_set_enable(inst->record_actions, FALSE);
  select_next_subtitle(inst);

  g_debug("Stopped");
}

static void
run_error_cb(ClipRecorder *recorder, GError *err, InstanceContext *inst)
{
  g_clear_object(&inst->recorded_file);
  action_group_set_enable(inst->instance_actions, TRUE);
  action_group_set_enable(inst->subtitle_actions, TRUE);
  action_group_set_enable(inst->record_actions, FALSE);
  show_error_msg(inst, "Audio pipeline error", err->message);
}

static gboolean
record_timeout (gpointer user_data) {
  InstanceContext *inst = user_data;
  inst->record_timer = 0;
  gtk_widget_set_sensitive(GTK_WIDGET(inst->subtitle_text_view), FALSE);
  return FALSE;
}

static void
record_power_cb(ClipRecorder *recorder, gdouble power , InstanceContext *inst)
{
  /* g_debug("record_power_cb: %lf, %f, %f, %f", power, DEFAULT_SILENCE_LEVEL, DEFAULT_NORMAL_LEVEL, DEFAULT_TOP_LEVEL); */
  if (power > clip_recorder_get_trim_level(recorder)) {
    if (inst->record_timer == 0) {
      if (inst->active_subtitle) {
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(inst->subtitle_store),
				    &iter, inst->active_subtitle)) {
	  gint64 in;
	  gint64 out;
	  gtk_tree_model_get(GTK_TREE_MODEL(inst->subtitle_store), &iter,
			     SUBTITLE_STORE_COLUMN_IN, &in,
			     SUBTITLE_STORE_COLUMN_OUT, &out, -1);
	  inst->record_timer = g_timeout_add((out - in)/1000000,
					    record_timeout, inst);
	}
      }
    }
    
  }
  if (power > clip_recorder_get_trim_level(recorder)) {
    gtk_widget_set_state_flags(GTK_WIDGET(inst->green_lamp),
			       GTK_STATE_FLAG_PRELIGHT, FALSE);
  } else {
    gtk_widget_unset_state_flags(GTK_WIDGET(inst->green_lamp),
				 GTK_STATE_FLAG_PRELIGHT);
  }
  if (power > inst->normal_level) {
    gtk_widget_set_state_flags(GTK_WIDGET(inst->yellow_lamp),
			       GTK_STATE_FLAG_PRELIGHT, FALSE);
  } else {
    gtk_widget_unset_state_flags(GTK_WIDGET(inst->yellow_lamp),
				 GTK_STATE_FLAG_PRELIGHT);
  }
  if (power > DEFAULT_TOP_LEVEL) {
    gtk_widget_set_state_flags(GTK_WIDGET(inst->red_lamp),
			       GTK_STATE_FLAG_PRELIGHT, FALSE);
  } else {
    gtk_widget_unset_state_flags(GTK_WIDGET(inst->red_lamp),
				 GTK_STATE_FLAG_PRELIGHT);
  }
}

static void
activate_new_working_directory(GSimpleAction *action,
			       GVariant      *parameter,
			       gpointer user_data)
{
  /* AppContext *app_ctxt = user_data; */
}

static void
activate_open_working_directory(GSimpleAction *action,
			       GVariant      *parameter,
			       gpointer user_data)
{
  AppContext *app_ctxt = user_data;
  if (!app_ctxt->working_directory_dialog) {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new("Working directory",
					 NULL,
					 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					 _("Cancel"),
					 GTK_RESPONSE_CANCEL,
					 _("OK"), GTK_RESPONSE_ACCEPT,
					 NULL);
    g_object_ref_sink(dialog);
    g_signal_connect(dialog, "destroy",
		     G_CALLBACK(working_directory_dialog_destroyed), app_ctxt);
    g_signal_connect(dialog, "response",
		     G_CALLBACK(working_directory_dialog_response), app_ctxt);
    app_ctxt->working_directory_dialog = GTK_FILE_CHOOSER_DIALOG(dialog);
  }
  gtk_widget_show(GTK_WIDGET(app_ctxt->working_directory_dialog));
}



static void
activate_expand_all(GSimpleAction *action,
		     GVariant      *parameter,
		     gpointer user_data)
{
  InstanceContext *inst = user_data;
  gtk_tree_view_expand_all(inst->subtitle_list_view);
}

static void
activate_collapse_all(GSimpleAction *action,
		     GVariant      *parameter,
		     gpointer user_data)
{
  InstanceContext *inst = user_data;
  gtk_tree_view_collapse_all(inst->subtitle_list_view);
}

static void
activate_about(GSimpleAction *simple,
	       GVariant      *parameter,
	       gpointer user_data)
{
  InstanceContext *inst = user_data;
  show_about_dialog(GTK_WINDOW(inst->main_win));
}

static void
activate_preferences(GSimpleAction *simple,
		     GVariant      *parameter,
		     gpointer user_data)
{
  show_preferences_dialog(NULL);
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
  InstanceContext *inst = user_data;
  GtkTreeIter iter;
  GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
  if (!path) return;
  if (gtk_tree_model_get_iter(GTK_TREE_MODEL(inst->subtitle_store),
			      &iter,path)) {
    GtkTreeModel *files;
    gchar *new_text;
    gint64 new_duration;

    gtk_tree_model_get(GTK_TREE_MODEL(inst->subtitle_store), &iter,
		       SUBTITLE_STORE_COLUMN_FILES, &files, -1);
    gtk_tree_model_get(files, new_iter,
		       SUBTITLE_STORE_FILES_COLUMN_FILE, &new_text,
		       SUBTITLE_STORE_FILES_COLUMN_DURATION, &new_duration,
		       -1);
    subtitle_store_set_file(inst->subtitle_store, &iter,
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
setup_subtitle_list(InstanceContext *inst,   GtkBuilder *builder,
		    GError **err)
{
  GtkCellRenderer *render;
  GtkTreeViewColumn *column;
  GtkTreeView *viewer;
  inst->subtitle_store = subtitle_store_new();
 
  viewer = GTK_TREE_VIEW(FIND_OBJECT("subtitle_list"));
  if (!viewer) return FALSE;
  gtk_tree_view_set_model(viewer, GTK_TREE_MODEL(inst->subtitle_store));
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
  g_signal_connect(render, "changed", (GCallback)file_changed, inst);
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





static gboolean
setup_actions(InstanceContext *inst, GError **err)
{
 
  const GActionEntry instance_actions[] = {
    { "save", activate_save, NULL},
    { "close", activate_close, NULL},
    { "import-assetmap", activate_import_assetmap, NULL},
    { "expand-all", activate_expand_all, NULL},
    { "collapse-all", activate_collapse_all, NULL},
    { "about", activate_about, NULL},
    
  };
  
  const GActionEntry subtitle_actions[] = {
    { "play", activate_play, NULL},
    { "record", activate_record, NULL},
  };
  
  const GActionEntry record_actions[] = {
    { "stop", activate_stop, NULL},
  };
  
  g_action_map_add_action_entries (G_ACTION_MAP (inst->main_win),
				   instance_actions,
				   G_N_ELEMENTS(instance_actions),
				   inst);

  inst->instance_actions = G_ACTION_GROUP (inst->main_win);
  g_object_ref(inst->instance_actions);

  inst->subtitle_actions = G_ACTION_GROUP(g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP(inst->subtitle_actions),
				   subtitle_actions,
				   G_N_ELEMENTS(subtitle_actions),
				   inst);
  gtk_widget_insert_action_group (GTK_WIDGET(inst->main_win), "sub",
				  inst->subtitle_actions);
  
  inst->record_actions = G_ACTION_GROUP(g_simple_action_group_new ());
  g_action_map_add_action_entries (G_ACTION_MAP (inst->record_actions),
				   record_actions,
				   G_N_ELEMENTS(record_actions),
				   inst);
  
  gtk_widget_insert_action_group (GTK_WIDGET(inst->main_win), "rec",
				  inst->record_actions);
  
  /*
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
  */
  return TRUE;
}


static void
main_win_destroyed(GtkWidget *object,
		   InstanceContext *inst)
{
  instance_free(inst);
}

static gboolean
create_main_window(InstanceContext *inst, GtkApplication *application,
		   GError **err)
{
  
  GtkBuilder *builder = gtk_builder_new();
  if (!builderutils_add_from_file(builder, "main_win.ui", err)) {
    g_object_unref(builder);
    return FALSE;
  }

  
  inst->main_win = GTK_WIDGET(FIND_OBJECT("subrec"));
  g_assert(inst->main_win != NULL);
  g_object_ref_sink(inst->main_win);
  gtk_application_add_window(application, GTK_WINDOW(inst->main_win));
 
  if (!setup_actions(inst, err)) { 
    g_object_unref(builder);
    return FALSE; 
  }
  gtk_widget_show(inst->main_win);
  g_signal_connect(inst->main_win, "destroy", G_CALLBACK(main_win_destroyed), inst);
  gtk_application_window_set_show_menubar(GTK_APPLICATION_WINDOW(inst->main_win), TRUE); 
  
  
  if (!setup_subtitle_list(inst, builder, err)) {
    g_object_unref(builder);
     return FALSE;
   }
  
  inst->subtitle_list_view = GTK_TREE_VIEW(FIND_OBJECT("subtitle_list"));
  g_assert(inst->subtitle_list_view != NULL);

  inst->subtitle_selection =gtk_tree_view_get_selection(inst->subtitle_list_view);
  g_signal_connect(inst->subtitle_selection, "changed",
  		   (GCallback)subtitle_selection_changed_cb, inst);
  inst->subtitle_text_buffer = GTK_TEXT_BUFFER(FIND_OBJECT("subtitle_text"));
  g_assert(inst->subtitle_text_buffer != NULL);
  g_object_ref(inst->subtitle_text_buffer);
  
  inst->subtitle_text_view = GTK_TEXT_VIEW(FIND_OBJECT("subtitle_textview"));
  g_assert(inst->subtitle_text_view);

  inst->red_lamp = GTK_IMAGE(FIND_OBJECT("red_lamp"));
  g_assert(inst->red_lamp);
  inst->yellow_lamp = GTK_IMAGE(FIND_OBJECT("yellow_lamp"));
  g_assert(inst->red_lamp);
  inst->green_lamp = GTK_IMAGE(FIND_OBJECT("green_lamp"));
  g_assert(inst->green_lamp);

  /* { */
  /*   GdkRGBA color; */
  /*   color.red = 0.5; */
  /*   color.green = 1.0; */
  /*   color.blue = 0.5; */
  /*   color.alpha = 1.0; */
  /*   gtk_widget_override_background_color(GTK_WIDGET(inst->subtitle_text_view), */
  /* 					 GTK_STATE_FLAG_ACTIVE, */
  /* 					 &color); */
  /*   color.red = 1.0; */
  /*   color.green = 1.0; */
  /*   color.blue = 0.5; */
  /*   gtk_widget_override_background_color(GTK_WIDGET(inst->subtitle_text_view), */
  /* 					 GTK_STATE_FLAG_INSENSITIVE, */
  /* 					 &color); */
  /* } */

  /* { */
  /*   GtkAccelGroup *ag = gtk_accel_group_new(); */
 
  /*   gtk_window_add_accel_group (GTK_WINDOW (inst->main_win), ag); */

  /*   g_assert(inst->main_win != NULL); */
  /*   g_object_ref(inst->main_win); */
    
  /*   g_object_unref(ag); */
  /* } */

  /* inst->save_sequence_progress = */
  /*   GTK_DIALOG(FIND_OBJECT("save_sequence_progress_dialog")); */
  /* g_assert(inst->save_sequence_progress); */
  /* inst->save_sequence_progress_bar = */
  /*   GTK_PROGRESS_BAR(FIND_OBJECT("save_sequence_progress_bar")); */
  /* g_assert(inst->save_sequence_progress_bar); */
  
  /* gtk_builder_connect_signals(builder, inst); */
 

  g_object_unref(builder);
  return TRUE;
}


static gboolean
setup_recorder(InstanceContext *inst, GError **error)
{
  inst->recorder = clip_recorder_new();
  g_settings_bind(inst->app_ctxt->settings, PREF_SILENCE_LEVEL, inst->recorder,
		  "trim-level", G_SETTINGS_BIND_GET);
  g_settings_bind(inst->app_ctxt->settings, PREF_PRE_SILENCE, inst->recorder,
		  "pre-silence", G_SETTINGS_BIND_GET);
  g_settings_bind(inst->app_ctxt->settings, PREF_POST_SILENCE, inst->recorder,
		  "post-silence", G_SETTINGS_BIND_GET);

  g_signal_connect(inst->recorder, "recording", (GCallback)recording_cb, inst);
  g_signal_connect(inst->recorder, "playing", (GCallback)playing_cb, inst);
  g_signal_connect(inst->recorder, "stopped", (GCallback)stopped_cb, inst);
  g_signal_connect(inst->recorder, "run-error", (GCallback)run_error_cb, inst);
  g_signal_connect(inst->recorder, "power", (GCallback)record_power_cb, inst);

  return TRUE;
}
#if 0
#define USE_STYLE 0
#if USE_STYLE
static void
setup_style(InstanceContext *inst)
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
  context = gtk_widget_get_style_context(inst->main_win); 
  gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), 
				 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}
#endif
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

static gchar *work_dir = NULL;

static GOptionEntry options[] =
  {
    {"working-directory", 'w',
     0,
     G_OPTION_ARG_FILENAME,
     &work_dir,
     "Working directory",
   NULL
    },
    {NULL}
  };

#if 0
int
main(int argc, char **argv)
{
  GOptionContext *oc;
  GError *error = NULL;
  AppContext app;
  LIBXML_TEST_VERSION;
  g_thread_init(NULL);
  app_init(&app);
  oc = g_option_context_new(" - Record subtitles");
  g_option_context_add_group (oc, gtk_get_option_group (TRUE));
  g_option_context_add_group (oc, gst_init_get_option_group ());
  g_option_context_add_main_entries (oc, options, NULL);
  if (!g_option_context_parse (oc, &argc, &argv, &error)) {
    g_warning("Failed to parse options: %s", error->message);
    return EXIT_FAILURE;
  }
  g_option_context_free (oc);
  app.settings = g_settings_new(PREF_SCHEMA);
  g_assert(app.settings);
  if (!setup_recorder(&app, &error)) {
    app_destroy(&app);
    g_error("%s", error->message);
  }
  app.application = gtk_application_new("se.fluffware.subrec",
					(G_APPLICATION_HANDLES_COMMAND_LINE
					 | G_APPLICATION_NON_UNIQUE));
  if (!create_main_window(&app, &error)) {
    app_destroy(&app);
    g_error("%s", error->message);
  }
  if (work_dir) {
    GFile *file = g_file_new_for_path(work_dir);
    if (!set_working_directory(&app, file, &error)) {
      show_error(&app, "Failed to set working directory", &error);
    }
    g_object_unref(file);
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
#endif

static void
startup(GApplication *application, AppContext *app_ctxt)
{
  GtkBuilder *builder;
  GError *err = NULL;
  const GActionEntry app_actions[] = {
    {"quit", activate_quit, NULL},
    {"open-working-directory", activate_open_working_directory, NULL},
    {"new-working-directory", activate_new_working_directory, NULL},
    {"preferences", activate_preferences, NULL},
  };
  
  g_set_prgname("subrec");
  g_set_application_name("SubRec");
  app_init(app_ctxt);
  app_ctxt->application = GTK_APPLICATION(application);
  app_ctxt->settings = g_settings_new(PREF_SCHEMA);
  g_assert(app_ctxt->settings);

  builder = gtk_builder_new();
  /* Read menus */
  if (!builderutils_add_from_file(builder, "menus.ui", &err)) {
    app_destroy(app_ctxt);
    g_printerr("%s", err->message);
    g_error_free(err);
    exit(EXIT_FAILURE);
  }

  app_ctxt->app_menu = G_MENU_MODEL (gtk_builder_get_object(builder,
							    "appmenu"));
  g_assert(app_ctxt->app_menu);
  g_object_ref(app_ctxt->app_menu);
  g_action_map_add_action_entries (G_ACTION_MAP (application),
				   app_actions,G_N_ELEMENTS(app_actions),
				   app_ctxt);
  
  gtk_application_set_app_menu (GTK_APPLICATION(application),
				app_ctxt->app_menu);


  app_ctxt->win_menu = G_MENU_MODEL (gtk_builder_get_object(builder,
							    "winmenu"));
  g_assert(app_ctxt->win_menu);
  g_object_ref(app_ctxt->win_menu);
  
  gtk_application_set_menubar(GTK_APPLICATION(application),
			      app_ctxt->win_menu);
  
  g_object_unref(builder);
  g_debug("Startup");
 
}

static void
shutdown(GApplication *application, AppContext *app_ctxt)
{
  g_debug("Shutdown");
  app_destroy(app_ctxt);
}

gint
create_instance(GApplication *application, GFile *work_dir,
		AppContext *app_ctxt)
{
  InstanceContext *inst;
  GError *err = NULL;

  inst = instance_create(app_ctxt);
  if (!setup_recorder(inst, &err)) {
    instance_free(inst);
    g_printerr("%s", err->message);
    g_error_free(err);
    return EXIT_FAILURE;
  }
  if (!create_main_window(inst, GTK_APPLICATION(application), &err)) {
    instance_free(inst);
    g_printerr("%s", err->message);
    g_error_free(err);
    return EXIT_FAILURE;
  }
  
  if (work_dir) {
    if (!set_working_directory(inst, work_dir , &err)) {
      show_error(inst, "Failed to set working directory", &err);
    }
  }
								  
  /* print_widget_tree(inst->main_win,2); */
  return EXIT_SUCCESS;
}

gint
handle_command_line (GApplication            *application,
		     GApplicationCommandLine *command_line,
		     AppContext *app_ctxt)
{
  gchar **argv = NULL;
  int argc;
  
  argv = g_application_command_line_get_arguments (command_line,
						   &argc);
  if (argc >= 2) {
    gint d;
    for (d = 1; d < argc; d++) {
      GFile *work_dir =
	g_application_command_line_create_file_for_arg(command_line,
						       argv[d]);
      create_instance(application, work_dir, app_ctxt);
      g_object_unref(work_dir);
    }
  } else {
    create_instance(application, NULL, app_ctxt);
  }
  g_strfreev(argv);

  return EXIT_SUCCESS;
}

static void
handle_open (GApplication *application,
	     GFile **files, gint n_files,
	     gchar *hint, AppContext *app_ctxt)
{
  gint d;
  for (d = 0; d < n_files; d++) {
    GFile *work_dir = files[d];
    create_instance(application, work_dir, app_ctxt);
  }
}


int
main(int argc, char **argv)
{
  GApplication *gapp;
  int status;
  AppContext app_ctxt;

  gapp = G_APPLICATION(gtk_application_new("se.fluffware.subrec",
					   (G_APPLICATION_HANDLES_COMMAND_LINE|
					    G_APPLICATION_HANDLES_OPEN)));
  g_application_add_option_group(gapp, gst_init_get_option_group ());
  g_application_add_main_option_entries(gapp, options);
  g_signal_connect (gapp, "startup", G_CALLBACK (startup), &app_ctxt);
  g_signal_connect (gapp, "shutdown", G_CALLBACK (shutdown), &app_ctxt);
  g_signal_connect (gapp, "command-line", G_CALLBACK (handle_command_line),
		    &app_ctxt);
  g_signal_connect (gapp, "open", G_CALLBACK (handle_open),
		    &app_ctxt);
  status = g_application_run (gapp, argc, argv);

  g_object_unref (gapp);

  return status;
}

