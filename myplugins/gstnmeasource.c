/* GStreamer
 * Copyright (C) 2020 FIXME <fixme@example.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstnmeasource
 *
 * The nmeasource element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! nmeasource ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gstnmeasource.h"
#include <string>

GST_DEBUG_CATEGORY_STATIC (gst_nmeasource_debug_category);
#define GST_CAT_DEFAULT gst_nmeasource_debug_category

/* prototypes */


static void gst_nmeasource_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_nmeasource_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_nmeasource_dispose (GObject * object);
static void gst_nmeasource_finalize (GObject * object);

static GstCaps *gst_nmeasource_get_caps (GstBaseSrc * src, GstCaps * filter);
static gboolean gst_nmeasource_negotiate (GstBaseSrc * src);
static GstCaps *gst_nmeasource_fixate (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_nmeasource_set_caps (GstBaseSrc * src, GstCaps * caps);
static gboolean gst_nmeasource_decide_allocation (GstBaseSrc * src,
    GstQuery * query);
static gboolean gst_nmeasource_start (GstBaseSrc * src);
static gboolean gst_nmeasource_stop (GstBaseSrc * src);
static void gst_nmeasource_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
//static gboolean gst_nmeasource_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_nmeasource_is_seekable (GstBaseSrc * src);
static gboolean gst_nmeasource_prepare_seek_segment (GstBaseSrc * src,
    GstEvent * seek, GstSegment * segment);
static gboolean gst_nmeasource_do_seek (GstBaseSrc * src, GstSegment * segment);
static gboolean gst_nmeasource_unlock (GstBaseSrc * src);
static gboolean gst_nmeasource_unlock_stop (GstBaseSrc * src);
static gboolean gst_nmeasource_query (GstBaseSrc * src, GstQuery * query);
static gboolean gst_nmeasource_event (GstBaseSrc * src, GstEvent * event);
static GstFlowReturn gst_nmeasource_create (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** buf);
static GstFlowReturn gst_nmeasource_alloc (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer ** buf);
static GstFlowReturn gst_nmeasource_fill (GstBaseSrc * src, guint64 offset,
    guint size, GstBuffer * buf);

enum
{
  PROP_0,
  PROP_FRAME_RATE,
  PROP_PARENT,
  PROP_PIPELINE_TIME
};

#define DEFAULT_FRAMERATE 30


/* pad templates */

#define _USE_FIXED_CAPS
#define _USE_UTF8_CAPS

static GstStaticPadTemplate gst_nmeasource_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
#ifdef _USE_UTF8_CAPS    
    GST_STATIC_CAPS ("text/x-raw,format=utf8")
#else    
    GST_STATIC_CAPS ("application/x-subtitle-unknown")
#endif    
    );


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstNmeaSource, gst_nmeasource, GST_TYPE_PUSH_SRC,
  GST_DEBUG_CATEGORY_INIT (gst_nmeasource_debug_category, "nmeasource", 0,
  "debug category for nmeasource element"));

static void
gst_nmeasource_class_init (GstNmeaSourceClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
      &gst_nmeasource_src_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "FIXME Long name", "Generic", "FIXME Description",
      "FIXME <fixme@example.com>");

  gobject_class->set_property = gst_nmeasource_set_property;
  gobject_class->get_property = gst_nmeasource_get_property;
  //gobject_class->dispose = gst_nmeasource_dispose;
  //gobject_class->finalize = gst_nmeasource_finalize;
  //base_src_class->get_caps = GST_DEBUG_FUNCPTR (gst_nmeasource_get_caps);
  //base_src_class->negotiate = GST_DEBUG_FUNCPTR (gst_nmeasource_negotiate);
  //base_src_class->fixate = GST_DEBUG_FUNCPTR (gst_nmeasource_fixate);
  base_src_class->set_caps = GST_DEBUG_FUNCPTR (gst_nmeasource_set_caps);
  //base_src_class->decide_allocation = GST_DEBUG_FUNCPTR (gst_nmeasource_decide_allocation);
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_nmeasource_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_nmeasource_stop);
  base_src_class->get_times = GST_DEBUG_FUNCPTR (gst_nmeasource_get_times);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_nmeasource_is_seekable);
  //base_src_class->prepare_seek_segment = GST_DEBUG_FUNCPTR (gst_nmeasource_prepare_seek_segment);
  //base_src_class->do_seek = GST_DEBUG_FUNCPTR (gst_nmeasource_do_seek);
  //base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_nmeasource_unlock);
  //base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_nmeasource_unlock_stop);
  //base_src_class->query = GST_DEBUG_FUNCPTR (gst_nmeasource_query);
  //base_src_class->event = GST_DEBUG_FUNCPTR (gst_nmeasource_event);
  //base_src_class->create = GST_DEBUG_FUNCPTR (gst_nmeasource_create);
  //base_src_class->alloc = GST_DEBUG_FUNCPTR (gst_nmeasource_alloc);
  base_src_class->fill = GST_DEBUG_FUNCPTR (gst_nmeasource_fill);


  // set up my properties
  g_object_class_install_property (gobject_class, PROP_FRAME_RATE,
      g_param_spec_int ("frame-rate", "Frame Rate",
          "Refresh Rate for the Meta", 
          1,30, DEFAULT_FRAMERATE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));  

  g_object_class_install_property (gobject_class, PROP_PARENT,
      g_param_spec_pointer ("parent", "parent",
          "Parent gstPipeline ptr", 
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));  

  g_object_class_install_property (gobject_class, PROP_PIPELINE_TIME,
      g_param_spec_boolean ("pipelinetime", "pipelinetime",
          "Use pipelinetime time", 
          FALSE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));  

}

