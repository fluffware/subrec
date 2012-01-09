
#include "gtkcellrenderertime.h"
#include <time_string.h>

static GObjectClass *parent_class = NULL;

enum {
  PROP_0,
  PROP_TIME,
  PROP_TIME_PARSED,
  PROP_TIME_PLUS,
  PROP_TIME_MINUS
};

static void
update_text(GtkCellRendererTime *renderer)
{
  gchar buffer[20];
  time_string_format(buffer, sizeof(buffer),
		     (renderer->time + renderer->time_plus
		      + renderer->time_minus));
  g_object_set(renderer, "text", buffer , NULL);
}

static void
gtk_cell_renderer_time_set_property (GObject        *object,
				     guint           property_id,
				     const GValue   *value,
				     GParamSpec     *pspec)
{
  GtkCellRendererTime *renderer = GTK_CELL_RENDERER_TIME(object);
  switch(property_id) {
  case PROP_TIME:
    renderer->time = g_value_get_int64(value);
    update_text(renderer);
    break;
  case PROP_TIME_PLUS:
    renderer->time_plus = g_value_get_int64(value);
    update_text(renderer);
    break;
  case PROP_TIME_MINUS:
    renderer->time_minus = g_value_get_int64(value);
    update_text(renderer);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
gtk_cell_renderer_time_get_property (GObject        *object,
				     guint           property_id,
				     GValue         *value,
				     GParamSpec     *pspec)
{
  GtkCellRendererTime *renderer = GTK_CELL_RENDERER_TIME(object);
  switch(property_id) {
  case PROP_TIME_PARSED:
    {
      gchar *text;
      gint64 time = G_MININT64;
      g_object_get(renderer, "text", &text, NULL);
      time_string_parse(text, &time);
      g_value_set_int64(value, time);
    }
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
gtk_cell_renderer_time_class_init(GtkCellRendererTimeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  parent_class = g_type_class_ref (GTK_TYPE_CELL_RENDERER_TEXT);
  gobject_class->set_property = gtk_cell_renderer_time_set_property;
  gobject_class->get_property = gtk_cell_renderer_time_get_property;

  g_object_class_install_property(gobject_class, PROP_TIME,
				  g_param_spec_int64("time", "Time",
						     "Displayed time",
						     G_MININT64,G_MAXINT64, 0,
						     G_PARAM_WRITABLE));
  g_object_class_install_property(gobject_class, PROP_TIME_PLUS,
				  g_param_spec_int64("time-plus", "Time plus",
						     "Added time",
						     G_MININT64,G_MAXINT64, 0,
						     G_PARAM_WRITABLE));
  
   g_object_class_install_property(gobject_class, PROP_TIME_MINUS,
				  g_param_spec_int64("time-minus", "Time minus",
						     "Subtracted time",
						     G_MININT64,G_MAXINT64, 0,
						     G_PARAM_WRITABLE));

   g_object_class_install_property(gobject_class, PROP_TIME_PARSED,
				   g_param_spec_int64("parsed-time",
						      "Parsed time",
						     "Time as parsed from entry",
						     G_MININT64,G_MAXINT64, 0,
						     G_PARAM_READABLE));
  
}

static void
gtk_cell_renderer_time_init(GtkCellRendererTime *renderer)
{
  renderer->time = 0LL;
  renderer->time_plus = 0LL;
  renderer->time_minus = 0LL;
}

GType
gtk_cell_renderer_time_get_type(void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo info =
      {
	sizeof (GtkCellRendererTimeClass),
	NULL, /* base_init */
	NULL, /* base_finalize */
	(GClassInitFunc) gtk_cell_renderer_time_class_init,
	NULL,
	NULL, /* class_data */
	sizeof (GtkCellRendererTime),
	0,    /* n_preallocs */
	(GInstanceInitFunc) gtk_cell_renderer_time_init,
	NULL
      };
      
      type = g_type_register_static (GTK_TYPE_CELL_RENDERER_TEXT,
				     "GtkCellRendererTime",
				     &info,
				     0);
      
    }

  return type;
}

GtkCellRenderer *
gtk_cell_renderer_time_new (void)
{
   GtkCellRendererTime *renderer =
     GTK_CELL_RENDERER_TIME(g_object_new (GTK_TYPE_CELL_RENDERER_TIME,NULL));
  if (!renderer) return NULL;
  return GTK_CELL_RENDERER(renderer);
}

