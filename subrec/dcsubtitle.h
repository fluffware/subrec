#ifndef __DCSUBTITLE_H__FVGHVIDJ8S__
#define __DCSUBTITLE_H__FVGHVIDJ8S__

#include <glib-object.h>
#include <gio/gio.h>

#define DCSUBTITLE_ERROR (dcsubtitle_error_quark())
enum {
  DCSUBTITLE_ERROR_CREATE_READER_FAILED = 1,
  DCSUBTITLE_ERROR_PARSER,
  DCSUBTITLE_ERROR_XML_STRUCTURE,
  DCSUBTITLE_ERROR_VALUE
};

/*
 * Type macros.
 */
#define DCSUBTITLE_TYPE (dcsubtitle_get_type ())
#define DCSUBTITLE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), DCSUBTITLE_TYPE, DCSubtitle))
#define IS_DCSUBTITLE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), DCSUBTITLE_TYPE))
#define DCSUBTITLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), DCSUBTITLE_TYPE, DCSubtitleClass)) 
#define IS_DCSUBTITLE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), DCSUBTITLE_TYPE))
#define DCSUBTITLE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), DCSUBTITLE_TYPE, DCSubtitleClass))

typedef struct _DCSubtitle        DCSubtitle;
typedef struct _DCSubtitleClass   DCSubtitleClass;
typedef struct _DCSubtitleSpot   DCSubtitleSpot;
typedef struct _DCSubtitleText   DCSubtitleText;
  
struct _DCSubtitleSpot
{
  gint spot_number;
  gint time_in;
  gint time_out;
  gint fade_up_time;
  gint fade_down_time;
  GList *text;
};
#define SUBTITLE_DIR_UNKNOWN 0x0
#define SUBTITLE_DIR_HORIZONTAL 0x1
#define SUBTITLE_DIR_MASK 0xf

#define SUBTITLE_HALIGN_UNKNOWN 0x00
#define SUBTITLE_HALIGN_LEFT 0x10
#define SUBTITLE_HALIGN_CENTER 0x20
#define SUBTITLE_HALIGN_RIGHT 0x30
#define SUBTITLE_HALIGN_MASK 0x70

#define SUBTITLE_VALIGN_UNKNOWN 0x000
#define SUBTITLE_VALIGN_TOP 0x100
#define SUBTITLE_VALIGN_CENTER 0x200
#define SUBTITLE_VALIGN_BOTTOM 0x300
#define SUBTITLE_VALIGN_MASK 0x700


struct _DCSubtitleText
{
  guint flags;
  gdouble hpos;
  gdouble vpos;
  gchar *text;
};

struct _DCSubtitle
{
  GObject parent_instance;
  
  /* instance members */
  gchar *id;
  gchar *language;
  GList *spots;
  
};

struct _DCSubtitleClass
{
  GObjectClass parent_class;

  /* class members */
};

/* used by DCSUBTITLE_TYPE */
GType dcsubtitle_get_type (void);

/*
 * Method definitions.
 */


/*
 * Create a new list from file
 */
DCSubtitle *
dcsubtitle_read(GFile *file, GError **error);


#endif /* __DCSUBTITLE_H__FVGHVIDJ8S__ */
