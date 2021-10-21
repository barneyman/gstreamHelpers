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
 * SECTION:element-jsoninject
 *
 * FIXME:Describe jsoninject here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! jsoninject ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstjsoninject.h"

#include <string.h>
#include <stdio.h>

#include <unistd.h>



GST_DEBUG_CATEGORY_STATIC (gst_json_inject_debug);
#define GST_CAT_DEFAULT gst_json_inject_debug

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
  PROP_SUBTITLE_FORMAT,
  PROP_TIMEOFFSET_NS
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("vidsink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("vidsrc",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw")
    );

// unfortunately, the matroskamux will accept utf8, but the demuxer will spit out pango!
#define _USE_UTF8_SUBS    
//#define _USE_PANGO_SUBS    

//#define _USE_FIXED_CAPS

static GstStaticPadTemplate subtitle_factory = GST_STATIC_PAD_TEMPLATE ("subtitlesrc",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    
#ifdef _USE_UTF8_SUBS    
    GST_STATIC_CAPS ("text/x-raw,format=utf8")
#elif defined(_USE_PANGO_SUBS)
    GST_STATIC_CAPS ("text/x-raw,format=pango")
#else
    GST_STATIC_CAPS ("application/x-subtitle-unknown")
#endif
    );


#define gst_json_inject_parent_class parent_class
G_DEFINE_TYPE (GstjsonInject, gst_json_inject, GST_TYPE_ELEMENT);

static void gst_json_inject_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_json_inject_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_json_inject_subsrc_query (GstPad * pad, GstObject * parent, GstQuery * event);
static gboolean gst_json_inject_subsrc_event (GstPad * pad, GstObject * parent, GstEvent * event);
static gboolean gst_json_inject_videosink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_json_inject_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

GstIterator *jsonInjectIiterateLinkedVideoPads (GstPad * pad, GstObject * parent);

static GstStateChangeReturn gst_my_filter_change_state (GstElement *element, GstStateChange transition);




/* GObject vmethod implementations */

/* initialize the jsoninject's class */
static void
gst_json_inject_class_init (GstjsonInjectClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_json_inject_set_property;
  gobject_class->get_property = gst_json_inject_get_property;
 
  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SUBTITLE_FORMAT,
      g_param_spec_boolean ("subformat", "SubFormat", "UTF8 TRUE, unknown FALSE",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TIMEOFFSET_NS,
      g_param_spec_uint64 ("offset", "offset", "baseline offset nanoseconds",
          0,-1,0, G_PARAM_READWRITE));

  gst_element_class_set_details_simple(gstelement_class,
    "jsonInject",
    "FIXME:Generic",
    "FIXME:Generic Template Element",
    "root <<user@hostname.org>>");

  gstelement_class->change_state = gst_my_filter_change_state;



  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&subtitle_factory));



}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_json_inject_init (GstjsonInject * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "videosink");
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_json_inject_videosink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_json_inject_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);

  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);
  GST_PAD_ITERINTLINKFUNC(filter->sinkpad)=jsonInjectIiterateLinkedVideoPads;

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "videosrc");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);

  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
  GST_PAD_ITERINTLINKFUNC(filter->srcpad)=jsonInjectIiterateLinkedVideoPads;

  filter->subpad = gst_pad_new_from_static_template (&subtitle_factory, "sub_src");
  gst_pad_set_query_function(filter->subpad, GST_DEBUG_FUNCPTR(gst_json_inject_subsrc_query));
  gst_pad_set_event_function (filter->subpad,GST_DEBUG_FUNCPTR(gst_json_inject_subsrc_event));
#ifdef _USE_FIXED_CAPS  
  gst_pad_use_fixed_caps(filter->subpad);
