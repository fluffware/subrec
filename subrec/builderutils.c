#include "builderutils.h"
#include "dir_config.h"
gboolean
builderutils_add_from_file(GtkBuilder *builder, gchar *filename, GError **err)
{
  gboolean res;
  gchar *path;
  const gchar *uidir = g_getenv("UIDIR");
  if (!uidir) uidir = UIDIR;
  path= g_build_filename(uidir, filename, NULL);
  res = gtk_builder_add_from_file(builder, path, err);
  g_free(path);
  return res;
}
