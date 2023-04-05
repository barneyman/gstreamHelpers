/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2023 root <<user@hostname.org>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * SECTION:element-barrier
 *
 * FIXME:Describe barrier here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! barrier ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstbarrier.h"

GST_DEBUG_CATEGORY_STATIC (gst_barrier_debug);
#define GST_CAT_DEFAULT gst_barrier_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_SILENT
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("ANY")
    //GST_STATIC_CAPS ("text/x-raw")
    );

static GstStaticPadTemplate sink_factory_request = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS("ANY")
//    GST_STATIC_CAPS ("video/x-h264")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("ANY")
//    GST_STATIC_CAPS ("text/x-raw")
    );

static GstStaticPadTemplate src_factory_sometimes = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS("ANY")
//    GST_STATIC_CAPS ("video/x-h264")
    );

#define gst_barrier_parent_class parent_class
G_DEFINE_TYPE (Gstbarrier, gst_barrier, GST_TYPE_ELEMENT);

static void gst_barrier_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_barrier_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_barrier_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_barrier_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);
static gboolean gst_barrier_query(GstPad * pad,GstObject * parent,GstQuery * query);


GstPad* request_new_pad(GstElement *element, GstPadTemplate *templ,const gchar* name, const GstCaps *caps);
void release_pad(GstElement *element, GstPad *pad);


/* GObject vmethod implementations */

/* initialize the barrier's class */
static void
gst_barrier_class_init (GstbarrierClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_barrier_set_property;
  gobject_class->get_property = gst_barrier_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple(gstelement_class,
    "barrier",
    "FIXME:Generic",
    "FIXME:Generic Template Element",
    "root <<user@hostname.org>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory_sometimes));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory_request));

// https://github.com/Kurento/gstreamer/blob/master/gst/gstelement.h

/*
 GstPad*               (*request_new_pad)      (GstElement *element, GstPadTemplate *templ,
                                                 const gchar* name, const GstCaps *caps);

  void                  (*release_pad)          (GstElement *element, GstPad *pad);
*/

  gstelement_class->request_new_pad=GST_DEBUG_FUNCPTR(request_new_pad);
  gstelement_class->release_pad=GST_DEBUG_FUNCPTR(release_pad);

}


void setupSinkPad(Gstbarrier * filter,GstPad *sinkpad)
{
  gst_pad_set_event_function (sinkpad,
                              GST_DEBUG_FUNCPTR(gst_barrier_sink_event));
  gst_pad_set_chain_function (sinkpad,
                              GST_DEBUG_FUNCPTR(gst_barrier_chain));
  gst_pad_set_query_function(sinkpad,GST_DEBUG_FUNCPTR(gst_barrier_query));                              
  GST_PAD_SET_PROXY_CAPS (sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), sinkpad);

}




void setupSrcPad(Gstbarrier * filter,GstPad *srcpad)
{
  GST_PAD_SET_PROXY_CAPS (srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), srcpad);


}

GstPad* request_new_pad(GstElement *element, GstPadTemplate *templ,const gchar* name, const GstCaps *caps)
{
  Gstbarrier *filter = GST_BARRIER (element);

  GstPad *retPad=gst_pad_new_from_static_template (&sink_factory, "sink_%u");
  GstPad *srcPad=gst_pad_new_from_static_template (&src_factory, "src_%u");

  filter->pads.push_back(std::pair<GstPad*,GstPad*>(retPad,srcPad));

  setupSinkPad ( (filter), retPad);
  gst_element_add_pad (GST_ELEMENT (filter), srcPad);

  return retPad;
}
void release_pad(GstElement *element, GstPad *pad)
{
  Gstbarrier *filter = GST_BARRIER (element);

}




/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_barrier_init (Gstbarrier * filter)
{
  GstPad*sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  setupSinkPad(filter, sinkpad);

  GstPad*srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  setupSrcPad(filter,srcpad);

  filter->pads.push_back(std::pair<GstPad*, GstPad*>(sinkpad,srcpad));

  filter->silent = TRUE;
}

