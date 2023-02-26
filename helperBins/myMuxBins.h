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

        setBinFlags(GST_ELEMENT_FLAG_SINK);

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

        advertiseElementsPadTemplates_splitmuxsink(pluginContainer<GstElement>::FindNamedPlugin("splitmuxsink"));

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("splitmuxsink"), 
            "max-size-time", time_seconds * GST_SECOND, NULL);

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("splitmuxsink"), 
            "location", out, NULL);

        setBinFlags(GST_ELEMENT_FLAG_SINK);

    }

    ~gstSplitMuxOutBin()
    {
        releaseRequestedPads();
    }

    // splitmuxsink is an idiot ... it declares all its 'on request' pads as caps ANY
    // good luck trying to get a subtitles pad, you'll get video, and stiff shit
    // so this deliberately tightens the caps according to template name
    void advertiseElementsPadTemplates_splitmuxsink(GstElement *element)
    {
        if(!element)
            return;

        GstElementClass  *eclass = GST_ELEMENT_GET_CLASS (element);
        GstElementClass  *myclass = GST_ELEMENT_GET_CLASS (m_myBin);
        // get its pad templates

        GList * padlist = gst_element_class_get_pad_template_list (eclass);

        std::vector<std::pair<std::string,std::string>> fixedCaps={
            std::make_pair("video","video/any"),
            std::make_pair("subtitle_%u","text/any"),
            std::make_pair("audio_%u","audio/any"),
            std::make_pair("video_%u","video/any")
        };

        while (padlist) 
        {
            if(padlist->data)
            {
                //GstStaticPadTemplate *padtempl = (GstStaticPadTemplate*)padlist->data;
                // doc'd as GstStaticPadTemplate but apparently not (looking at gstutils.c/gst_element_get_compatible_pad_template)
                GstPadTemplate *padtempl = (GstPadTemplate*)padlist->data;

                if(padtempl->direction == GST_PAD_SINK && padtempl->presence == GST_PAD_REQUEST)
                {

                    // create a new one - dynamic
                    GstStaticPadTemplate *stat=new GstStaticPadTemplate();
                    stat->name_template=padtempl->name_template;
                    stat->direction=padtempl->direction;
                    stat->presence=padtempl->presence;

                    std::string toFind=padtempl->name_template;

                    auto found=std::find_if(fixedCaps.begin(),fixedCaps.end(),
                        [&toFind](const std::pair<std::string,std::string>& x) { return x.first == toFind;}
                        );

                    if(found!=fixedCaps.end())
                    {
                        stat->static_caps=GST_STATIC_CAPS(found->second.c_str());

                        m_advertised.push_back(std::pair<GstElement*,GstStaticPadTemplate*>(element,stat));

                        GST_INFO_OBJECT (m_myBin, "Advertising '%s' pad from '%s' - caps %s",padtempl->name_template,GST_ELEMENT_NAME(element), found->second.c_str());                

                        gst_element_class_add_pad_template(myclass, padtempl);
                    }


                }
            }
            padlist = g_list_next (padlist);
        }

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

        setBinFlags(GST_ELEMENT_FLAG_SINK);

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
