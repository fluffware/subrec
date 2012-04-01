#include "preferences_dialog.h"
#include <unitspinbutton.h>
#include <string.h>
#include <math.h>

static GtkDialog *preferences_dialog = NULL;

static void
preferences_dialog_destroyed(GtkWidget *object, gpointer *user_data)
{
  preferences_dialog = NULL;
}

static void
preferences_dialog_response(GtkDialog *dialog,
		      gint response_id, gpointer *user_data)
{
  gtk_widget_hide(GTK_WIDGET(dialog));
}

static void
get_range(GSettings *settings, const gchar *key, gdouble *low, gdouble *high)
{
  GVariant *range;
  GVariant *limits;
  gchar *type_str;
  range = g_settings_get_range(settings, key);
  g_variant_get(range, "(sv)", &type_str, &limits);
  g_debug("range type %s", type_str);
  if (strcmp("range", type_str) == 0) {
    g_variant_get(limits, "(dd)", low, high);
  }
  g_variant_unref(limits);
  g_variant_unref(range);
}

static gboolean
dB_get_mapping(GValue *value, GVariant *variant, gpointer user_data)
{
  gdouble dB;
  gdouble lin = g_variant_get_double(variant);
  dB = 10.0*log10(lin);
  g_value_set_double(value, dB);
  return TRUE;
}

static GVariant *
dB_set_maapping(const GValue *value, const GVariantType *expected_type,
		gpointer user_data)
{
  gdouble dB;
  gdouble lin;
  dB = g_value_get_double(value);
  lin = pow(10, dB / 10.0);
  if (!g_variant_type_equal(expected_type, G_VARIANT_TYPE_DOUBLE)) {
    return NULL;
  }
  return g_variant_new_double (lin);
}


static GtkWidget *
create_settings_unit_spin_box(GSettings *settings, const gchar *key,
			      const gchar *unit, gboolean dB)
{
  GtkWidget *spin;
  GtkAdjustment *adj;
  gdouble low = 0.0;
  gdouble high = 1.0;
  get_range(settings, key, &low, &high);
  if (!dB) {
    adj = gtk_adjustment_new(low, low, high, 0.1, 1.0, 0);
    spin = unit_spin_button_new(adj, 1, 1, unit);
    g_settings_bind(settings, key, spin, "value",
		    G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET);
  } else {
    low = 10.0*log10(low);
    high = 10.0*log10(high);
    adj = gtk_adjustment_new(low, low, high, 0.1, 1.0, 0);
    spin = unit_spin_button_new(adj, 1, 1, unit);
    g_settings_bind_with_mapping(settings, key, spin, "value",
				 G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
				 dB_get_mapping, dB_set_maapping, NULL, NULL);
  }
  return spin;
}

static void
add_setting(GSettings *settings, const gchar *key, const gchar *label_str,
	    const gchar *unit, gboolean dB,  GtkTable *table, gint row)
{
  GtkWidget *label;
  GtkWidget *spin;
  
  label = gtk_label_new(label_str);
  gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
  gtk_table_attach (table, label,
		    0,1, row, row + 1,
		    GTK_FILL,
		    0,
		    3, 0);
  gtk_widget_show(label);

  spin = create_settings_unit_spin_box(settings, key, unit, dB);
  gtk_table_attach (table, spin,
		    1,2, row, row + 1,
		    GTK_EXPAND | GTK_FILL,
		    0,
		    3, 0);
  gtk_widget_show(spin);
}

void
show_preferences_dialog(GtkWindow *parent)
{
  if (!preferences_dialog) {
    GSettings *settings = g_settings_new("se.fluffware.apps.subrec");
    GtkWidget *content;
    GtkWidget *viewport;
    GtkWidget *scrolled;
    GtkWidget *table;
    if (!settings) return;
    preferences_dialog =
      GTK_DIALOG(gtk_dialog_new_with_buttons ("Preferences",
					      parent,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_ACCEPT,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      NULL));
    gtk_window_set_default_size(GTK_WINDOW(preferences_dialog),500,400);
    content = gtk_dialog_get_content_area(preferences_dialog);
    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				   GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);
    viewport = gtk_viewport_new(gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolled)),
				gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled)));
    table = gtk_table_new(4,2, FALSE);
    add_setting(settings, "silence-level", "Silence level",
		"dB", TRUE,
		GTK_TABLE(table),0);
    add_setting(settings, "warning-level", "Warning level",
		"dB", TRUE,
		GTK_TABLE(table),1);
    add_setting(settings, "pre-silence", "Silence before",
		"s", FALSE,
		GTK_TABLE(table),2);
    add_setting(settings, "post-silence", "Silence after",
		"s", FALSE,
		GTK_TABLE(table),3);
    gtk_container_add(GTK_CONTAINER(viewport), table); 
    gtk_container_add(GTK_CONTAINER(scrolled), viewport); 
    gtk_widget_show(table);
    gtk_widget_show(viewport);
    gtk_box_pack_start(GTK_BOX(content), scrolled, TRUE, TRUE, 0);
    gtk_widget_show(scrolled);
    g_signal_connect(preferences_dialog, "destroy",
		     G_CALLBACK(preferences_dialog_destroyed), NULL);
    g_signal_connect(preferences_dialog, "response",
		     G_CALLBACK(preferences_dialog_response), NULL);
    g_object_set_data_full (G_OBJECT(preferences_dialog), "settings",
			    settings, g_object_unref);
  }
  gtk_widget_show(GTK_WIDGET(preferences_dialog));
}
  
