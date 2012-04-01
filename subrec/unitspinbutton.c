#include "unitspinbutton.h"

G_DEFINE_TYPE (UnitSpinButton, unit_spin_button, GTK_TYPE_SPIN_BUTTON)

static void
unit_spin_button_init (UnitSpinButton *spin_button)
{
}

static gboolean
unit_spin_button_output(GtkSpinButton *spin)
{
  GtkAdjustment *adj;
  gchar *text;
  double value;
  adj = gtk_spin_button_get_adjustment (spin);
  value = gtk_adjustment_get_value (adj);
  text = g_strdup_printf ("%.1lf %s", value, UNIT_SPIN_BUTTON(spin)->unit);
  gtk_entry_set_text (GTK_ENTRY (spin), text);
  g_free (text);
  return TRUE;
}

static void
unit_spin_button_class_init (UnitSpinButtonClass *g_class)
{
  GtkSpinButtonClass *spin_class = GTK_SPIN_BUTTON_CLASS (g_class);
  spin_class->output = unit_spin_button_output;
}

GtkWidget*
unit_spin_button_new (GtkAdjustment  *adjustment,
		      gdouble         climb_rate,
		      guint           digits,
		      const gchar *unit)
{
  UnitSpinButton *spin;
  spin = g_object_new(UNIT_SPIN_BUTTON_TYPE, NULL);
  gtk_spin_button_configure(GTK_SPIN_BUTTON(spin), adjustment, climb_rate, digits);
  spin->unit = g_strdup(unit);
  return GTK_WIDGET(spin);
}
