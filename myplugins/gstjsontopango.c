/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2019 root <<user@hostname.org>>
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
 * SECTION:element-jsontopango
 *
 * FIXME:Describe jsontopango here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! jsontopango ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstjsontopango.h"

#include <string.h>
#include <stdio.h>

#include "../json/json.hpp"



///
/// https://gstreamer.freedesktop.org/data/doc/gstreamer/head/pwg/pwg.pdf
/// 


GST_DEBUG_CATEGORY_STATIC (gst_json_to_pango_debug);
#define GST_CAT_DEFAULT gst_json_to_pango_debug

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
  PROP_MODE
};


#define MODE_GPS_LOCATION   0
#define MODE_SPEED_BRG      (MODE_GPS_LOCATION+1)
#define MODE_TIME           (MODE_SPEED_BRG+1)

#define DEFAULT_MODE        MODE_GPS_LOCATION
#define MIN_MODE            MODE_GPS_LOCATION
#define MAX_MODE            MODE_TIME

static GstStaticPadTemplate text_src_factory = GST_STATIC_PAD_TEMPLATE ("text_src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/x-raw,format=pango-markup")
    );

// unfortunately, the matroskamux will accept utf8, but the demuxer will spit out pango!
#define _USE_UTF8_SUBS 
//#define _USE_PANGO_SUBS

#define _USE_FIXED_CAPS



static GstStaticPadTemplate sub_sink_factory = GST_STATIC_PAD_TEMPLATE ("subtitle_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
#ifdef _USE_UTF8_SUBS        
    GST_STATIC_CAPS ("text/x-raw,format=utf8")
#elif defined(_USE_PANGO_SUBS)    
    GST_STATIC_CAPS ("text/x-raw,format=pango-markup")
#else
    GST_STATIC_CAPS ("application/x-subtitle-unknown")
#endif    
    );


#define gst_json_to_pango_parent_class parent_class
G_DEFINE_TYPE (GstMyFirstFilter, gst_json_to_pango, GST_TYPE_ELEMENT);

static void gst_json_to_pango_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_json_to_pango_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_json_to_pango_sink_event_json (GstPad * pad, GstObject * parent, GstEvent * event);

static gboolean gst_json_to_pango_subsink_query (GstPad * pad, GstObject * parent, GstQuery * event);

static GstFlowReturn gst_json_to_pango_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);
static GstFlowReturn gst_json_to_pango_subsink_chain(GstPad * pad, GstObject * parent, GstBuffer * buf);

void negotiate_set_my_caps(GstMyFirstFilter*);

GstIterator *myFirstFilterIterateLinkedPads (GstPad * pad, GstObject * parent);

/* GObject vmethod implementations */

/* initialize the jsontopango's class */
static void
gst_json_to_pango_class_init (GstMyFirstFilterClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_json_to_pango_set_property;
  gobject_class->get_property = gst_json_to_pango_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_int ("mode", "Mode", "Change the output",
          MIN_MODE,MAX_MODE,DEFAULT_MODE, 
          G_PARAM_READWRITE));

          

  gst_element_class_set_details_simple(gstelement_class,
    "MyFirstFilter",
    "FIXME:Generic",
    "FIXME:Generic Template Element",
    "Barney <<barnaby.james.flint@gmail.com>>");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&text_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sub_sink_factory));

      

}


