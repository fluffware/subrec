#include <glib-object.h>

#if !(GLIB_CHECK_VERSION(2,28,0))
void
g_clear_object(volatile GObject **object_ptr)
{
  if (*object_ptr == NULL) return;
  g_object_unref((GObject *)*object_ptr);
  *object_ptr = NULL;
}

void g_list_free_full (GList *list, GDestroyNotify free_func)
{
  GList *l = list;
  while(l) {
    free_func(l->data);
    l = l->next;
  }
  g_list_free(list);
}
#endif
