ACLOCAL_AMFLAGS="-I $(top_srcdir)/m4" 
plugin_LTLIBRARIES = libgstaudiotrim.la

libgstaudiotrim_la_SOURCES = audiotrim.c

libgstaudiotrim_la_CFLAGS = $(GST_CFLAGS) -std=c99
libgstaudiotrim_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(GSTAUDIO_LIBS) $(GSTCTRL_LIBS) $(GSTINTERFACES_LIBS)
libgstaudiotrim_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstaudiotrim_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = audiotrim.h
