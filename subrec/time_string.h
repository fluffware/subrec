#ifndef __TIME_STRING_H__ITDJPCNM3S__
#define __TIME_STRING_H__ITDJPCNM3S__


#include <glib.h>

G_BEGIN_DECLS

#define TIME_STRING_MIN_LEN (3 + 3 + (2+1+3+1) + 1) /* Including '\0' */

gboolean
time_string_parse(const gchar *str, gint64 *time);

void
time_string_format(gchar *str, guint capacity, gint64 time);
G_END_DECLS

#endif /* __TIME_STRING_H__ITDJPCNM3S__ */