/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_json_to_pango_init (GstMyFirstFilter * filter)
{
  // pango source
  filter->pango_srcpad = gst_pad_new_from_static_template (&text_src_factory, "pango_src");
#ifdef _USE_FIXED_CAPS  
  gst_pad_use_fixed_caps(filter->pango_srcpad);
#endif  
  gst_element_add_pad (GST_ELEMENT (filter), filter->pango_srcpad);
  GST_PAD_ITERINTLINKFUNC(filter->pango_srcpad)=myFirstFilterIterateLinkedPads;


  // subtitle sink
  filter->json_sinkpad = gst_pad_new_from_static_template (&sub_sink_factory, "json_sink");
#ifdef _USE_FIXED_CAPS  
  gst_pad_use_fixed_caps(filter->json_sinkpad);
#endif  


  gst_pad_set_query_function(filter->json_sinkpad,GST_DEBUG_FUNCPTR(gst_json_to_pango_subsink_query));
  gst_element_add_pad (GST_ELEMENT (filter), filter->json_sinkpad);
  gst_pad_set_chain_function (filter->json_sinkpad, GST_DEBUG_FUNCPTR(gst_json_to_pango_subsink_chain));
  gst_pad_set_event_function (filter->json_sinkpad, GST_DEBUG_FUNCPTR(gst_json_to_pango_sink_event_json));
  GST_PAD_ITERINTLINKFUNC(filter->json_sinkpad)=myFirstFilterIterateLinkedPads;


  // construct and populate the lists
  filter->subSinks=NULL;
  filter->textSources=NULL;

  filter->subSinks=g_list_append(filter->subSinks,filter->json_sinkpad);
  filter->textSources=g_list_append(filter->textSources,filter->pango_srcpad);


  filter->silent = TRUE;
  filter->jsonEOS=FALSE;
  filter->jsonFlush=FALSE;

  filter->mode=DEFAULT_MODE;

  filter->pango_srcpad_negotiated =false;

  filter->frameNumber=0;
}


// because i want to use GST_PAD_SET_PROXY_CAPS  (because it does a lot of useful plumbing)
// i have to have an iterator that internally links the VIDEO pads, and ignores the text_src.
// deep in the bowels of gstpad is the gst_pad_iterate_internal_links - i'm putting my own in
// that explicitly links only VIDEO pads


GstIterator *myFirstFilterIterateLinkedPads (GstPad * pad, GstObject * parent)
{
  GstIterator *res;
  GList **padlist;
  guint32 *cookie;
  GMutex *lock;
  gpointer owner;
  GstElement *eparent;
  GstMyFirstFilter * filter;


  g_return_val_if_fail (GST_IS_PAD (pad), NULL);

  if (parent != NULL && GST_IS_ELEMENT (parent)) {
    eparent = GST_ELEMENT_CAST (gst_object_ref (parent));
  } else {
    GST_OBJECT_LOCK (pad);
    eparent = GST_PAD_PARENT (pad);
    if (!eparent || !GST_IS_ELEMENT (eparent))
      goto no_parent;

    gst_object_ref (eparent);
    GST_OBJECT_UNLOCK (pad);
  }

  filter=GST_JSONTOPANGO(eparent);

  if(pad==filter->pango_srcpad)
      padlist = &filter->subSinks;
  else
      padlist = &filter->textSources;


  GST_DEBUG_OBJECT (pad, "Making iterator for %s", GST_ELEMENT_NAME(pad));

  cookie = &eparent->pads_cookie;
  owner = eparent;
  lock = GST_OBJECT_GET_LOCK (eparent);

  res = gst_iterator_new_list (GST_TYPE_PAD,lock, cookie, padlist, (GObject *) owner, NULL);

  gst_object_unref (owner);

  return res;

  /* ERRORS */
no_parent:
  {
    GST_OBJECT_UNLOCK (pad);
    GST_DEBUG_OBJECT (pad, "no parent element");
    return NULL;
  }
}


