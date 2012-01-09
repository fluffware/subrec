#ifndef __COMPOSITION_PLAYLIST_H__EVLL31ENFX__
#define __COMPOSITION_PLAYLIST_H__EVLL31ENFX__

#include <glib-object.h>
#include <gio/gio.h>

#define COMPOSITION_PLAYLIST_ERROR (composition_playlist_error_quark())
enum {
  COMPOSITION_PLAYLIST_ERROR_CREATE_READER_FAILED = 1,
  COMPOSITION_PLAYLIST_ERROR_PARSER,
  COMPOSITION_PLAYLIST_ERROR_XML_STRUCTURE,
  COMPOSITION_PLAYLIST_ERROR_VALUE
};

/*
 * Type macros.
 */
#define COMPOSITION_PLAYLIST_TYPE (composition_playlist_get_type ())
#define COMPOSITION_PLAYLIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), COMPOSITION_PLAYLIST_TYPE, CompositionPlaylist))
#define IS_COMPOSITION_PLAYLIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), COMPOSITION_PLAYLIST_TYPE))
#define COMPOSITION_PLAYLIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), COMPOSITION_PLAYLIST_TYPE, CompositionPlaylistClass)) 
#define IS_COMPOSITION_PLAYLIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), COMPOSITION_PLAYLIST_TYPE))
#define COMPOSITION_PLAYLIST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), COMPOSITION_PLAYLIST_TYPE, CompositionPlaylistClass))

typedef struct _CompositionPlaylist        CompositionPlaylist;
typedef struct _CompositionPlaylistClass   CompositionPlaylistClass;
typedef struct _CompositionPlaylistReel   CompositionPlaylistReel;
typedef struct _CompositionPlaylistAsset   CompositionPlaylistAsset;
typedef struct _CompositionPlaylistRatio   CompositionPlaylistRatio;

struct _CompositionPlaylistRatio
{
  guint num;
  guint denom;
};

enum {
  AssetTypeGeneric = 0,
  AssetTypeTrackFile,
  AssetTypeSoundTrack,
  AssetTypeSubtitleTrack,
  AssetTypePictureTrack,
  AssetTypeMarker,
  AssetTypeProjectorData
};
  
struct _CompositionPlaylistAsset
{
  gint type;
  gchar *id;
  gulong intrinsic_duration;
  gulong duration;
  CompositionPlaylistRatio edit_rate;
  gulong entry_point;
  gchar *annotation;
};

struct _CompositionPlaylistReel
{
  gchar *id;
  gchar *annotation;
  GList *assets;
};

struct _CompositionPlaylist
{
  GObject parent_instance;
  
  /* instance members */
  gchar *id;
  GList *reels;
  
};

struct _CompositionPlaylistClass
{
  GObjectClass parent_class;

  /* class members */
};

/* used by COMPOSITION_PLAYLIST_TYPE */
GType composition_playlist_get_type (void);

/*
 * Method definitions.
 */


/*
 * Create a new list from file
 */
CompositionPlaylist *
composition_playlist_read(GFile *file, GError **error);

GList *
composition_playlist_get_reels(CompositionPlaylist *cpl);

#endif /* __COMPOSITION_PLAYLIST_H__EVLL31ENFX__ */
