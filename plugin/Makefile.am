ACLOCAL_AMFLAGS="-I $(top_srcdir)/m4" 
GST_CFLAGS += -DGST_PACKAGE_NAME=\"subrec\" -DGST_PACKAGE_ORIGIN=\"\"

plugin_LTLIBRARIES = libgstsubrec.la

libgstsubrec_la_SOURCES = subrec-plugin.c audiormspower.c gstaudiotestsrc.c
libgstsubrec_la_CFLAGS = $(GST_CFLAGS) -std=c99
libgstsubrec_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(GSTAUDIO_LIBS) $(GSTCTRL_LIBS) $(GSTINTERFACES_LIBS) $(GST_CONTROLLER_LIBS)
libgstsubrec_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstsubrec_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = audiotrim.h
