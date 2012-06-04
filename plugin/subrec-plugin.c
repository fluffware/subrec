#include "gstaudiotestsrc.h"
#include "audiormspower.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_audio_test_src_plugin_init (plugin)) return FALSE;
  if (!audio_rms_power_plugin_init (plugin)) return FALSE;
  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "subrec",
    "Plugins for subrec application",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