#endif
  gst_element_add_pad (GST_ELEMENT (filter), filter->subpad);

  // construct and populate the lists
  filter->videoSinks=NULL;
  filter->videoSources=NULL;

  filter->videoSinks=g_list_append(filter->videoSinks,filter->sinkpad);
  filter->videoSources=g_list_append(filter->videoSources,filter->srcpad);

  filter->silent = true;
  filter->capsNegotiated = false;
  filter->utf8 = true;

  filter->frameNumber=0;

  filter->offset=0;

  filter->ptsSeenLast-GST_CLOCK_TIME_NONE;



}

// GST_STATE_CHANGE_FAILURE GST_STATE_CHANGE_SUCCESS
static GstStateChangeReturn
gst_my_filter_change_state (GstElement *element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstjsonInject *filter = GST_JSONINJECT (element);

  GST_INFO_OBJECT (filter, "changing state");

  switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
    // spin the thread up
    startMonitorThread(filter->threadInfo);

    GST_INFO_OBJECT (filter, "starting thread");

	  break;
	default:
	  break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
  {
	  return ret;
  }

  switch (transition) {
	case GST_STATE_CHANGE_READY_TO_NULL:
	  stopMonitorThread(filter->threadInfo);

    GST_INFO_OBJECT (filter, "ending thread");

	  break;
	default:
	  break;
  }

  return ret;
}


// because i want to use GST_PAD_SET_PROXY_CAPS  (because it does a lot of useful plumbing)
// i have to have an iterator that internally links the VIDEO pads, and ignores the text_src.
// deep in the bowels of gstpad is the gst_pad_iterate_internal_links - i'm putting my own in
// that explicitly links only VIDEO pads


GstIterator *jsonInjectIiterateLinkedVideoPads (GstPad * pad, GstObject * parent)
{
  GstIterator *res;
  GList **padlist;
  guint32 *cookie;
  GMutex *lock;
  gpointer owner;
  GstElement *eparent;
  GstjsonInject * filter;


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

  filter=GST_JSONINJECT(parent);

  if (pad==filter->srcpad)
    padlist = &filter->videoSinks;
  else
    padlist = &filter->videoSources;

  GST_INFO_OBJECT (pad, "Making iterator");

  cookie = &eparent->pads_cookie;
  owner = eparent;
  lock = GST_OBJECT_GET_LOCK (eparent);

  res = gst_iterator_new_list (GST_TYPE_PAD,
      lock, cookie, padlist, (GObject *) owner, NULL);

  gst_object_unref (owner);

  return res;

  /* ERRORS */
no_parent:
  {
    GST_OBJECT_UNLOCK (pad);
    GST_INFO_OBJECT (pad, "no parent element");
    return NULL;
  }
}



