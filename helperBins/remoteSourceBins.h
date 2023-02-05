#include "../gstreamBin.h"
#include "myElementBins.h"
#include <gst/base/gstbasesrc.h>

class baseRemoteSourceBin : public gstreamListeningBin
{
public:
    baseRemoteSourceBin(const char*name, gstreamPipeline *parent):
        gstreamListeningBin(name,parent),
        m_q2(this,"q2"),m_progress(this,0)
    {

    }

protected:

    gstQueue2 m_q2;
    gstFrameBufferProgress m_progress;

};

// 
class rtspSourceBin : public baseRemoteSourceBin
{
public:
    rtspSourceBin(gstreamPipeline *parent, const char*location, const char*name="rtspSource"):baseRemoteSourceBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("rtspsrc","rtspsrc");
        pluginContainer<GstElement>::AddPlugin("rtph264depay","depay2");
        pluginContainer<GstElement>::AddPlugin("rtpjitterbuffer","antijitter");
        pluginContainer<GstElement>::AddPlugin("h264parse","parser2");

        // and config
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("rtspsrc"), 
            "location",location,
            NULL);

        // rtsp connects late
        ConnectLate(    pluginContainer<GstElement>::FindNamedPlugin("rtspsrc"),
                        pluginContainer<GstElement>::FindNamedPlugin("antijitter"));

        // then all the others ..
        gst_element_link_many(
                pluginContainer<GstElement>::FindNamedPlugin("antijitter"),
                pluginContainer<GstElement>::FindNamedPlugin(m_q2),
                pluginContainer<GstElement>::FindNamedPlugin("depay2"),
                pluginContainer<GstElement>::FindNamedPlugin("parser2"),
                pluginContainer<GstElement>::FindNamedPlugin(m_progress),
                NULL
            );

        // add ghost - only src, no sink
        AddGhostPads(NULL,m_progress);

    }

    ~rtspSourceBin()
    {   
        releaseRequestedPads();
    }



};

//

class rtmpSourceBin : public baseRemoteSourceBin
{
public:

    rtmpSourceBin(gstreamPipeline *parent, const char*location, const char*name="rtmpSource", unsigned toSecs=30):baseRemoteSourceBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("rtmpsrc","rtmpsrc");
        pluginContainer<GstElement>::AddPlugin("flvdemux","depay2");
        pluginContainer<GstElement>::AddPlugin("h264parse","parser2");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("rtmpsrc"), 
            "location",location,
            "timeout",toSecs,
            NULL);

        // https://lists.freedesktop.org/archives/gstreamer-devel/2018-June/068076.html
        // set it as a live source
        gst_base_src_set_live(
            GST_BASE_SRC(pluginContainer<GstElement>::FindNamedPlugin("rtmpsrc")), 
            TRUE );


        ConnectLate(    pluginContainer<GstElement>::FindNamedPlugin("depay2"),
                        pluginContainer<GstElement>::FindNamedPlugin("parser2"));

        gst_element_link_many(  pluginContainer<GstElement>::FindNamedPlugin("rtmpsrc"),
                                pluginContainer<GstElement>::FindNamedPlugin(m_q2),
                                pluginContainer<GstElement>::FindNamedPlugin("depay2"),
                                NULL);

        gst_element_link_many(  pluginContainer<GstElement>::FindNamedPlugin("parser2"),
                                pluginContainer<GstElement>::FindNamedPlugin(m_progress),
                                NULL);

        // add ghost - only src, no sink
        AddGhostPads(NULL,m_progress);

        parent->DumpGraph("rtmp");

    }

    ~rtmpSourceBin()
    {   
        releaseRequestedPads();
    }



};