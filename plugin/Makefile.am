ACLOCAL_AMFLAGS="-I $(top_srcdir)/m4" 
plugin_LTLIBRARIES = libgstaudiotrim.la libgstaudiormspower.la

libgstaudiotrim_la_SOURCES = audiotrim.c

libgstaudiotrim_la_CFLAGS = $(GST_CFLAGS) -std=c99
libgstaudiotrim_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(GSTAUDIO_LIBS) $(GSTCTRL_LIBS) $(GSTINTERFACES_LIBS)
libgstaudiotrim_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstaudiotrim_la_LIBTOOLFLAGS = --tag=disable-static

libgstaudiormspower_la_SOURCES = audiormspower.c
libgstaudiormspower_la_CFLAGS = $(GST_CFLAGS) -std=c99
libgstaudiormspower_la_LIBADD = $(GST_LIBS) $(GST_BASE_LIBS) $(GSTAUDIO_LIBS) $(GSTCTRL_LIBS) $(GSTINTERFACES_LIBS)
libgstaudiormspower_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstaudiormspower_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = audiotrim.h