static void
gst_json_inject_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstjsonInject *filter = GST_JSONINJECT (object);

  switch (prop_id) {
    case PROP_SUBTITLE_FORMAT:
      filter->utf8 = g_value_get_boolean (value);
      break;
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    case PROP_TIMEOFFSET_NS:
      filter->offset=g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_json_inject_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstjsonInject *filter = GST_JSONINJECT (object);

  switch (prop_id) {
    case PROP_SUBTITLE_FORMAT:
      g_value_set_boolean (value, filter->utf8);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    case PROP_TIMEOFFSET_NS:
      g_value_set_uint64 (value, filter->offset);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

static GstCaps* getMyCaps()
{
#ifdef _USE_UTF8_SUBS  
  return gst_caps_new_simple ("text/x-raw", "format", G_TYPE_STRING, "utf8", NULL);
#elif defined(_USE_PANGO_SUBS)
  return gst_caps_new_simple ("text/x-raw", "format", G_TYPE_STRING, "pango", NULL);
#else
  return gst_caps_new_simple ("application/x-subtitle-unknown", NULL);
#endif
  
}

bool NegotiateSubCaps(GstjsonInject *filter)
{
  if(!filter->capsNegotiated)
  {
#ifndef _USE_FIXED_CAPS 

    GstCaps *myCaps=getMyCaps();

    // query the caps
    GstCaps *srcCaps=gst_pad_peer_query_caps(filter->subpad, myCaps);

    GST_INFO_OBJECT (filter,"peer caps '%s'", gst_caps_to_string(srcCaps) );

    GstCaps *intersect = gst_caps_intersect (srcCaps, myCaps);
    GST_INFO_OBJECT (filter,"intersect caps '%s'", gst_caps_to_string(intersect) );

    // finished with them
    gst_caps_unref(myCaps);
    gst_caps_unref(srcCaps);

    // see if they will accept it
    gboolean acceptable=gst_pad_peer_query_accept_caps(filter->subpad, intersect);

    GstCaps *fixated=gst_caps_fixate(intersect);
    GST_INFO_OBJECT (filter,"fixated caps '%s'", gst_caps_to_string(fixated) );

    if (!gst_pad_set_caps (filter->subpad, fixated)) 
    {
      GST_WARNING_OBJECT (filter, "Failed to set caps in NegotiateSubCaps");
      return false;
    }

#else
    gst_pad_set_caps(filter->subpad, getMyCaps());
#endif

    filter->capsNegotiated=TRUE;
  }
  return true;
}


static gboolean gst_json_inject_subsrc_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean ret;

  GstjsonInject *filter = GST_JSONINJECT (parent);

   GST_INFO_OBJECT (filter, "Received %s query: %" GST_PTR_FORMAT,
      GST_QUERY_TYPE_NAME (query), query);

  GST_INFO_OBJECT (filter,"gst_json_inject_subsrc_query %s to %s",  GST_QUERY_TYPE_NAME (query), GST_ELEMENT_NAME(pad));


  switch (GST_QUERY_TYPE (query)) {

      case GST_QUERY_CAPS:
        // stolen from https://gstreamer.freedesktop.org/documentation/plugin-development/advanced/negotiation.html?gi-language=c#implementing-a-caps-query-function

          {
            GstCaps *filt=NULL;
            gst_query_parse_caps (query, &filt);

            // dont care about downstream caps
            // ...
            GstCaps *caps=NULL;

            // get our template caps
            GstCaps *templateCaps=gst_pad_get_pad_template_caps (pad);

            if (templateCaps) 
            {
              GST_INFO_OBJECT (filter,"my template caps '%s'", gst_caps_to_string(templateCaps) );

              if(caps)
              {
                GstCaps *temp = gst_caps_intersect (caps, templateCaps);
                // release unused caps
                gst_caps_unref (caps);
                // and move forward 
                caps = temp;
                // let go of template
                gst_caps_unref (templateCaps);
              }
              else
              {
                // don't let go of template
                caps=templateCaps;
              }

            }            

            // if the requestor has put in a filter
            if(filt)
            {
              GST_INFO_OBJECT (filter,"filter caps '%s'", gst_caps_to_string(filt) );

              if(caps)
              {
                // get the intersection
                GstCaps *temp = gst_caps_intersect (caps, filt);
                // loose
                gst_caps_unref (caps);
                // remember
                caps = temp;
              }
              else
              {
                caps=gst_caps_new_empty();
              }
            }

            GST_INFO_OBJECT (filter,"returning caps '%s'", gst_caps_to_string(caps) );


            // and fill the return
            gst_query_set_caps_result (query, caps);

            // it's taken a ref, so release ours
            gst_caps_unref (caps);
            ret = TRUE;

          }

        break;

      default:
        ret = gst_pad_query_default  (pad, parent, query);
        break;
    }

    return ret;
}


/* handles events on the subtitle source */

static gboolean gst_json_inject_subsrc_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstjsonInject *filter;
  gboolean ret;

  filter = GST_JSONINJECT (parent);

  GST_INFO_OBJECT (filter, "Received vid sink %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  GST_INFO_OBJECT (filter,"gst_json_inject_subsrc_event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) 
  {
    case GST_EVENT_RECONFIGURE:
      filter->capsNegotiated=FALSE;
      ret=TRUE;
      break;
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
      
  }

  return ret;

}


