#ifndef __UNITSPINBUTTON_H__UI2CSM7DYV__
#define __UNITSPINBUTTON_H__UI2CSM7DYV__

#include <gtk/gtk.h>

G_BEGIN_DECLS


#define UNIT_SPIN_BUTTON_TYPE                  (unit_spin_button_get_type ())
#define UNIT_SPIN_BUTTON(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), UNIT_SPIN_BUTTON_TYPE, UnitSpinButton))
#define UNIT_SPIN_BUTTON_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), UNIT_SPIN_BUTTON_TYPE, GtkSpinButtonClass))
#define IS_UNIT_SPIN_BUTTON(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UNIT_SPIN_BUTTON_TYPE))
#define IS_UNIT_SPIN_BUTTON_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), UNIT_SPIN_BUTTON_TYPE))
#define UNIT_SPIN_BUTTON_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), UNIT_SPIN_BUTTON_TYPE, UnitSpinButtonClass))


typedef struct _UnitSpinButton UnitSpinButton;
typedef struct _UnitSpinButtonClass UnitSpinButtonClass;

struct _UnitSpinButton
{
  GtkSpinButton entry;
  gchar *unit;
};

struct _UnitSpinButtonClass
{
  GtkSpinButtonClass parent_class;
};

GType unit_spin_button_get_type (void) G_GNUC_CONST;

GtkWidget* unit_spin_button_new (GtkAdjustment  *adjustment,
				 gdouble climb_rate,
				 guint digits,
				 const gchar *unit);

G_END_DECLS

#endif /* __UNITSPINBUTTON_H__UI2CSM7DYV__ */
