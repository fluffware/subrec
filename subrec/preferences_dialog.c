#include "preferences_dialog.h"
#include "preferences.h"
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

static double
get_numeric(GVariant *variant)
{
  const GVariantType *type = g_variant_get_type(variant);
  if (g_variant_type_equal(type, G_VARIANT_TYPE_DOUBLE)) {
    return g_variant_get_double(variant);
  } else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT64)) {
    return g_variant_get_uint64(variant);
  }
  return 0.0;
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
    GVariant *low_var;
    GVariant *high_var;
    g_variant_get(limits, "(@?@?)", &low_var, &high_var);
    *low = get_numeric(low_var);
    g_variant_unref(low_var);
    *high = get_numeric(high_var);
    g_variant_unref(high_var);
  }
  g_variant_unref(limits);
  g_variant_unref(range);
}

typedef struct  {
  const gchar *unit;
  gboolean log;
  double scale; /* From setting to widget */
  double climb_rate;
  gint digits;
} SettingMapping;

static double
get_mapping(double v, const SettingMapping *mapping)
{
  if (mapping->log) {
    v = log10(v);
  }
  v = mapping->scale * v;
  return v;
}

static gboolean
spin_get_mapping(GValue *value, GVariant *variant, gpointer user_data)
{
  SettingMapping *map = user_data;
  double v;
  v = get_numeric(variant);
  v = get_mapping(v, map);
  g_value_set_double(value, v);
  return TRUE;
}

static double
set_mapping(double v, const SettingMapping *mapping)
{
  v = v / mapping->scale;
  if (mapping->log) {
    v = pow(10, v);
  }
  return v;
}

static GVariant *
spin_set_mapping(const GValue *value, const GVariantType *expected_type,
		gpointer user_data)
{
  const SettingMapping *map = user_data;
  double v;
  v = g_value_get_double(value);
  v = set_mapping(v, map);
  if (g_variant_type_equal(expected_type, G_VARIANT_TYPE_DOUBLE)) {
    return g_variant_new_double (v);
  } else if (g_variant_type_equal(expected_type, G_VARIANT_TYPE_UINT64)) {
    return g_variant_new_uint64((guint64)round(v));
  } else {
    return NULL;
  }
}

static GtkWidget *
create_settings_unit_spin_box(GSettings *settings, const gchar *key,
			      const SettingMapping *mapping)
{
  GtkWidget *spin;
  GtkAdjustment *adj;
  gdouble low = 0.0;
  gdouble high = 1.0;
  get_range(settings, key, &low, &high);
  low = get_mapping(low, mapping);
  high = get_mapping(high, mapping);
  adj = gtk_adjustment_new(low, low, high,
			   mapping->climb_rate, mapping->climb_rate * 10,
			   0);
  spin = unit_spin_button_new(adj, mapping->climb_rate, mapping->digits,
			      mapping->unit);
  g_settings_bind_with_mapping(settings, key, spin, "value",
			       G_SETTINGS_BIND_GET | G_SETTINGS_BIND_SET,
			       spin_get_mapping, spin_set_mapping,
			       (gpointer)mapping, NULL);
  return spin;
}

const static SettingMapping dB_mapping =
  {
    "dB",
    TRUE, 10,
    0.1,
    1
  };

const static SettingMapping ns_to_s_mapping =
  {
    "s",
    FALSE, 1e-9,
    0.01,
    2
  };
    
static void
add_setting(GSettings *settings, const gchar *key, const gchar *label_str,
	    const SettingMapping *mapping, GtkTable *table, gint row)
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

  spin = create_settings_unit_spin_box(settings, key, mapping);
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
    GSettings *settings = g_settings_new(PREF_SCHEMA);
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
    add_setting(settings, PREF_SILENCE_LEVEL, "Silence level",
		&dB_mapping,
		GTK_TABLE(table),0);
    add_setting(settings, PREF_NORMAL_LEVEL, "Normal level",
		&dB_mapping,
		GTK_TABLE(table),1);
    add_setting(settings, PREF_PRE_SILENCE, "Silence before",
		&ns_to_s_mapping,
		GTK_TABLE(table),2);
    add_setting(settings, PREF_POST_SILENCE, "Silence after",
		&ns_to_s_mapping,
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
  
