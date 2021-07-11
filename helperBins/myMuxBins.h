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

};


class gstH264MuxOutBin :  public gstreamBin
{
public:
    gstH264MuxOutBin(gstreamPipeline *parent,const char*location,const char *muxer, const char *name="muxh264OutBin"):gstreamBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("filesink");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("filesink"), 
            "location", location, NULL);

        lateCtor(muxer);

    }


    gstH264MuxOutBin(gstreamPipeline *parent,FILE *fhandle,const char *muxer, const char *name="muxh264OutBin"):gstreamBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("fdsink","filesink");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("filesink"), 
            "fd", fileno(fhandle), NULL);

        lateCtor(muxer);

    }

protected:

    void lateCtor(const char *muxer)
    {
        if(pluginContainer<GstElement>::AddPlugin("vaapih264enc","encoder"))
        {
            pluginContainer<GstElement>::AddPlugin("identity","pre-encoder");    

            GST_WARNING_OBJECT (m_parent, "Failed to create vaapih264enc, trying v4l2h264enc");
            if(pluginContainer<GstElement>::AddPlugin("v4l2h264enc","encoder"))
            {
                GST_WARNING_OBJECT (m_parent, "Failed to create v4l2h264enc, trying x264enc");
                if(pluginContainer<GstElement>::AddPlugin("x264enc","encoder"))
                {
                    GST_ERROR_OBJECT (m_parent, "Failed to create x264enc, pretty fatal");
                }
            }
        }
        else
        {
            pluginContainer<GstElement>::AddPlugin("identity","pre-encoder");    
        }


        pluginContainer<GstElement>::AddPlugin("h264parse");
        pluginContainer<GstElement>::AddPlugin(muxer,"muxer");

        if(!gst_element_link_many(  
            pluginContainer<GstElement>::FindNamedPlugin("pre-encoder"),
            pluginContainer<GstElement>::FindNamedPlugin("encoder"),
            pluginContainer<GstElement>::FindNamedPlugin("h264parse"),
            pluginContainer<GstElement>::FindNamedPlugin("muxer"),
            pluginContainer<GstElement>::FindNamedPlugin("filesink"),
            NULL
            ))
        {
            GST_ERROR_OBJECT (m_parent, "Failed to link output bin!");
        }

        AddGhostPads("pre-encoder",NULL);
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

