#ifndef _mymuxbins_guard
#define _mymuxbins_guard

#include "myElementBins.h"
#include "muxInfo.h"


class gstMuxOutBin :  public gstreamBin
{

protected:

    gstFrameBufferProgress *m_progress;
    std::string m_muxerName;

    // for gstSplitMuxOutBin, ostensibly
    gstMuxOutBin(const char *name, gstreamPipeline *parent, const char *muxer):
        gstreamBin(name,parent),
        m_muxerName(muxer),
        m_progress(NULL)
    {
    }

public:
    gstMuxOutBin(gstreamPipeline *parent,const char*location, const char *muxer="mp4mux", const char *name="muxOutBin"):
        gstreamBin(name,parent),
        m_muxerName(muxer)
    {
        m_progress=new gstFrameBufferProgress(this);
        pluginContainer<GstElement>::AddPlugin("filesink","finalsink");
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("finalsink"), 
            "location", location, 
            // this make SEEK work ...
            "async", FALSE,
            NULL);

        lateCtor();

    }


    gstMuxOutBin(gstreamPipeline *parent,FILE *fhandle,const char *muxer="mp4mux", const char *name="muxOutBin"):
        gstreamBin(name,parent),
        m_muxerName(muxer)
    {
        m_progress=new gstFrameBufferProgress(this);

        pluginContainer<GstElement>::AddPlugin("fdsink","finalsink");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("finalsink"), 
            "fd", fileno(fhandle), 
            // this make SEEK work ...
            "async", FALSE,
            NULL);

        lateCtor();

    }


    void lateCtor()
    {
        pluginContainer<GstElement>::AddPlugin(m_muxerName.c_str());

        // then look at the muxinfo and request those pads

        gst_element_link_many( pluginContainer<GstElement>::FindNamedPlugin(m_muxerName.c_str()),
            pluginContainer<GstElement>::FindNamedPlugin(*m_progress),
            pluginContainer<GstElement>::FindNamedPlugin("finalsink"),
            NULL
            );

        // no point ghosting the muxer, it has no sink pins until requested

        setBinFlags(GST_ELEMENT_FLAG_SINK);
    }

    ~gstMuxOutBin()
    {
        releaseRequestedPads();
        delete m_progress;
    }

    virtual GstPad *request_new_pad (GstElement * element,GstPadTemplate * templ,const gchar * name,const GstCaps * caps)
    {
        GstElementClass  *eclass = GST_ELEMENT_GET_CLASS (pluginContainer<GstElement>::FindNamedPlugin(m_muxerName.c_str()));

        GList * padlist = gst_element_class_get_pad_template_list (eclass);
        while (padlist) 
        {
            if(padlist->data)
            {
                GstPadTemplate *padtempl = (GstPadTemplate*)padlist->data;

                if(padtempl->direction == GST_PAD_SINK && padtempl->presence == GST_PAD_REQUEST)
                {
                    // ostensibly for splitmuxsink
                    GstCaps* tightenedCaps=tightenCaps(padtempl);

                    bool capsInterset=doCapsIntersect(caps,tightenedCaps?tightenedCaps:padtempl->caps);

                    if(tightenedCaps)
                    {
                        gst_caps_unref(tightenedCaps);
                    }

                    if(capsInterset)
                    {
                        bool proceed=true;

                        // if it's not a %u template check it's not already linked!
                        if(!strchr(padtempl->name_template,'%'))
                        {
                            for(GList *elementPads=pluginContainer<GstElement>::FindNamedPlugin(m_muxerName.c_str())->sinkpads;
                                        elementPads;
                                        elementPads=elementPads->next)
                            {
                                GstPad *eachPad=(GstPad *)elementPads->data;  
                                if(!strcmp(eachPad->object.name,padtempl->name_template))
                                {
                                    if(gst_pad_is_linked(eachPad))
                                    {
                                        /// oh dear ... already linked
                                        proceed=false;
                                        break;
                                    }   
                                }
                            }
                        }

                        GstPad *newPad=NULL;
                        if(proceed)
                        {
                            newPad=gst_element_request_pad(pluginContainer<GstElement>::FindNamedPlugin(m_muxerName.c_str()),
                                                    padtempl,
                                                    padtempl->name_template,
                                                    padtempl->caps);
                        }

                        if(!newPad)                                                        
                        {
                            // requesting video when it's already taken ... fall thru to video_aux
                            padlist = g_list_next (padlist);
                            continue;
                        }

                        // remember to release it
                        addPadToBeReleased(pluginContainer<GstElement>::FindNamedPlugin(m_muxerName.c_str()),newPad);

                        return GhostSingleRequestPad(newPad);

                    } 
                    else
                    {
                        GstPad *handled=handleUnfurnishedRequest(caps);
                        if(handled)
                        {
                            // assume they ghosted it
                            return handled;
                        }
                    }
                }
            }

            padlist = g_list_next (padlist);
        }
        return NULL;
    }    

    virtual GstPad* handleUnfurnishedRequest(const GstCaps *)
    {
        return NULL;
    }

    virtual GstCaps* tightenCaps(GstPadTemplate *)
    {
        return NULL;
    }

};

