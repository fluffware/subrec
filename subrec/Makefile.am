uidir=@datadir@/subrec/ui
INCLUDES= -I. @GTK_CFLAGS@ @GLIB_CFLAGS@ @GST_APP_CFLAGS@ @XML_CPPFLAGS@ -DUIDIR=$(uidir)

AM_CFLAGS="-std=c99"

bin_PROGRAMS = subrec
subrec_SOURCES = main.c  builderutils.c \
asset_map.c asset_map.h \
xml_tree_parser.c xml_tree_parser.h \
packing_list.c packing_list.h \
composition_playlist.c composition_playlist.h \
dcsubtitle.c dcsubtitle.h \
subtitle_store.c subtitle_store.h \
subtitle_store_io.c subtitle_store_io.h \
gtkcellrenderertime.c gtkcellrenderertime.h \
time_string.c time_string.h \
clip_recorder.c clip_recorder.h \
g_compat.c g_compat.h

subrec_LDADD = @GTK_LIBS@ @GLIB_LIBS@ @XML_LIBS@ @GST_APP_LIBS@

ui_DATA= main_win.ui

gui: main_win.ui