static void
gst_nmeasource_init (GstNmeaSource *nmeasource)
{
#ifdef _USE_FIXED_CAPS  
  gst_pad_use_fixed_caps(GST_BASE_SRC(nmeasource)->srcpad);
#endif

  nmeasource->threadInfo.usePipelineTime=true;
  nmeasource->threadInfo.firstFrame=GST_CLOCK_TIME_NONE;
  nmeasource->parent=NULL;

  nmeasource->threadInfo.frameRate=DEFAULT_FRAMERATE;
  nmeasource->threadInfo.frameTimeDelta=gst_util_uint64_scale(GST_SECOND,1,nmeasource->threadInfo.frameRate);

  // if i set this to true, i need to provide a clock to the pipeline
  // GST_MESSAGE_CLOCK_PROVIDE
  gst_base_src_set_live(GST_BASE_SRC(nmeasource), TRUE);
  gst_base_src_set_format(GST_BASE_SRC(nmeasource), GST_FORMAT_TIME );

}

void
gst_nmeasource_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (object);

  GST_DEBUG_OBJECT (nmeasource, "set_property");

  switch (property_id) {
    case PROP_FRAME_RATE:
      nmeasource->threadInfo.frameRate= g_value_get_int (value);
      nmeasource->threadInfo.frameTimeDelta=gst_util_uint64_scale(GST_SECOND,1,nmeasource->threadInfo.frameRate);
      break;
    case PROP_PARENT:
      nmeasource->parent=(gstreamPipeline*)g_value_get_pointer (value);
      break;
    case PROP_PIPELINE_TIME:
      nmeasource->threadInfo.usePipelineTime=g_value_get_boolean (value)?true:false;
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_nmeasource_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (object);

  GST_DEBUG_OBJECT (nmeasource, "get_property");

  switch (property_id) {

    case PROP_FRAME_RATE:
      g_value_set_int (value, nmeasource->threadInfo.frameRate);
      break;
    case PROP_PARENT:
      g_value_set_pointer(value, nmeasource->parent);
      break;
    case PROP_PIPELINE_TIME:
      g_value_set_boolean(value, nmeasource->threadInfo.usePipelineTime);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_nmeasource_dispose (GObject * object)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (object);

  GST_DEBUG_OBJECT (nmeasource, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_nmeasource_parent_class)->dispose (object);
}

void
gst_nmeasource_finalize (GObject * object)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (object);

  GST_DEBUG_OBJECT (nmeasource, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_nmeasource_parent_class)->finalize (object);
}

/* get caps from subclass */
static GstCaps *
gst_nmeasource_get_caps (GstBaseSrc * src, GstCaps * filter)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "get_caps");

  return NULL;
}

/* decide on caps */
static gboolean
gst_nmeasource_negotiate (GstBaseSrc * src)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "negotiate");

  return TRUE;
}

/* called if, in negotiation, caps need fixating */
static GstCaps *
gst_nmeasource_fixate (GstBaseSrc * src, GstCaps * caps)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "fixate");

  return NULL;
}

/* notify the subclass of new caps */
static gboolean
gst_nmeasource_set_caps (GstBaseSrc * src, GstCaps * caps)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource,"set caps '%s'", gst_caps_to_string(caps) );

  return TRUE;
}

