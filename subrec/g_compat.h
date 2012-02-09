#include <glib-object.h>

#if (!GLIB_CHECK_VERSION(2,28,0))
void g_clear_object(volatile GObject **object_ptr);
#define g_clear_object(object_ptr) \
  G_STMT_START {                                                             \
    /* Only one access, please */                                            \
    gpointer *_p = (gpointer) (object_ptr);                                  \
    gpointer _o;                                                             \
                                                                             \
    do                                                                       \
      _o = g_atomic_pointer_get (_p);                                        \
    while G_UNLIKELY (!g_atomic_pointer_compare_and_exchange (_p, _o, NULL));\
                                                                             \
    if (_o)                                                                  \
      g_object_unref (_o);                                                   \
  } G_STMT_END

void g_list_free_full(GList *list, GDestroyNotify free_func);
#endif
