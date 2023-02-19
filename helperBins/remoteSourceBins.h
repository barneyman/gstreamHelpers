#include "../gstreamBin.h"
#include "myElementBins.h"
#include <gst/base/gstbasesrc.h>

class baseRemoteSourceBin : public gstreamListeningBin
{
public:
    baseRemoteSourceBin(const char*name, pluginContainer<GstElement> *parent):
        gstreamListeningBin(name,parent),
        m_q2(this,"q2"),m_progress(this,0)
    {
        setBinFlags(GST_ELEMENT_FLAG_SOURCE);
    }

protected:

    gstQueue2 m_q2;
    gstFrameBufferProgress m_progress;

};

// 
class rtspSourceBin : public baseRemoteSourceBin
{
public:
    rtspSourceBin(pluginContainer<GstElement> *parent, const char*location, const char*name="rtspSource"):baseRemoteSourceBin(name,parent)
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

        // add ghost - only src, no sinkgstreamPipeline
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

    rtmpSourceBin(pluginContainer<GstElement> *parent, const char*location, const char*name="rtmpSource", unsigned toSecs=30):baseRemoteSourceBin(name,parent)
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


    }

    ~rtmpSourceBin()
    {   
        releaseRequestedPads();
    }



};

template <class TremoteSource>
class multiRemoteSourceBin : public gstreamBin
{
public:

    multiRemoteSourceBin(pluginContainer<GstElement>*parent, std::vector<std::string> sources, const char *simple_caps):
        gstreamBin("multiRemoteSourceBin",parent)
    {
        int i=0;
        for(auto each=sources.begin();each!=sources.end();each++,i++)
        {
            char capsFilterName[20];
            snprintf(capsFilterName,sizeof(capsFilterName)-1,"capsfilt_%u",i);
            char sourceName[20];
            snprintf(sourceName,sizeof(sourceName)-1,"remote%u",i);


            TremoteSource *eachSourceBin=new TremoteSource(this,each->c_str(),sourceName);
            gstCapsFilterSimple *eachSourceCaps=new gstCapsFilterSimple(this,simple_caps,capsFilterName);
            // connect them
            gst_element_link_many(  pluginContainer<GstElement>::FindNamedPlugin(*eachSourceBin),
                                    pluginContainer<GstElement>::FindNamedPlugin(*eachSourceCaps),
                                    NULL);
            // and ghost it
            AddGhostPads(NULL,*eachSourceCaps);
            

            m_remoteSources.push_back(std::pair<TremoteSource*,gstCapsFilterSimple*>(eachSourceBin,eachSourceCaps));   
        }
        setBinFlags(GST_ELEMENT_FLAG_SOURCE);

    }

    ~multiRemoteSourceBin()
    {
        for(auto each=m_remoteSources.begin();each!=m_remoteSources.end();each++)
        {
            delete std::get<0>(*each);
            delete std::get<1>(*each);
        }
        m_remoteSources.clear();
    }

protected:

    std::vector<std::pair<TremoteSource*,gstCapsFilterSimple*>> m_remoteSources;

};