/* setup allocation query */
static gboolean
gst_nmeasource_decide_allocation (GstBaseSrc * src, GstQuery * query)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "decide_allocation");

  return TRUE;
}


/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_nmeasource_start (GstBaseSrc * src)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "start");

  startMonitorThread(nmeasource->threadInfo);

  nmeasource->threadInfo.sample.pts=0;
  nmeasource->threadInfo.framesFilled=0;
  nmeasource->threadInfo.runningTime=0;
  nmeasource->waitId=NULL;



  return TRUE;
}

static gboolean
gst_nmeasource_stop (GstBaseSrc * src)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "stop");

  stopMonitorThread(nmeasource->threadInfo);

  // and yeild the waitId
  if(nmeasource->waitId)
  {
    gst_clock_id_unref(nmeasource->waitId);
    nmeasource->waitId=NULL;
  }

  return TRUE;
}

/* given a buffer, return start and stop time when it should be pushed
 * out. The base class will sync on the clock using these times. */
static void
gst_nmeasource_get_times (GstBaseSrc * src, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  // stolen from gsttestvieosrc
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (src)) {
    /* get duration to calculate end time */
    GstClockTime duration = GST_BUFFER_DURATION (buffer);
    GstClockTime pts = GST_BUFFER_PTS (buffer);
    GST_DEBUG_OBJECT (nmeasource, "PTS %" GST_TIME_FORMAT ".",GST_TIME_ARGS(pts));

    // give me some latency
    GstClockTime timestamp = pts+(20*GST_MSECOND);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }

  GST_DEBUG_OBJECT (nmeasource, "start  %" GST_TIME_FORMAT ".",GST_TIME_ARGS(*start));

}



/* check if the resource is seekable */
static gboolean
gst_nmeasource_is_seekable (GstBaseSrc * src)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "is_seekable");

  return FALSE;
}

/* Prepare the segment on which to perform do_seek(), converting to the
 * current basesrc format. */
static gboolean
gst_nmeasource_prepare_seek_segment (GstBaseSrc * src, GstEvent * seek,
    GstSegment * segment)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "prepare_seek_segment");

  return TRUE;
}

/* notify subclasses of a seek */
static gboolean
gst_nmeasource_do_seek (GstBaseSrc * src, GstSegment * segment)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "do_seek");

  return TRUE;
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
static gboolean
gst_nmeasource_unlock (GstBaseSrc * src)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "unlock");

  return TRUE;
}

/* Clear any pending unlock request, as we succeeded in unlocking */
static gboolean
gst_nmeasource_unlock_stop (GstBaseSrc * src)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "unlock_stop");

  return TRUE;
}

/* notify subclasses of a query */
static gboolean
gst_nmeasource_query (GstBaseSrc * src, GstQuery * query)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

   GST_DEBUG_OBJECT (nmeasource, "Received %s query: %" GST_PTR_FORMAT,
      GST_QUERY_TYPE_NAME (query), query);

  gboolean ret=TRUE;

  switch (GST_QUERY_TYPE (query)) {

      case GST_QUERY_CAPS:


      default:
        ret=FALSE;
        break;
    
  }


  GST_DEBUG_OBJECT (nmeasource, "query");

  return ret;
}

/* notify subclasses of an event */
static gboolean
gst_nmeasource_event (GstBaseSrc * src, GstEvent * event)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "event");

  return TRUE;
}

/* ask the subclass to create a buffer with offset and size, the default
 * implementation will call alloc and fill. */
static GstFlowReturn
gst_nmeasource_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "create");

  return GST_FLOW_OK;
}

/* ask the subclass to allocate an output buffer. The default implementation
 * will use the negotiated allocator. */
static GstFlowReturn
gst_nmeasource_alloc (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "alloc");

  return GST_FLOW_OK;
}

