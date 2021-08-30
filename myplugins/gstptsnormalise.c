/*
 * GStreamer
 * Copyright (C) 2006 Stefan Kost <ensonic@users.sf.net>
 * Copyright (C) 2021 pi <<user@hostname.org>>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-ptsnormalise
 *
 * FIXME:Describe ptsnormalise here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! ptsnormalise ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/base.h>
#include <gst/controller/controller.h>

#include "gstptsnormalise.h"

GST_DEBUG_CATEGORY_STATIC (gst_ptsnormalise_debug);
#define GST_CAT_DEFAULT gst_ptsnormalise_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT,
};

/* the capabilities of the inputs and outputs.
 *
 * FIXME:describe the real formats here.
 */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_ptsnormalise_parent_class parent_class
G_DEFINE_TYPE (Gstptsnormalise, gst_ptsnormalise, GST_TYPE_BASE_TRANSFORM);

static void gst_ptsnormalise_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_ptsnormalise_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_ptsnormalise_transform_ip (GstBaseTransform *
    base, GstBuffer * outbuf);

/* GObject vmethod implementations */

/* initialize the ptsnormalise's class */
static void
gst_ptsnormalise_class_init (GstptsnormaliseClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_ptsnormalise_set_property;
  gobject_class->get_property = gst_ptsnormalise_get_property;

  gst_element_class_set_details_simple (gstelement_class,
      "ptsnormalise",
      "Generic/Filter",
      "FIXME:Generic Template Filter", "pi <<user@hostname.org>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

  GST_BASE_TRANSFORM_CLASS (klass)->transform_ip =
      GST_DEBUG_FUNCPTR (gst_ptsnormalise_transform_ip);

  /* debug category for fltering log messages
   *
   * FIXME:exchange the string 'Template ptsnormalise' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_ptsnormalise_debug, "ptsnormalise", 0,
      "Template ptsnormalise");
}

/* initialize the new element
 * initialize instance structure
 */
static void
gst_ptsnormalise_init (Gstptsnormalise * filter)
{
  filter->silent = FALSE;
  filter->normal = GST_CLOCK_TIME_NONE;

  GstBaseTransform *base=GST_BASE_TRANSFORM(filter);

  gst_base_transform_set_in_place(base, TRUE);


}

static void
gst_ptsnormalise_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstptsnormalise *filter = GST_PTSNORMALISE (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ptsnormalise_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstptsnormalise *filter = GST_PTSNORMALISE (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstBaseTransform vmethod implementations */

/* this function does the actual processing
 */
static GstFlowReturn
gst_ptsnormalise_transform_ip (GstBaseTransform * base, GstBuffer * outbuf)
{
  Gstptsnormalise *filter = GST_PTSNORMALISE (base);


  if(filter->normal==GST_CLOCK_TIME_NONE)
  {
    // first time round
    filter->normal=outbuf->pts;
  }

  //outbuf->dts=outbuf->pts-=filter->normal;


  if (GST_CLOCK_TIME_IS_VALID (GST_BUFFER_TIMESTAMP (outbuf)))
    gst_object_sync_values (GST_OBJECT (filter), GST_BUFFER_TIMESTAMP (outbuf));

  /* FIXME: do something interesting here.  This simply copies the source
   * to the destination. */

  

  GST_DEBUG_OBJECT (filter, "normalised PTS= %" GST_TIME_FORMAT " dts %" GST_TIME_FORMAT "" , GST_TIME_ARGS(outbuf->pts),GST_TIME_ARGS(outbuf->dts));

  return GST_FLOW_OK;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
ptsnormalise_init (GstPlugin * ptsnormalise)
{
  //return GST_ELEMENT_REGISTER (ptsnormalise, ptsnormalise);

  return gst_element_register (ptsnormalise, "ptsnormalise", GST_RANK_NONE, GST_TYPE_PTSNORMALISE);

}

/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
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


/* gstreamer looks for this structure to register ptsnormalises
 *


 
 * FIXME:exchange the string 'Template ptsnormalise' with you ptsnormalise description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    ptsnormalise,
    "ptsnormalise",
    ptsnormalise_init,
    VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)


void ptsnormalise_registerRunTimePlugin()
{
  gst_plugin_register_static(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "ptsnormalise",
    "Template ptsnormalise",
    ptsnormalise_init,
    "0.1",
    "Proprietary",
    "bjf",
    "source_bjf",
    PACKAGE );
}