// TODO this should inherit from gstMuxOutBin
class gstSplitMuxOutBin : public gstMuxOutBin
{

protected:

    std::vector<std::pair<std::string,std::string>> m_fixedCaps;

public:
    gstSplitMuxOutBin(gstreamPipeline *parent, unsigned time_seconds, const char*out="out_%05d.mp4"):
        gstMuxOutBin("splitMuxOutBin",parent,"splitmuxsink")
    {

        pluginContainer<GstElement>::AddPlugin("splitmuxsink");

        m_fixedCaps={
            std::make_pair("video","video/x-h264"),
            std::make_pair("subtitle_%u","text/x-raw"),
            std::make_pair("audio_%u","audio/ANY"),
            std::make_pair("video_aux_%u","video/x-h264")
        };

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("splitmuxsink"), 
            "max-size-time", time_seconds * GST_SECOND, NULL);

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("splitmuxsink"), 
            "location", out, NULL);

        setBinFlags(GST_ELEMENT_FLAG_SINK);
    }

    ~gstSplitMuxOutBin()
    {
        GST_DEBUG_OBJECT (m_myBin, "gstSplitMuxOutBin dtor");                
    }

    virtual GstCaps* tightenCaps(GstPadTemplate *padtempl)
    {
        std::string toFind=padtempl->name_template;
        auto found=std::find_if(m_fixedCaps.begin(),m_fixedCaps.end(),
            [&toFind](const std::pair<std::string,std::string>& x) { return x.first == toFind;}
            );

        // if we found tightened caps, use them
        if(found!=m_fixedCaps.end())
        {
            return gst_caps_from_string(found->second.c_str());
        }


        return NULL;
    }


};


// TODO this should inherit from gstMuxOutBin
class gstH264EncoderMuxOutBin :  public gstMuxOutBin
{
protected:

    std::vector<gstH264encoderBin*> m_encoders;

public:
    gstH264EncoderMuxOutBin(gstreamPipeline *parent,const char*location,const char *muxer, const char *name="muxh264OutBin"):
        gstMuxOutBin(parent,location,muxer,name)
    {
    }


    gstH264EncoderMuxOutBin(gstreamPipeline *parent,FILE *fhandle,const char *muxer, const char *name="muxh264OutBin"):
        gstMuxOutBin(parent,fhandle,muxer,name)
    {
    }

    virtual ~gstH264EncoderMuxOutBin()
    {
        for(auto iter=m_encoders.begin();iter!=m_encoders.end();iter++)
        {
            delete *iter;
        }
    }

protected:

    gstH264encoderBin* requestEncoder()
    {
        char namebuf[32];
        snprintf(namebuf,sizeof(namebuf)-1,"encoder_%d",(int)m_encoders.size());

        gstH264encoderBin *encoder=new gstH264encoderBin(this,namebuf);
        m_encoders.push_back(encoder);

        return encoder;
    }


    virtual GstPad* handleUnfurnishedRequest(const GstCaps *caps)
    {
        if(doCapsIntersect(caps,"video/x-raw"))
        {
            auto encoder=requestEncoder();

            gst_element_link(pluginContainer<GstElement>::FindNamedPlugin(*encoder),
                                pluginContainer<GstElement>::FindNamedPlugin(m_muxerName.c_str()));

            return GhostSingleSinkPad(*encoder);

        }
        return NULL;
    }






};

class gstMatroskaOutBin : public gstH264EncoderMuxOutBin
{
public:
    gstMatroskaOutBin(gstreamPipeline *parent,const char*location,const char *name="mkvOutBin"):
        gstH264EncoderMuxOutBin(parent,location,"matroskamux",name)
    {

    }

};


class gstMP4OutBin : public gstH264EncoderMuxOutBin
{
public:
    gstMP4OutBin(gstreamPipeline *parent,const char*location,const char *name="mp4OutBin"):
        gstH264EncoderMuxOutBin(parent,location,"mp4mux",name)
    {

    }

    gstMP4OutBin(gstreamPipeline *parent,FILE*location,const char *name="mp4OutBin"):
        gstH264EncoderMuxOutBin(parent,location,"mp4mux",name)
    {
    }

};

#endif // _mymuxbins_guard