/* ask the subclass to fill the buffer with data from offset and size */
static GstFlowReturn
gst_nmeasource_fill (GstBaseSrc * src, guint64 offset, guint size, GstBuffer * buf)
{
  GstNmeaSource *nmeasource = GST_NMEASOURCE (src);

  GST_DEBUG_OBJECT (nmeasource, "fill offset %lu size %u", offset, size);
 
  GstClockTime baseTime=gst_element_get_base_time(GST_ELEMENT(src));

  GstClock *myClock=GST_ELEMENT_CLOCK (src);

  GstClockTime runningTime=gst_clock_get_time(myClock)-baseTime;

  // while(runningTime < GST_SECOND*2)
  // {
  //   runningTime=gst_clock_get_time(myClock)-baseTime;    
  // }


  offset=0;

  std::string copyOfData, timeString;

  GST_DEBUG_OBJECT (nmeasource, "At frame rate %d timeDelta is %lu", nmeasource->threadInfo.frameRate, nmeasource->threadInfo.frameTimeDelta);

  time_t utcmillis=0;

  if(nmeasource->threadInfo.usePipelineTime)
  {
    if(nmeasource->parent)
    {
      GstClockTime pts=nmeasource->parent->FixTimeForEpoch((nmeasource->threadInfo.runningTime+baseTime));

      struct tm *info; 
      time_t nowsecs=pts/GST_SECOND;
      info = gmtime(&nowsecs);

      utcmillis=pts/GST_MSECOND;

      char timebuf[128];
      snprintf(timebuf,sizeof(timebuf)-1, "%d-%02d-%02dT%02d:%02d:%02d.%luZ",
        info->tm_year+1900,
        info->tm_mon+1,
        info->tm_mday,
        info->tm_hour,
        info->tm_min,
        info->tm_sec,
        //(pts-(nowsecs*GST_SECOND))/GST_MSECOND
        ((pts%GST_SECOND)/GST_MSECOND)/100
        );

      GST_DEBUG_OBJECT (nmeasource, "UTC %s", timebuf);

      timeString=timebuf;
    }
    else
    {
      copyOfData="Need parent property set to pipeline* for pipeline time";
    }
  }
  

  {
    // get the mutex for shortest time
    {
      std::lock_guard<std::mutex> guard(nmeasource->threadInfo.gpsMutex);
      
      if(timeString.length())
      {
        nmeasource->threadInfo.sample.gpsOutput["utc"]=timeString;
        nmeasource->threadInfo.sample.gpsOutput["utc-millis"]=utcmillis;
      }
      copyOfData=nmeasource->threadInfo.sample.gpsOutput.dump();
    }
  }

  int len=copyOfData.length();

  GST_DEBUG_OBJECT (nmeasource, "Buffer %d, needed %d (offset %lu) %s", size, len, offset, copyOfData.c_str());

  if((offset+len)>=size)
  {
    GST_ERROR_OBJECT (nmeasource, "Buffer too small %d, needed %d (offset %lu)", size, len, offset);
    return GST_FLOW_ERROR;
  }    

  gst_buffer_fill (buf, offset, copyOfData.c_str(), len);
  gst_buffer_set_size (buf, len);


  if(!nmeasource->threadInfo.runningTime)
  {
    nmeasource->threadInfo.runningTime=(gst_clock_get_time(myClock)-baseTime);

  }
  else
  {
    nmeasource->threadInfo.runningTime+=nmeasource->threadInfo.frameTimeDelta;
  }

  GST_INFO_OBJECT (nmeasource, "Fill: Running time = %" GST_TIME_FORMAT ,GST_TIME_ARGS(runningTime));

  {
    // sort out timestamps - stolen from gstvideotestsrc
    GST_BUFFER_PTS (buf) = nmeasource->threadInfo.runningTime;
    GST_BUFFER_DTS (buf) = GST_CLOCK_TIME_NONE;

  }

  GST_BUFFER_OFFSET (buf) = nmeasource->threadInfo.framesFilled++;
  GST_BUFFER_OFFSET_END (buf) =nmeasource->threadInfo.framesFilled;

  GST_DEBUG_OBJECT (nmeasource, "Buffer PTS %" GST_TIME_FORMAT ".",GST_TIME_ARGS(GST_BUFFER_PTS (buf)));

  gst_object_sync_values (GST_OBJECT (src), GST_BUFFER_PTS (buf));

  GST_BUFFER_DURATION (buf) = nmeasource->threadInfo.frameTimeDelta; 

  GST_DEBUG_OBJECT (nmeasource, "Buffer Dur %" GST_TIME_FORMAT ".",GST_TIME_ARGS(GST_BUFFER_DURATION (buf)));

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

  /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register (plugin, "nmeasource", GST_RANK_NONE,
      GST_TYPE_NMEASOURCE);
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

void gst_nmeasource_registerRunTimePlugin()
{
  gst_plugin_register_static(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "nmeasource",
    "Template nmeasource",
    plugin_init,
    "0.1",
    "Proprietary",
    "bjf",
    "source_bjf",
    PACKAGE );
}


// GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
//     GST_VERSION_MINOR,
//     nmeasource,
//     "FIXME plugin description",
//     plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

