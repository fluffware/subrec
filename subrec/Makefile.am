uidir=@datadir@/subrec/ui
appdir=@datadir@/applications
icondir=@datadir@/icons
themedir=$(icondir)/hicolor
app48x48dir=$(themedir)/48x48/apps
app32x32dir=$(themedir)/32x32/apps
app22x22dir=$(themedir)/22x22/apps

INCLUDES= -I @builddir@ -I. @GTK_CFLAGS@ @GLIB_CFLAGS@ @GST_APP_CFLAGS@ @XML_CPPFLAGS@

AM_CFLAGS="-std=c99"

bin_PROGRAMS = subrec sequence_test segment_test

subrec_SOURCES = main.c  builderutils.c \
about_dialog.c about_dialog.h \
preferences_dialog.c preferences_dialog.h \
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
unitspinbutton.c unitspinbutton.h \
blocked_seek.c blocked_seek.h \
save_sequence.c save_sequence.h \
buffer_counter.c buffer_counter.h

subrec_LDADD = @GTK_LIBS@ @GLIB_LIBS@ @XML_LIBS@ @GST_APP_LIBS@

sequence_test_SOURCES = sequence_test.c blocked_seek.c blocked_seek.h
sequence_test_LDADD = @GLIB_LIBS@ @GST_APP_LIBS@

segment_test_SOURCES = segment_test.c 
segment_test_LDADD = @GLIB_LIBS@ @GST_APP_LIBS@

images = green_lamp_active.png green_lamp_normal.png \
yellow_lamp_active.png yellow_lamp_normal.png \
red_lamp_active.png red_lamp_normal.png
ui_DATA= ui/main_win.ui $(images:%=ui/%)

app_DATA=subrec.desktop

app48x48_DATA=images/hicolor/48x48/subrec.png
app32x32_DATA=images/hicolor/32x32/subrec.png
app22x22_DATA=images/hicolor/22x22/subrec.png

dir_config.h: @top_srcdir@/config.status
	echo "#define UIDIR \"$(uidir)\"" > $@

version_info.h: FORCE
	echo "#define PKG_VERSION \"$(VERSION)\"" > $@
	echo "#define GIT_COMMIT \"`git rev-parse HEAD`\"" >> $@


.PHONY: FORCE

gsettings_SCHEMAS = subrec.gschema.xml

@GSETTINGS_RULES@

BUILT_SOURCES= dir_config.h version_info.h

CLEANFILES = dir_config.h version_info.h

EXTRA_DIST=subrec.gschema.xml
