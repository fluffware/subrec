#ifndef __BUILDERUTILS_H__48LHGIV4KF__
#define __BUILDERUTILS_H__48LHGIV4KF__

#include <gtk/gtk.h>
#ifndef UIDIR
#define UIDIR .
#endif

gboolean
builderutils_add_from_file(GtkBuilder *builder, gchar *filename, GError **err);


#endif /* __BUILDERUTILS_H__48LHGIV4KF__ */
