#include "gstreamBin.h"

// 
class rtspSourceBin : public gstreamListeningBin
{
public:
    rtspSourceBin(gstreamPipeline *parent, const char*location, const char*name="rtspSource"):gstreamListeningBin(name,parent),
        m_q2(this,"q2"),m_progress(this)
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


protected:

    gstQueue2 m_q2;
    gstFrameBufferProgress m_progress;

};

//

class rtmpSourceBin : public gstreamListeningBin
{
public:

    rtmpSourceBin(gstreamPipeline *parent, const char*location, const char*name="rtmpSource", unsigned toSecs=30):gstreamListeningBin(name,parent),
        m_q2(this,"q2forrtmpsrc"), m_progress(this)
    {
        pluginContainer<GstElement>::AddPlugin("rtmpsrc","rtmpsrc");
        pluginContainer<GstElement>::AddPlugin("flvdemux","depay2");
        pluginContainer<GstElement>::AddPlugin("h264parse","parser2");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("rtmpsrc"), 
            "location",location,
            "timeout",toSecs,
            NULL);

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


protected:

    gstQueue2 m_q2;
    gstFrameBufferProgress m_progress;

};