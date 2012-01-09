#ifndef __GTKCELLRENDERERTIME_H__ABZXBGJKOS__
#define __GTKCELLRENDERERTIME_H__ABZXBGJKOS__
#include <gtk/gtk.h>

G_BEGIN_DECLS
#define GTK_TYPE_CELL_RENDERER_TIME  (gtk_cell_renderer_time_get_type ())
#define GTK_CELL_RENDERER_TIME(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CELL_RENDERER_TIME, GtkCellRendererTime)
#define GTK_CELL_RENDERER_TIME_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, GTK_TYPE_CELL_RENDERER_TIME, GtkCellRendererTimeClass)
#define GTK_IS_CELL_RENDERER_TIME(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CELL_RENDERER_TIME)
#define GTK_IS_CELL_RENDERER_TIME_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CELL_RENDERER_TIME))
#define GTK_CELL_RENDERER_TIME_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CELL_RENDERER_TIME, GtkCellRendererTimeClass))

#define GTK_CELL_RENDERER_TIME_INVALID G_MININT64

typedef struct _GtkCellRendererTime GtkCellRendererTime;
typedef struct _GtkCellRendererTimeClass GtkCellRendererTimeClass;
struct _GtkCellRendererTime
{
  GtkCellRendererText parent;
  gint64 time;
  gint64 time_plus;
  gint64 time_minus;
};

struct _GtkCellRendererTimeClass
{
  GtkCellRendererTextClass parent_class;
};

GtkCellRenderer *gtk_cell_renderer_time_new (void);

GType
gtk_cell_renderer_time_get_type(void);
G_END_DECLS
#endif /* __GTKCELLRENDERERTIME_H__ABZXBGJKOS__ */
