#ifndef _mymuxbins_guard
#define _mymuxbins_guard

#include "myElementBins.h"


class gstMuxOutBin :  public gstreamBin
{

protected:

    gstFrameBufferProgress m_progress;

public:
    gstMuxOutBin(gstreamPipeline *parent,const char*location,const char *muxer="mp4mux", const char *name="muxOutBin"):
        gstreamBin(name,parent),
        m_progress(this)
    {
        pluginContainer<GstElement>::AddPlugin("filesink","finalsink");
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("finalsink"), 
            "location", location, NULL);


        lateCtor(muxer);

    }


    gstMuxOutBin(gstreamPipeline *parent,FILE *fhandle,const char *muxer="mp4mux", const char *name="muxOutBin"):
        gstreamBin(name,parent),
        m_progress(this)
    {
        pluginContainer<GstElement>::AddPlugin("fdsink","finalsink");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("finalsink"), 
            "fd", fileno(fhandle), NULL);

        lateCtor(muxer);

    }


    void lateCtor(const char *muxer="mp4mux")
    {
        pluginContainer<GstElement>::AddPlugin(muxer,"muxer");


        gst_element_link_many( pluginContainer<GstElement>::FindNamedPlugin("muxer"),
            pluginContainer<GstElement>::FindNamedPlugin(m_progress),
            pluginContainer<GstElement>::FindNamedPlugin("finalsink"),
            NULL
            );

        AddGhostPads("muxer",NULL);

        advertiseElementsPadTemplates("muxer");

    }

    ~gstMuxOutBin()
    {
        releaseRequestedPads();
    }

};

class gstSplitMuxOutBin : public gstreamBin
{
public:
    gstSplitMuxOutBin(gstreamPipeline *parent, unsigned time_seconds, const char*out="out_%05d.mp4"):gstreamBin("splitMuxOutBin",parent)
    {

        pluginContainer<GstElement>::AddPlugin("splitmuxsink");

        AddGhostPads("splitmuxsink",NULL);

        advertiseElementsPadTemplates("splitmuxsink");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("splitmuxsink"), 
            "max-size-time", time_seconds * GST_SECOND, NULL);

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("splitmuxsink"), 
            "location", out, NULL);


    }

    ~gstSplitMuxOutBin()
    {
        releaseRequestedPads();
    }

};



class gstH264MuxOutBin :  public gstreamBin
{
protected:

    gstH264encoderBin m_encoder;
    gstFrameBufferProgress m_progress;

public:
    gstH264MuxOutBin(gstreamPipeline *parent,const char*location,const char *muxer, const char *name="muxh264OutBin"):
        gstreamBin(name,parent),
        m_progress(this),
        m_encoder(this)
    {
        pluginContainer<GstElement>::AddPlugin("filesink");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("filesink"), 
            "location", location, NULL);

        lateCtor(muxer);

    }


    gstH264MuxOutBin(gstreamPipeline *parent,FILE *fhandle,const char *muxer, const char *name="muxh264OutBin"):
        gstreamBin(name,parent),
        m_progress(this),
        m_encoder(this)
    {
        pluginContainer<GstElement>::AddPlugin("fdsink","filesink");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("filesink"), 
            "fd", fileno(fhandle), NULL);

        lateCtor(muxer);

    }

protected:

    void lateCtor(const char *muxer)
    {

        pluginContainer<GstElement>::AddPlugin(muxer,"muxer");

        if(!gst_element_link_many(  
            pluginContainer<GstElement>::FindNamedPlugin(m_encoder),
            pluginContainer<GstElement>::FindNamedPlugin(m_progress),
            pluginContainer<GstElement>::FindNamedPlugin("muxer"),
            pluginContainer<GstElement>::FindNamedPlugin("filesink"),
            NULL
            ))
        {
            GST_ERROR_OBJECT (m_parent, "Failed to link output bin!");
        }

        AddGhostPads(m_encoder,NULL);
    }

};

class gstMatroskaOutBin : public gstH264MuxOutBin
{
public:
    gstMatroskaOutBin(gstreamPipeline *parent,const char*location,const char *name="mkvOutBin"):
        gstH264MuxOutBin(parent,location,"matroskamux",name)
    {

    }

};


class gstMP4OutBin : public gstH264MuxOutBin
{
public:
    gstMP4OutBin(gstreamPipeline *parent,const char*location,const char *name="mp4OutBin"):
        gstH264MuxOutBin(parent,location,"mp4mux",name)
    {

    }

    gstMP4OutBin(gstreamPipeline *parent,FILE*location,const char *name="mp4OutBin"):
        gstH264MuxOutBin(parent,location,"mp4mux",name)
    {
    }

};

#endif // _mymuxbins_guard
