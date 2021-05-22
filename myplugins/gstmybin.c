#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/gstbin.h>
#include "gstmybin.h"

// given i've done this to get request pads running, lets declare one
static GstStaticPadTemplate gst_mybin_sink_template =
GST_STATIC_PAD_TEMPLATE ("request_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY
    );



// protos
void gst_mybin_set_property (GObject * object, guint property_id,const GValue * value, GParamSpec * pspec);
void gst_mybin_get_property (GObject * object, guint property_id,GValue * value, GParamSpec * pspec);
void gst_mybin_dispose (GObject * object);
void gst_mybin_finalize (GObject * object);
GstPad * gst_mybin_request_pad (GstElement * element,GstPadTemplate * templ,const gchar * name,const GstCaps * caps);



GST_DEBUG_CATEGORY_STATIC (gst_mybin_debug_category);
#define GST_CAT_DEFAULT gst_mybin_debug_category


G_DEFINE_TYPE_WITH_CODE (GstMyBin, gst_mybin, GST_TYPE_BIN,
  GST_DEBUG_CATEGORY_INIT (gst_mybin_debug_category, "mybin", 0,
  "debug category for mybin element"));


static void
gst_mybin_class_init (GstMyBinClass * klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GstBinClass *base_src_class = GST_BIN_CLASS (klass);

    gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
        &gst_mybin_sink_template);


    gobject_class->set_property = gst_mybin_set_property;
    gobject_class->get_property = gst_mybin_get_property;
    gobject_class->dispose = gst_mybin_dispose;
    gobject_class->finalize = gst_mybin_finalize;

    GstElementClass *element_class=GST_ELEMENT_CLASS(base_src_class);

    element_class->request_new_pad=gst_mybin_request_pad;

}


static void
gst_mybin_init (GstMyBin *mybin)
{
    mybin->myClassPointer=NULL;
}


void
gst_mybin_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMyBin *mybin = GST_MYBIN (object);

  GST_INFO_OBJECT (mybin, "set_property");

  switch (property_id) {
    // case PROP_FRAME_RATE:
    //   nmeasource->threadInfo.frameRate= g_value_get_int (value);
    //   break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_mybin_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstMyBin *mybin = GST_MYBIN (object);

  GST_INFO_OBJECT (mybin, "get_property");

  switch (property_id) {
    // case PROP_FRAME_RATE:
    //   g_value_set_int (value, nmeasource->threadInfo.frameRate);
    //   break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_mybin_dispose (GObject * object)
{
  GstMyBin *mybin = GST_MYBIN (object);

  GST_INFO_OBJECT (mybin, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_mybin_parent_class)->dispose (object);
}

void
gst_mybin_finalize (GObject * object)
{
  GstMyBin *mybin = GST_MYBIN (object);

  GST_INFO_OBJECT (mybin, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_mybin_parent_class)->finalize (object);
}

GstPad *
gst_mybin_request_pad (GstElement * element,
                 GstPadTemplate * templ,
                 const gchar * name,
                 const GstCaps * caps)
{
  GstMyBin *mybin = GST_MYBIN (element);  
  if(mybin->myClassPointer)
    return mybin->myClassPointer->request_new_pad(element,templ,name,caps);
  return NULL;
}


static gboolean
plugin_init (GstPlugin * plugin)
{

  /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register (plugin, "mybin", GST_RANK_NONE,
      GST_TYPE_MYBIN);
}


#ifndef VERSION
#define VERSION "0.0.FIXME"
#endif
#ifndef PACKAGE
#define PACKAGE "FIXME_package"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "FIXME_package_name"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://FIXME.org/"
#endif

void gst_mybin_registerRunTimePlugin()
{
  gst_plugin_register_static(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "mybin",
    "Template mybin",
    plugin_init,
    "0.1",
    "Proprietary",
    "bjf",
    "source_bjf",
    PACKAGE );
}