/* this function handles sink events */
static gboolean
gst_json_inject_videosink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstjsonInject *filter;
  gboolean ret;

  filter = GST_JSONINJECT (parent);

  GST_INFO_OBJECT (filter, "Received vid sink %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  GST_INFO_OBJECT (filter,"gst_json_inject_videosink_event %s",GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) 
  {

    case GST_EVENT_CAPS:

      {
        GstCaps*eventCaps;
        gst_event_parse_caps(event,&eventCaps);

        GST_INFO_OBJECT (filter,"event caps '%s'", gst_caps_to_string(eventCaps) );

        // send this straight downstream
        gst_pad_set_caps (filter->srcpad, eventCaps);


      }

      //ret = gst_pad_event_default (filter->srcpad, parent, event);
      //ret=gst_pad_event_default (pad, parent, event);
      ret=TRUE;

      break;

    case GST_EVENT_STREAM_START:

      // get the streamid
      {
        const gchar *streamId=NULL;
        gst_event_parse_stream_start(event,&streamId);

        gst_pad_push_event(filter->srcpad,gst_event_new_stream_start(streamId));

        gchar *substreamId=gst_pad_create_stream_id(filter->subpad,GST_ELEMENT(parent),"sub");

        gst_pad_push_event(filter->subpad,gst_event_new_stream_start(substreamId));

        ret=TRUE;
      }

      break;


    case GST_EVENT_SEGMENT_DONE:
        {
          GstFormat format;
          gint64 position;

          gst_event_parse_segment_done ( event, &format, &position);

          gst_pad_push_event (filter->srcpad, gst_event_new_segment_done (format, position));
          gst_pad_push_event (filter->subpad, gst_event_new_segment_done (format, position));

          ret=TRUE;

          break;
        }
    case GST_EVENT_SEGMENT:
      // send it to the text overlay
      NegotiateSubCaps(filter);
      {
        const GstSegment *segment;
        gst_event_parse_segment (event, &segment);

        if (segment->format == GST_FORMAT_TIME) 
        {
          gst_pad_push_event (filter->srcpad, gst_event_new_segment (segment));
          gst_pad_push_event (filter->subpad, gst_event_new_segment (segment));
        } 
        else 
        {
          GST_WARNING_OBJECT (filter, "received non-TIME newsegment event on text input");
        }


        ret=TRUE;

      }
      break;    

      case GST_EVENT_EOS:
        // this will get pushed down the video stream for free
        // but cos i'm the source (of the subtitle) i need
        // to propogate that from my sink
        gst_pad_push_event(filter->srcpad, gst_event_new_eos());
        gst_pad_push_event(filter->subpad, gst_event_new_eos());
        GST_INFO_OBJECT (filter, "pushing EOS down subtitle sink");
        ret=TRUE;
        break;

    default:
        ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

/*

#include "nmeaparse/NMEAParser.h"
#include "nmeaparse/GPSService.h"
#include "json/json.hpp"
#include <fcntl.h>


void pushJson(GstjsonInject *filter, nlohmann::json &jsonData)
{
    // get the element clock
    GstClock *currentClock=gst_element_get_clock  ((GstElement*)filter);

    // get the mutex
    {
      std::lock_guard<std::mutex> guard(filter->threadInfo.gpsMutex);

      // dump the string
      filter->threadInfo.sample.gpsOutput=jsonData.dump();
      filter->threadInfo.sample.pts=currentClock?gst_clock_get_time (currentClock):GST_CLOCK_TIME_NONE;

      GST_INFO_OBJECT (filter, "Inject: %s",filter->threadInfo.sample.gpsOutput.c_str());

    // release the mutex
    }
    g_object_unref(currentClock);
}


// thread that looks after the GPS details
void* gpsMonitorThreadEntry(void *arg)
{

  GstjsonInject *filter=(GstjsonInject*)arg;

  GST_INFO_OBJECT (filter, "thread started");


  nmea::NMEAParser parser;
  nmea::GPSService gps(parser);

  filter->threadInfo.gpsPolling=true;

  // requires enable_uart=1 in config.txt
  FILE* gpsFeed = fopen ("/dev/serial0", "r"); 

  GST_INFO_OBJECT (filter, "Entered parser thread");

  nlohmann::json jsonData;

  if(!gpsFeed)
  {
    jsonData["msg"]="No GPS Serial Feed";

    pushJson(filter,jsonData);

  }
  else
  {

    while(filter->threadInfo.gpsPolling)
    {

      char lineBuffer[255];
      memset(&lineBuffer,0,sizeof(lineBuffer));

      nmea::NMEAParser parser;
      nmea::GPSService gps(parser);

      // parse it

      if(gpsFeed)
      {

        while(fgets(lineBuffer,sizeof(lineBuffer)-1,gpsFeed) && filter->threadInfo.gpsPolling)
        {


            jsonData.clear();

            try
            {
              parser.readLine(lineBuffer);
            }
            catch(const std::exception& e)
            {
              GST_ERROR_OBJECT (filter, "Exception in NMEA parser '%s'",lineBuffer);
              continue;
            }

            GST_INFO_OBJECT (filter, "Parser line: '%s'",lineBuffer);

            // build the json
            jsonData["gpsLocked"]=gps.fix.locked();
            if(gps.fix.locked())
            {
              jsonData["speedKMH"]=gps.fix.speed;
              jsonData["altitudeM"]=gps.fix.altitude;
              jsonData["latitudeN"]=gps.fix.latitude;
              jsonData["longitudeE"]=gps.fix.longitude;
              jsonData["bearingDeg"]=gps.fix.travelAngle;
              jsonData["satelliteCount"]=gps.fix.trackingSatellites;
            }
            else
            {
              jsonData["msg"]="No GPS signal";
              GST_INFO_OBJECT(filter,"Lost GPS signal");
            }

            pushJson(filter,jsonData);
        }
      }
    }
  }

  GST_INFO_OBJECT (filter, "Exited parser thread");

  return NULL;

}

*/


/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_json_inject_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{

  GstjsonInject *filter;
  filter = GST_JSONINJECT (parent);
  GstFlowReturn ret;

  std::string copyOfData;
  GstClockTime pts=GST_BUFFER_PTS(buf);

/*  
    // get the mutex for shortest time
  {
    std::lock_guard<std::mutex> guard(filter->threadInfo.gpsMutex);

    copyOfData=filter->threadInfo.sample.gpsOutput;
    pts=filter->threadInfo.sample.pts==GST_CLOCK_TIME_NONE?filter->threadInfo.sample.pts:GST_BUFFER_PTS(buf);

    // releasd the mutex
  }
*/

  pts+=filter->offset;
  struct tm *info; time_t nowsecs=(pts)/GST_SECOND;
  info = gmtime(&nowsecs);

  time_t millis=(pts-(nowsecs*GST_SECOND))/GST_MSECOND;

  char timebuf[256];
  snprintf(timebuf,sizeof(timebuf)-1, "{\"data-type\":\"datetime\",\"utc\":\"%d-%02d-%02dT%02d:%02d:%02d.%03luZ\"}",
    info->tm_year+1900,
    info->tm_mon+1,
    info->tm_mday,
    info->tm_hour,
    info->tm_min,
    info->tm_sec,
    millis);
  
  copyOfData=timebuf;

  int len=copyOfData.length();

  /* make empty buffer */
  GstBuffer *textBuffer = gst_buffer_new_and_alloc(len);
  gst_buffer_fill (textBuffer, 0, copyOfData.c_str(), len);
  gst_buffer_set_size (textBuffer, len);

  // set dts, pts and dur for our video buffer
  // this locks us to the framerate of the video source
  gst_buffer_copy_into(textBuffer, buf, GST_BUFFER_COPY_TIMESTAMPS ,0,-1);

  if(GST_BUFFER_PTS(buf)<=filter->ptsSeenLast)
  {
    // https://community.nxp.com/t5/i-MX-Processors/appsrc-mp4mux-get-err-Buffer-has-no-PTS/m-p/1177648
    GST_ERROR_OBJECT (filter, "Seen a broken PTS %" GST_TIME_FORMAT ", previous %" GST_TIME_FORMAT " - this will generate a 'Buffer has no PTS.' error downstream\r", GST_TIME_ARGS(GST_BUFFER_PTS(buf)),GST_TIME_ARGS(filter->ptsSeenLast));
  }
  if(GST_BUFFER_IS_DISCONT(buf) )
  {
    GST_ERROR_OBJECT (filter, "Seen a DISCONT PTS %" GST_TIME_FORMAT ", previous %" GST_TIME_FORMAT " - this will generate a 'Buffer has no PTS.' error downstream\r", GST_TIME_ARGS(GST_BUFFER_PTS(buf)),GST_TIME_ARGS(filter->ptsSeenLast));    
  }
  unsigned long flags=(unsigned long)gst_buffer_get_flags (buf);
  if(flags)
    g_print("buffer flags 0x%lx\n",flags);

  filter->ptsSeenLast=GST_BUFFER_PTS(buf);


  //textBuffer->offset=0;
  //textBuffer->duration= GST_CLOCK_TIME_NONE;
  //textBuffer->duration=GST_SECOND/5;


  //GST_INFO_OBJECT (filter, "pushing '%s' pts %lu video pts (%lu)", copyOfData.c_str(), pts, GST_BUFFER_PTS(buf));
  // g_print("pushing pts %" GST_TIME_FORMAT " video pts - video dts %" GST_TIME_FORMAT " - duration %" GST_TIME_FORMAT " - base time %" GST_TIME_FORMAT " start time %" GST_TIME_FORMAT "\n", 
  //   GST_TIME_ARGS(GST_BUFFER_PTS(buf)), 
  //   GST_TIME_ARGS(GST_BUFFER_DTS(buf)), 
  //   GST_TIME_ARGS(GST_BUFFER_DURATION(buf)), 
  //   GST_TIME_ARGS(gst_element_get_base_time (GST_ELEMENT(parent))),
  //   GST_TIME_ARGS(gst_element_get_start_time (GST_ELEMENT(parent)))
  //   );
  // g_print("subs %s\n",timebuf);

  if(!NegotiateSubCaps(filter))
  {
    return GST_FLOW_ERROR;
  }

  GstFlowReturn subret=gst_pad_push(filter->subpad, textBuffer);

  if(subret!=GST_FLOW_OK)
  {
    GST_WARNING_OBJECT (filter, "push subbuffer returned %d", subret);
  }


  /* just push out the incoming buffer without touching it */
  ret=gst_pad_push (filter->srcpad, buf);
  GST_INFO_OBJECT (filter, "push vidbuffer returned %d", ret);

  if(ret!=GST_FLOW_OK)
  {
    return ret;
  }

  return ret;

}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
jsoninject_init (GstPlugin * jsoninject)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template jsoninject' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_json_inject_debug, "jsoninject",
      0, "Template jsoninject");

  return gst_element_register (jsoninject, "jsoninject", GST_RANK_NONE,
      GST_TYPE_JSONINJECT);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstjsoninject"
#endif

/* gstreamer looks for this structure to register jsoninjects
 *
 * exchange the string 'Template jsoninject' with your jsoninject description
 */


void jsoninject_registerRunTimePlugin()
{
  gst_plugin_register_static(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "jsoninject",
    "Template jsoninject",
    jsoninject_init,
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
    jsoninject,
    "Template jsoninject",
    jsoninject_init,
    PACKAGE_VERSION,
    GST_LICENSE,
    GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)
*/