static void
gst_barrier_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  Gstbarrier *filter = GST_BARRIER (object);

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
gst_barrier_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstbarrier *filter = GST_BARRIER (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */
static
gboolean gst_barrier_query(GstPad * pad,
                        GstObject * parent,
                        GstQuery * query)
{
  Gstbarrier *filter;
  gboolean ret;

  filter = GST_BARRIER (parent);

  GST_DEBUG_OBJECT (filter, "Received %s query from %s" ,
      GST_QUERY_TYPE_NAME (query), GST_PAD_NAME(pad));

/*
  for(auto each=filter->pads.begin();each!=filter->pads.end();each++)
  {
    if(GST_QUERY_IS_DOWNSTREAM(query))
    {
      if(each->first==pad)
      {
        ret=gst_pad_peer_query(each->second,query);
        return ret;
      }
    }
    if(GST_QUERY_IS_UPSTREAM(query))
    {
      if(each->second==pad)
      {
        ret=gst_pad_peer_query(each->first,query);
        return ret;
      }
    }
  }
*/
  ret = gst_pad_query_default(pad,parent,query);
  return ret;
}                  


/* this function handles sink events */
static gboolean
gst_barrier_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  Gstbarrier *filter;
  gboolean ret;

  filter = GST_BARRIER (parent);

  GST_DEBUG_OBJECT (filter, "Received %s event: from pad %s" ,
      GST_EVENT_TYPE_NAME (event), GST_PAD_NAME(pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      GST_DEBUG_OBJECT(filter,"GST_EVENT_CAPS from %s of %s",GST_PAD_NAME(pad), gst_caps_to_string(caps));

      for(auto each=filter->pads.begin();each!=filter->pads.end();each++)
      {
        if(each->first==pad)
        {
          ret=gst_pad_push_event(each->second,event);
          return ret;
        }
      }

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:

      for(auto each=filter->pads.begin();each!=filter->pads.end();each++)
      {
        if(GST_EVENT_IS_DOWNSTREAM(event))
        {
          if(each->first==pad)
          {
            ret=gst_pad_push_event(each->second,event);
            return ret;
          }
        }
        
        if(GST_EVENT_IS_UPSTREAM(event))
        {
          if(each->second==pad)
          {
            ret=gst_pad_push_event(each->first,event);
            return ret;
          }
        }
      }


      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_barrier_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  Gstbarrier *filter;

  filter = GST_BARRIER (parent);

  if (filter->silent == FALSE)
    g_print ("I'm plugged, therefore I'm in.\n");

  /* just push out the incoming buffer without touching it */
  for(auto each=filter->pads.begin();each!=filter->pads.end();each++)
  {
    if(each->first==pad)
    {
      return gst_pad_push(each->second, buf);
    }
  }

  return GST_FLOW_NOT_LINKED;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
barrier_init (GstPlugin * barrier)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template barrier' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_barrier_debug, "barrier",
      0, "Template barrier");

  return gst_element_register (barrier, "barrier", GST_RANK_NONE,
      GST_TYPE_BARRIER);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstbarrier"
#endif

/* gstreamer looks for this structure to register barriers
 *
 * exchange the string 'Template barrier' with your barrier description
 */

void barrier_registerRunTimePlugin()
{
  gst_plugin_register_static(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "barrier",
    "Template barrier",
    barrier_init,
    "0.1",
    "Proprietary",
    "bjf",
    "source_bjf",
    PACKAGE );
}


// GST_PLUGIN_DEFINE (
//     GST_VERSION_MAJOR,
//     GST_VERSION_MINOR,
//     barrier,
//     "Template barrier",
//     barrier_init,
//     VERSION,
//     "LGPL",
//     "GStreamer",
//     "http://gstreamer.net/"
// )
