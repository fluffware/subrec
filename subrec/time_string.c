#include <time_string.h>
#include <stdio.h>

static void
skip_white(const gchar **p)
{
  while(g_unichar_isspace(g_utf8_get_char(*p))) *p = g_utf8_next_char(*p);
}

static gboolean
parse_uint(const gchar **p, gint64 *v)
{
  gint d;
  if ((d =g_unichar_digit_value(g_utf8_get_char(*p)))<0) {
    return FALSE;
  }
  *p = g_utf8_next_char(*p);
  *v = d;
  while((d =g_unichar_digit_value(g_utf8_get_char(*p)))>=0) {
    *p = g_utf8_next_char(*p);
    *v = *v *10 + d;
  }
  return TRUE;
}

static gboolean
parse_fraction(const gchar **p, guint64 scale, guint64 *v)
{
  gint d;
  *v = 0;
  while((d =g_unichar_digit_value(g_utf8_get_char(*p)))>=0) {
    *p = g_utf8_next_char(*p);
    scale /= 10;
    *v += scale * d;
  }
  return TRUE;
}

#define MILLISECONDS 1000000LL
#define SECONDS 1000000000LL
#define MINUTES (SECONDS  * 60)
#define HOURS (MINUTES * 60)
gboolean
time_string_parse(const gchar *str, gint64 *time)
{
  gint64 v;
  guint64 frac;
  gint64 t = 0LL;
  const gchar *p = str;
  gunichar sign;
  gunichar c;
  gint64 next = SECONDS;

  skip_white(&p);
  while(*p != '\0') {
    sign = g_utf8_get_char(p);
    if (sign == '+' ||  sign == '-') p = g_utf8_next_char(p);
    skip_white(&p);
    if (!parse_uint(&p, &v)) return FALSE;
    if (sign == '-') v = -v;
    skip_white(&p);
    c = g_utf8_get_char(p);
    if (c == 0) {
      t += next * v;
      break;
    } else {
      p = g_utf8_next_char(p);
    }
    switch(c) {
    case 'h':
    case 'H':
      t += HOURS * v;
      next = MINUTES;
      break;
    case 'm':
    case 'M':
      t += MINUTES * v;
      next = SECONDS;
      break;
    case 's':
    case 'S':
      t += SECONDS * v;
      next = SECONDS;
    break;
    case '.':
      {
	t += SECONDS * v;
	parse_fraction(&p, SECONDS, &frac);
	t += frac;
	skip_white(&p);
	c = g_utf8_get_char(p);
	if (c != 's' && c != 'S' && c != '\0') return FALSE;
	if (c != '\0') p = g_utf8_next_char(p);
	next = SECONDS;
      }
      break;
    }
    skip_white(&p);
  }
  *time = t;
  return TRUE;
}

void
time_string_format(gchar *str, guint capacity, gint64 time)
{
  gint h,m,s;
  h = time / HOURS;
  time -= h * HOURS;
  m = time / MINUTES;
  time -= m * MINUTES;
  s = time / SECONDS;
  time -= s * SECONDS;
  if (time > 0) {
    g_snprintf((char*)str, capacity, "%02dh%02dm%02d.%03ds",
	       h,m,s, (int)(time / MILLISECONDS));
  } else {
    snprintf((char*)str, capacity, "%02dh%02dm%02ds",h,m,s);
  }
}