static void
gst_json_to_pango_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMyFirstFilter *filter = GST_JSONTOPANGO (object);

  switch (prop_id) {
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    case PROP_MODE:
      filter->mode = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_json_to_pango_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMyFirstFilter *filter = GST_JSONTOPANGO (object);

  switch (prop_id) {
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    case PROP_MODE:
      g_value_set_int (value, filter->mode);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

#ifndef _USE_FIXED_CAPS
static GstCaps* getMyCaps()
{
#ifdef _USE_UTF8_SUBS  
  return gst_caps_new_simple ("text/x-raw", "format", G_TYPE_STRING, "pango-markup", NULL);
#else
  return gst_caps_new_simple ("application/x-subtitle-unknown", NULL);
#endif
  
}
#endif

/* GstElement vmethod implementations */


static gboolean gst_json_to_pango_subsink_query(GstPad * pad, GstObject * parent, GstQuery * query)
{

  GstMyFirstFilter *filter;
  gboolean ret;

  filter = GST_JSONTOPANGO (parent);

   GST_LOG_OBJECT (filter, "Received %s query: %" GST_PTR_FORMAT, GST_QUERY_TYPE_NAME (query), query);

  switch (GST_QUERY_TYPE (query)) {

#ifndef _USE_FIXED_CAPS
      case GST_QUERY_CAPS:
      {
        GstCaps  *caps=getMyCaps();

        gst_query_set_caps_result (query, caps);

        gst_caps_unref (caps);
        ret = TRUE;

        break;
      }
#endif
      default:
        ret=gst_pad_query_default  (pad, parent, query);
        break;
    }

    return ret;

}


/* this function handles sink events */
/* for text sink */
static gboolean gst_json_to_pango_sink_event_json (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMyFirstFilter *filter;
  gboolean ret;

  filter = GST_JSONTOPANGO (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {

    case GST_EVENT_SEGMENT:

        // and let default do the heavy lifting
        negotiate_set_my_caps(filter);
        ret = gst_pad_event_default (pad, parent, event);

        break;



      case GST_EVENT_EOS:
        // gst_pad_push_event(filter->video_srcpad, gst_event_new_eos());
        // gst_pad_push_event(filter->pango_srcpad, gst_event_new_eos());
        GST_DEBUG_OBJECT (filter, "pushing EOS down text sink\n");
        filter->jsonEOS=TRUE;
        ret = gst_pad_event_default (pad, parent, event);
        break;

      case GST_EVENT_FLUSH_START:
        filter->jsonFlush=FALSE;
        ret=TRUE;
        break;

    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}


// matroska translates utf8 into pango, and escapes any html chars - daft
// TODO - push application/x-subtitle-unknown into matroskamux
// TODO - change matrsokademux to not escape utf8 into pango

static const struct {
    const char* encodedEntity;
    const char *decodedChar;
} entityToChars[] ={
    {"&lt;", "<"},
    {"&gt;", ">"},
    {"&amp;", "&"},
    {"&quot;", "\""},
    { NULL, NULL }
};

bool detoken(std::string &source)
{
  for(unsigned each=0;entityToChars[each].encodedEntity;each++)
  {
    for(size_t found=source.find(entityToChars[each].encodedEntity);
        found!=std::string::npos;
        found=source.find(entityToChars[each].encodedEntity))
    {
      source.replace( found,
        strlen(entityToChars[each].encodedEntity),
        entityToChars[each].decodedChar
        );
    }
  }
  return true;
}

void negotiate_set_my_caps(GstMyFirstFilter *filter)
{
  if(!filter->pango_srcpad_negotiated)
  {
    // ask for caps
    GstCaps  *caps=gst_caps_new_simple ("text/x-raw","format",G_TYPE_STRING,"pango-markup", NULL);

    GstEvent *capRequest=gst_event_new_caps (caps);

    gst_pad_push_event(filter->pango_srcpad, capRequest);

    filter->pango_srcpad_negotiated=true;

  }

}


static GstFlowReturn
gst_json_to_pango_subsink_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstMyFirstFilter *filter;

  filter = GST_JSONTOPANGO (parent);

  negotiate_set_my_caps(filter);
  // if(!filter->pango_srcpad_negotiated)
  // {
  //   // ask for caps
  //   GstCaps  *caps=gst_caps_new_simple ("text/x-raw","format",G_TYPE_STRING,"pango-markup", NULL);

  //   GstEvent *capRequest=gst_event_new_caps (caps);

  //   gst_pad_push_event(filter->pango_srcpad, capRequest);

  //   filter->pango_srcpad_negotiated=true;

  // }


  if(filter->jsonEOS)
  {
    return GST_FLOW_EOS;
  }


  GstMemory *memory=gst_buffer_peek_memory(buf,0);

  


  GstMapInfo info;
  gst_memory_map(memory,&info,GST_MAP_READ);
  std::string jsonString((const char*)info.data, info.size);

  // detoken the pango that matroska gives us
  detoken(jsonString);



  auto j3 = nlohmann::json::parse(jsonString.c_str());

  if (filter->silent == FALSE)
  {
    GST_INFO_OBJECT (filter, "I'm plugged, therefore I'm in. SUBTITLE - %.*s Json %s\n",(int)info.size,(const char*)info.data, j3.dump().c_str());
  }


  // what we're sending
  #define PANGO_BUFFER  200
  char msg[PANGO_BUFFER];
  int len=0;

  // if there's a 'msg' - print that exclusively
  if(j3.contains("msg"))
  {
    len=snprintf(msg, sizeof(msg), "<span foreground=\"red\" size=\"small\">%s</span>",
      j3["msg"].get<std::string>().c_str()
    );
  }
  else
  {
    // pango - https://developer.gnome.org/pygtk/stable/pango-markup-language.html#:~:text=The%20pango%20markup%20language%20is,%3Ecool!
    switch(filter->mode)
    {

      case MODE_GPS_LOCATION:
        len=snprintf(msg, sizeof(msg), "<span foreground=\"white\" size=\"small\">Long:%.4f Lat:%.4f</span>",
          j3["longitudeE"].get<float>(),
          j3["latitudeN"].get<float>()
          );
        break;
      case MODE_SPEED_BRG:
        len=snprintf(msg, sizeof(msg), "<span foreground=\"white\" size=\"small\">%d km/h %.1f° %2d sats</span>",
          j3["speedKMH"].get<int>(),
          j3["bearingDeg"].get<float>(),
          j3["satelliteCount"].get<int>()
          );
        break;
      case MODE_TIME:
        len=snprintf(msg, sizeof(msg), "<span foreground=\"white\" size=\"small\">%s</span>",
          j3["utc"].get<std::string>().c_str()
          );
        break;
      default:
        len=snprintf(msg, sizeof(msg), "<span foreground=\"white\" size=\"small\">Speed: %d kmh Long: %.4f Lat: %.4f bearing: %.1f</span>",
          j3["speedKMH"].get<int>(),
          j3["longitudeE"].get<float>(),
          j3["latitudeN"].get<float>(),
          j3["bearingDeg"].get<float>()
          );
          break;

    }

  }

  if(len<0 || len>PANGO_BUFFER)
  {
    // ouch
    GST_ERROR_OBJECT (filter, "Exceeded buffer length"); 
    len=0;
  }
  else
  {
    if (filter->silent == FALSE)
    {
      g_print ("%s\n",msg);
    }
    GST_LOG_OBJECT (filter, "Sending %s", msg);
  }

  
  /* make empty buffer */
  GstBuffer *textBuffer = gst_buffer_new_and_alloc(len+1);
  gst_buffer_fill (textBuffer, 0, msg, len+1);
  gst_buffer_set_size (textBuffer, len);

  // set dts, pts and dur for our buffer
  gst_buffer_copy_into(textBuffer, buf, GST_BUFFER_COPY_TIMESTAMPS ,0,-1);

  return gst_pad_push(filter->pango_srcpad, textBuffer);

}



/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
jsontopango_init (GstPlugin * jsontopango)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template jsontopango' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_json_to_pango_debug, "jsontopango",
      0, "Template jsontopango");

  return gst_element_register (jsontopango, "jsontopango", GST_RANK_NONE,
      GST_TYPE_JSONTOPANGO);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstjsontopango"
#endif


/* gstreamer looks for this structure to register jsontopangos
 *
 * exchange the string 'Template jsontopango' with your jsontopango description
 */


void jsontopango_registerRunTimePlugin()
{
  gst_plugin_register_static(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "jsontopango",
    "Template jsontopango",
    jsontopango_init,
    "0.1",
    "Proprietary",
    "bjf",
    "source_bjf",
    PACKAGE );
}

/*
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    jsontopango,
    "Template jsontopango",
    jsontopango_init,
    PACKAGE_VERSION,
    GST_LICENSE,
    GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)
*/
