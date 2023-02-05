/*

bins that wrap elements, so complex property behaviour is easier to reuse

*/
#ifndef _myelementbins_guard
#define _myelementbins_guard

#include "../gstreamBin.h"




// identity can do some cool debug stuff 
// TODO impl
class gstIdentityBin : public gstreamBin
{

public:

    gstIdentityBin(pluginContainer<GstElement> *parent):gstreamBin("testidentity",parent)
    {
        pluginContainer<GstElement>::AddPlugin("identity");
        AddGhostPads("identity","identity");
    }

};

class gstSleepBin : public gstIdentityBin
{
public:

    gstSleepBin(pluginContainer<GstElement> *parent, unsigned millis):gstIdentityBin(parent)
    {
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("identity"), 
            // MICRO, not milli
            "sleep-time", millis*1000, 
            NULL);
    }

};

class gstTimeOffsetBin : public gstIdentityBin
{
public:

    gstTimeOffsetBin(pluginContainer<GstElement> *parent, int millis):gstIdentityBin(parent)
    {
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("identity"), 
            // NANO, not milli
            "ts-offset", millis*GST_MSECOND, 
            NULL);
    }

};




// adds a progress report
class gstFrameBufferProgress : public gstreamBin
{
public:
    gstFrameBufferProgress(pluginContainer<GstElement> *parent,unsigned frequencyS=15, const char *name="progress"):gstreamBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("progressreport");

        if(frequencyS)
        {
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("progressreport"), 
                "update-freq", frequencyS, 
                NULL);
        }
        else
        {
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("progressreport"), 
                "silent", TRUE,
                NULL);
            
        }

        AddGhostPads("progressreport","progressreport");

    }

};

class gstQueue : public gstreamBin
{
public:
    gstQueue(pluginContainer<GstElement> *parent,const char *name):gstreamBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("queue");
        AddGhostPads("queue","queue");
    }

};

// queue min-threshold before pushing thru the src
class gstQueueMinimum : public gstQueue
{
public:
    gstQueueMinimum(pluginContainer<GstElement> *parent,const char *name, unsigned minMSecs, unsigned maxMSecs=0):
        gstQueue(parent,name),
        m_blockSrc(true)
    {
        if(minMSecs)
        {
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("queue"), 
                "max-size-time", maxMSecs*GST_MSECOND, 
                "max-size-buffers", 0,
                "max-size-bytes", 0,
                "min-threshold-time", GST_MSECOND*minMSecs,
                NULL);
        }       
        // attach to the src (to block) and the overrun (to release)
        g_signal_connect(pluginContainer<GstElement>::FindNamedPlugin("queue"),"running", G_CALLBACK(staticBufferRunning), this);

        
        // GList *sourcePads=pluginContainer<GstElement>::FindNamedPlugin("queue")->srcpads;
        // if(sourcePads)
        // {
        //     GstPad *eachSourcePad=(GstPad *)sourcePads->data;
        //     //gst_pad_add_probe (eachSourcePad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, (GstPadProbeCallback) staticPadBlocked, this, NULL);
        // }

    }

    static void staticBufferRunning(GstElement * queue, gpointer data)
    {
        // release the lock
        gstQueueMinimum* me=(gstQueueMinimum*)data;
        me->m_blockSrc=false;
    }

    // static GstPadProbeReturn staticPadBlocked (GstPad *pad,GstPadProbeInfo *info, gpointer data)
    // {
    //     gstQueueMinimum* me=(gstQueueMinimum*)data;
    //     if(!me->m_blockSrc)
    //     {
    //         return GST_PAD_PROBE_REMOVE;
    //     }

    //     return GST_PAD_PROBE_OK;
    // }

    volatile bool m_blockSrc;

};


// non buffering queues are used to stop race conditions on pins
// queues introduce a thread into the connection
// if you have a src with two pads, and one of thos epads gets blocked downstream
// the other pad it owns will also be blocked ...
// throw a queue in there (between each pad and its peer) and you will avoid that
class gstQueue2 : public gstreamBin
{
public:
    gstQueue2(pluginContainer<GstElement> *parent,const char *name):gstreamBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("queue2",name);
        AddGhostPads(name,name);
    }

};


// buffering queues are used to control pipeline running state
// if queue > high watermark play the pipeline
// if queue < low watermark pause the pipeline
class gstQueue2Buffering : public gstQueue2
{
public:
    gstQueue2Buffering(gstreamPipeline *parent,const char *name, unsigned maxBuffSecs):gstQueue2(parent,name)
    {
        // if they WANT buffering ...
        if(maxBuffSecs)
        {
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin(name), 
                "use-buffering", TRUE, 
                "max-size-buffers", 0,
                "max-size-bytes", 0,
                "max-size-time", GST_SECOND*maxBuffSecs,
                NULL);
        }       

    }

};


class gstQueue2RingBuffering : public gstQueue2
{
public:
    gstQueue2RingBuffering(gstreamPipeline *parent,const char *name, unsigned maxBuffMbs=50):gstQueue2(parent,name)
    {
        // if they WANT buffering ...
        if(maxBuffMbs)
        {
            guint64 ringMax=(maxBuffMbs<<20);   
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin(name), 
                "use-buffering", TRUE, 
                //"max-size-buffers", 0,
                 "max-size-bytes", ringMax,
                // "max-size-time", 0,
                "ring-buffer-max-size", ringMax,
                NULL);
        }       

    }

};

#define _DATA_PTS_PROBE
#define _EVENT_PTS_PROBE


// muxers need threads on their sinks, but they connect late
// so this gets in the way and adds a queue
class gstMultiQueueBin : public gstreamBin
{

public:

    gstMultiQueueBin(pluginContainer<GstElement> *parent,unsigned numSinks, bool buffering=false, int numberOfSeconds=2, bool syncOnRunning=true, const char *name="multiQBin"):
        gstreamBin(name,parent),
        m_owningPipeline(NULL)
    {

        // add a multiqueueMixer at the front
        pluginContainer<GstElement>::AddPlugin("multiqueue","multiqueueDemux");

        if(buffering)        
        {
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("multiqueueDemux"), 
                "use_buffering", TRUE, NULL);
        }

        if(syncOnRunning)
        {
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("multiqueueDemux"), 
                "sync-by-running-time", TRUE, NULL);
        }

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("multiqueueDemux"), 
            "max-size-buffers", 0,
            "max-size-bytes", 0,//numberOfSeconds*4000000,
            "max-size-time", numberOfSeconds*GST_SECOND,
            "high-watermark", 0.75,
            NULL);


        for(auto each=0;each<numSinks;each++)
        {
            GstCaps *tmp=gst_caps_new_any();
            GstPadTemplate *sinkTemplate=gst_pad_template_new("sink_%d",GST_PAD_SINK,GST_PAD_REQUEST,tmp);
            GstPad *newPad=gst_element_request_pad(
                pluginContainer<GstElement>::FindNamedPlugin("multiqueueDemux"),
                sinkTemplate,
                NULL,NULL);
            if(newPad)
            {
                addPadToBeReleased(pluginContainer<GstElement>::FindNamedPlugin("multiqueueDemux"),newPad);
            }
            gst_object_unref(sinkTemplate);
            gst_caps_unref(tmp);
        }

        AddGhostPads("multiqueueDemux","multiqueueDemux");



#ifdef _DATA_PTS_PROBE

        for(auto each=m_ghostPadsSinks.begin();each!=m_ghostPadsSinks.end();each++)
        {
            long probe=gst_pad_add_probe(GST_PAD(*each),GST_PAD_PROBE_TYPE_BUFFER,staticPTSprobeSINK,this,NULL);
            if(probe)
            {
                m_padPTSprobes.push_back(std::pair<GstPad*,long>(GST_PAD(*each),probe));
            }
        }

        // for(auto each=m_ghostPadsSrcs.begin();each!=m_ghostPadsSrcs.end();each++)
        // {
        //     long probe=gst_pad_add_probe(GST_PAD(*each),GST_PAD_PROBE_TYPE_BUFFER,staticPTSprobeSRC,this,NULL);
        //     if(probe)
        //     {
        //         m_padPTSprobes.push_back(std::pair<GstPad*,long>(GST_PAD(*each),probe));
        //     }
        // }

#endif

#ifdef _EVENT_PTS_PROBE

        for(auto each=m_ghostPadsSrcs.begin();each!=m_ghostPadsSrcs.end();each++)
        {
            long probe=gst_pad_add_probe(GST_PAD(*each),GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,staticPTSprobeEventSRC,this,NULL);
            if(probe)
            {
                m_padPTSprobes.push_back(std::pair<GstPad*,long>(GST_PAD(*each),probe));
            }
        }


#endif




    }

    ~gstMultiQueueBin()
    {
#if defined( _DATA_PTS_PROBE ) || defined(_EVENT_PTS_PROBE)

        for(auto each=m_padPTSprobes.begin();each!=m_padPTSprobes.end();each++)
        {
            gst_pad_remove_probe(each->first,each->second);
        }


#endif

    }





    void setPipeline(gstreamPipeline *dad)
    {
        m_owningPipeline=dad;
    }

    gstreamPipeline *m_owningPipeline;

protected:


#ifdef _DATA_PTS_PROBE

    std::vector<std::pair<GstPad*,long>> m_padPTSprobes;


    static GstPadProbeReturn staticPTSprobeSINK (GstPad * pad,GstPadProbeInfo * info,gpointer user_data)
    {
        gstMultiQueueBin*ptr=(gstMultiQueueBin*)user_data;
        return ptr->PTSprobe(pad,info, "sink");
    }

    static GstPadProbeReturn staticPTSprobeSRC (GstPad * pad,GstPadProbeInfo * info,gpointer user_data)
    {
        gstMultiQueueBin*ptr=(gstMultiQueueBin*)user_data;
        return ptr->PTSprobe(pad,info, "src");
    }

    static void printtag(const GstTagList * list,const gchar * tag, gpointer user_data)
    {
        gstMultiQueueBin*ptr=(gstMultiQueueBin*)user_data;
        GST_DEBUG_OBJECT (ptr, "tag '%s'", tag);
    }





    GstClockTime m_deltaTime, m_sourceStreamTime;


    GstPadProbeReturn PTSprobe (GstPad * pad,GstPadProbeInfo * info, const char*direction)
    {
        GstBuffer *bf=gst_pad_probe_info_get_buffer (info);

        // on the first frame ....
        if(m_owningPipeline && !bf->offset)
        {
            //m_owningPipeline->setLatency((500*GST_MSECOND));
            m_sourceStreamTime=bf->pts;

            m_deltaTime=m_owningPipeline->GetTimeSinceEpoch()-bf->pts;

            GST_WARNING_OBJECT (this, "time offset START - rtc to pts is %" GST_TIME_FORMAT "", GST_TIME_ARGS(m_deltaTime));

        }

        if(m_owningPipeline && !bf->offset || !(bf->offset_end%300))
        {
            
            GstClockTime nowDelta=m_owningPipeline->GetTimeSinceEpoch()-bf->pts;
            GstClockTime drift=nowDelta>m_deltaTime?(nowDelta-m_deltaTime):(m_deltaTime-nowDelta);

            GST_DEBUG_OBJECT (this, "pad ptsprobe '%s:%s'@ %" GST_TIME_FORMAT " PTS= %" GST_TIME_FORMAT 
                " stream= %" GST_TIME_FORMAT  
                " off %lu end %lu dur= %" GST_TIME_FORMAT " drift= %" GST_TIME_FORMAT "",
                direction, GST_ELEMENT_NAME(pad), 
                GST_TIME_ARGS(GetRunningTime()),
                GST_TIME_ARGS(bf->pts), 
                GST_TIME_ARGS(bf->dts-m_sourceStreamTime), 
                bf->offset, bf->offset_end,
                GST_TIME_ARGS(bf->duration),
                GST_TIME_ARGS(drift) );

        }


        return GST_PAD_PROBE_OK ;
    }


#endif


#ifdef _EVENT_PTS_PROBE


    static GstPadProbeReturn staticPTSprobeEventSRC(GstPad * pad,GstPadProbeInfo * info,gpointer user_data)
    {
        gstMultiQueueBin*ptr=(gstMultiQueueBin*)user_data;

        GstEvent *ev=gst_pad_probe_info_get_event(info);

        switch(GST_EVENT_TYPE(ev) )
        {
            case GST_EVENT_SEGMENT:
                {
                    const GstSegment *seg=NULL;
                    gst_event_parse_segment (ev,&seg) ;

                    GST_WARNING_OBJECT (ptr, "pad '%s' Seg= %" GST_SEGMENT_FORMAT " ", GST_ELEMENT_NAME(pad), seg);
                }
                break;

            case GST_EVENT_TAG:
                break;

            default:
                GST_DEBUG_OBJECT (ptr, "pad '%s' %s", GST_ELEMENT_NAME(pad), GST_EVENT_TYPE_NAME(ev));
                break;
        }

        return GST_PAD_PROBE_OK ;

    }

#endif


};

// generally used to late bind to a demux
class gstMultiQueueWithTailBin : public gstMultiQueueBin
{
public:
    gstMultiQueueWithTailBin(pluginContainer<GstElement> *parent,const char *tailelement):
        gstMultiQueueBin(parent,0,false,0,false,"multiQTailedBin")
    {
        m_tailname=tailelement;
        // and advertise the q
        advertiseElementsPadTemplates("multiqueueDemux");
    }


    virtual GstPad *request_new_pad (GstElement * element,GstPadTemplate * templ,const gchar * name,const GstCaps * caps)
    {
        GstPad *retPad=NULL;
        GstElement *mqDemux=pluginContainer<GstElement>::FindNamedPlugin("multiqueueDemux");
        // ask the mq src for it, ghost it, join the new mq sink to the tail - profit
        GstPad *newPad=gst_element_request_pad(
            mqDemux,
            templ,
            NULL,NULL);
        if(newPad)
        {
            addPadToBeReleased(mqDemux,newPad);

            char tailDisguise[28];
            snprintf(tailDisguise,sizeof(tailDisguise),"mq%lu",m_padsToBeReleased.size());

            pluginContainer<GstElement>::AddPlugin(m_tailname.c_str(),tailDisguise);

            gst_element_link(mqDemux,
                pluginContainer<GstElement>::FindNamedPlugin(tailDisguise)
                );

            // and ghost the mq sink pin 
            retPad=GhostSingleRequestPad(newPad);
        }

        return retPad;

    }

    ~gstMultiQueueWithTailBin()
    {
        releaseRequestedPads();
    }

protected:

    std::string m_tailname;

};

class gstCapsFilterBaseBin: public gstreamBin
{
public:
    // takes ownership of the passed caps
    gstCapsFilterBaseBin(pluginContainer<GstElement> *parent,GstCaps*caps,const char *name="capsFilterBin"):
        gstreamBin(name,parent),
        m_filterCaps(caps)
    {
        pluginContainer<GstElement>::AddPlugin("capsfilter");
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("capsfilter"), 
            "caps", caps, NULL);

    }


    gstCapsFilterBaseBin(pluginContainer<GstElement> *parent,const char*caps,const char *name="capsFilterBin"):
        gstreamBin(name,parent),
        m_filterCaps(NULL)
    {
        pluginContainer<GstElement>::AddPlugin("capsfilter");
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("capsfilter"), 
            "caps", m_filterCaps=gst_caps_new_simple(caps, NULL, NULL), NULL);

    }



    ~gstCapsFilterBaseBin()
    {
        if(m_filterCaps)
            gst_caps_unref(m_filterCaps);
    }

protected:

    GstCaps *m_filterCaps;

};


class gstCapsFilterSimple : public gstCapsFilterBaseBin
{
public:
    gstCapsFilterSimple(pluginContainer<GstElement> *parent,const char*caps,const char *name="capsFilterBin"):
        gstCapsFilterBaseBin(parent,caps,name)
    {
        AddGhostPads("capsfilter","capsfilter");
    }



};


class gstFakeSinkForCapsSimple : public gstCapsFilterBaseBin
{
public:    
    gstFakeSinkForCapsSimple(gstreamPipeline *parent,const char*caps,const char *name="fakeSinkBinForCapsSimple"):
        gstCapsFilterBaseBin(parent,caps,name)
    {
        pluginContainer<GstElement>::AddPlugin("fakesink");

        gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("capsfilter"),pluginContainer<GstElement>::FindNamedPlugin("fakesink"));

        AddGhostPads("capsfilter");

    }

};


class gstTeeBin : public gstreamBin
{
public:
    gstTeeBin(pluginContainer<GstElement> *parent,const char * caps,unsigned count=2, const char *name="teeBin"):gstreamBin(name,parent),
        m_caps(this,caps)
    {
        pluginContainer<GstElement>::AddPlugin("tee");
        // so join them up
        gst_element_link(pluginContainer<GstElement>::FindNamedPlugin(m_caps),FindNamedPlugin("tee"));        
        // every tee goes thru the mq, threads for free
        pluginContainer<GstElement>::AddPlugin("multiqueue");

        for(unsigned n=0;n<count;n++)
        {
            GstPad *subPad=gst_element_get_request_pad(pluginContainer<GstElement>::FindNamedPlugin("tee"),"src_%u");
            addPadToBeReleased(pluginContainer<GstElement>::FindNamedPlugin("tee"),subPad);
            gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("tee"),pluginContainer<GstElement>::FindNamedPlugin("multiqueue"));
        }

        AddGhostPads(m_caps, "multiqueue");

        advertiseElementsPadTemplates("tee");

    }

    ~gstTeeBin()
    {
        releaseRequestedPads();
    }

protected:

    // TODO this is untested, and probs broken
    virtual GstPad *request_new_pad (GstElement * element,GstPadTemplate * templ,const gchar * name,const GstCaps * caps)
    {
        // this should be passed to the tee
        GstPad *newT=gst_element_get_request_pad(pluginContainer<GstElement>::FindNamedPlugin("tee"),name);
        if(newT)
        {
            addPadToBeReleased(pluginContainer<GstElement>::FindNamedPlugin("tee"),newT);
            gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("tee"),pluginContainer<GstElement>::FindNamedPlugin("multiqueue"));
        }
        return NULL;
    }


    gstCapsFilterSimple m_caps;
};


class gstFrameRateBin :  public gstCapsFilterBaseBin
{
public:
    // framerate 12 framerateDivisor 2 would be 12.5 fps
    gstFrameRateBin(gstreamPipeline *parent,unsigned framerate, unsigned framerateDivisor=1,const char *name="frameRateBin"):
        gstCapsFilterBaseBin(parent,gst_caps_new_simple("video/x-raw","framerate",GST_TYPE_FRACTION, framerate,framerateDivisor, NULL),name)
    {
        pluginContainer<GstElement>::AddPlugin("videorate");

        if(gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("videorate"),pluginContainer<GstElement>::FindNamedPlugin("capsfilter")))
        {
            AddGhostPads("videorate","capsfilter");
        }
        else
        {
            GST_ERROR_OBJECT (m_parent, "Connecting gstFrameRateBin failed");
        }

    }


};


/*
left (0) – left
center (1) – center
right (2) – right
*/

/*
baseline (0) – baseline
bottom (1) – bottom
top (2) – top
Absolute position clamped to canvas (3) – position
center (4) – center
Absolute position (5) – absolute
*/

class gstNamedVideo : public gstreamBin
{

public:

    gstNamedVideo(pluginContainer<GstElement> *parent, const char *text,const char *name, int valign=1, int halign=1,const char* fontDesc="Sans, 12"):gstreamBin(name,parent)
    {
        // https://gstreamer.freedesktop.org/documentation/pango/GstBaseTextOverlay.html?gi-language=c#properties
        pluginContainer<GstElement>::AddPlugin("textoverlay");
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("textoverlay"), 
            "text",text,
            "font-desc", fontDesc,
            "valignment", valign,
            "halignment",halign, 
            "wait-text", FALSE,
            NULL);

        // block off the subtitle sink, so we don't ghost it
        // textoverlay assumes it's running subtitles if it's linked
        AddGhostPads("textoverlay","textoverlay","video/x-raw");


    }

};

// all the videos into mixer need to have alpha added to them!!
// not documented anywhere!
class gstVideoScaleBin : public gstCapsFilterBaseBin
{
public:
    gstVideoScaleBin(pluginContainer<GstElement> *parent,int width, int height, int offsetTop=0, int offsetLeft=0, const char *name="vidScaleBox"):
        gstCapsFilterBaseBin(parent, gst_caps_new_simple("video/x-raw","width",G_TYPE_INT, width,"height",G_TYPE_INT,height, NULL),name)
    {
        // optimisation - if offsets==0 then exclude the videobox, it will make the pipeline a lot quicker

        // add a scaler
        pluginContainer<GstElement>::AddPlugin("videoscale");

        if(offsetTop || offsetLeft)
        {
            // add a videobox
            pluginContainer<GstElement>::AddPlugin("videobox");
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("videobox"), 
                "top", offsetTop, "left",offsetLeft, "border-alpha",0.0, NULL);

            gst_element_link_many (pluginContainer<GstElement>::FindNamedPlugin("videoscale"),
                pluginContainer<GstElement>::FindNamedPlugin("capsfilter"),
                pluginContainer<GstElement>::FindNamedPlugin("videobox"),
                NULL
                );

            AddGhostPads("videoscale","videobox");
        }
        else
        {
            gst_element_link_many (pluginContainer<GstElement>::FindNamedPlugin("videoscale"),
                pluginContainer<GstElement>::FindNamedPlugin("capsfilter"),
                NULL
                );

            AddGhostPads("videoscale","capsfilter");

        }

    }
};

// zorder is VERY important here - basically, latest is higher
// sink0 is lower, sinkN is higher
class gstVideoMixerBin : public gstCapsFilterBaseBin
{
public:

    // if we're doing composite mixing, and there is border-padding, we need to add an alpha channel to each incoming video
    gstVideoMixerBin(pluginContainer<GstElement> *parent, unsigned numSinks,bool addAlpha=false, const char *name="videoMixerBin"):
        gstCapsFilterBaseBin(parent,gst_caps_new_simple("video/x-raw","format",G_TYPE_STRING, "RGB", NULL),name)
    {
        // add a multiqueueMixer at the front
        pluginContainer<GstElement>::AddPlugin("multiqueue","multiqueueMixer");
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("multiqueueMixer"), 
            "use_buffering", TRUE, NULL);

        // then a mixer
        pluginContainer<GstElement>::AddPlugin("videomixer");




        // TODO check this fn - enabling it stops things negotiating, and shouldn't that by RGBA?
        // multisink won't expose srcs until the sinks are used, and we're going to request those
        if(addAlpha)
        {
            // then remove alpha
            pluginContainer<GstElement>::AddPlugin("videoconvert");

            gst_element_link_many (
                pluginContainer<GstElement>::FindNamedPlugin("videomixer"),
                pluginContainer<GstElement>::FindNamedPlugin("videoconvert"),
                pluginContainer<GstElement>::FindNamedPlugin("capsfilter"),
                NULL
                );
        }

        // i WANT to do this using the request_new_pad virtual, but that's harder
        for(auto each=0;each<numSinks;each++)
        {
            GstPadTemplate *sinkTemplate=gst_pad_template_new("sink_%d",GST_PAD_SINK,GST_PAD_REQUEST,gst_caps_new_any());
            GstPad *newPad=gst_element_request_pad(
                pluginContainer<GstElement>::FindNamedPlugin("multiqueueMixer"),
                sinkTemplate,
                NULL,NULL);
            if(newPad)
            {
                addPadToBeReleased(pluginContainer<GstElement>::FindNamedPlugin("multiqueueMixer"),newPad);

                if(addAlpha)
                {
                    char alphaName[20];
                    snprintf(alphaName, sizeof(alphaName)-1,"alpha_%u",each);
                    pluginContainer<GstElement>::AddPlugin("alpha",alphaName);

                    g_object_set (pluginContainer<GstElement>::FindNamedPlugin(alphaName), 
                        "alpha", 1.0, NULL);

                    gst_element_link_many(pluginContainer<GstElement>::FindNamedPlugin("multiqueueMixer"),
                        pluginContainer<GstElement>::FindNamedPlugin(alphaName),
                        pluginContainer<GstElement>::FindNamedPlugin("videomixer"),
                        NULL);

                }
                else
                {
                    gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("multiqueueMixer"),
                        pluginContainer<GstElement>::FindNamedPlugin("videomixer"));
                }
                gst_object_unref(sinkTemplate);
            }
        }

        if(addAlpha)
            AddGhostPads("multiqueueMixer","capsfilter");
        else
            AddGhostPads("multiqueueMixer","videomixer");



    }


};


class gstH264encoderBin : public gstreamBin
{
public:
    gstH264encoderBin(pluginContainer<GstElement> *parent,const char *name="encoderBin"):
        gstreamBin(name,parent)
    {
        if(pluginContainer<GstElement>::AddPlugin("vaapih264enc","encoder",NULL,false))
        {
            GST_WARNING_OBJECT (m_parent, "Failed to create vaapih264enc, trying v4l2h264enc");
            if(pluginContainer<GstElement>::AddPlugin("v4l2h264enc","encoder",NULL,false))
            {
                GST_WARNING_OBJECT (m_parent, "Failed to create v4l2h264enc, trying x264enc");
                if(pluginContainer<GstElement>::AddPlugin("x264enc","encoder"))
                {
                    GST_ERROR_OBJECT (m_parent, "Failed to create x264enc, pretty fatal");
                }
            }
        }

        pluginContainer<GstElement>::AddPlugin("h264parse");


        gst_element_link_many( pluginContainer<GstElement>::FindNamedPlugin("encoder"), 
            pluginContainer<GstElement>::FindNamedPlugin("h264parse"), 
            NULL);

        AddGhostPads("encoder","h264parse");

    }


};

class gstTestSourceBin : public gstCapsFilterBaseBin
{
protected:

    gstH264encoderBin m_encoder;
    gstFrameBufferProgress m_progress;

public:
    gstTestSourceBin(pluginContainer<GstElement> *parent,unsigned framerate,unsigned width=1280, unsigned height=960, const char *name="TestSrcBin"):
        gstCapsFilterBaseBin(parent,gst_caps_new_simple("video/x-raw","framerate",GST_TYPE_FRACTION, framerate,1, "width",G_TYPE_INT,width,"height",G_TYPE_INT,height,NULL),name),
        m_encoder(this),
        m_progress(this)
    {
        pluginContainer<GstElement>::AddPlugin("videotestsrc");
        
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("videotestsrc"), 
            "is-live",TRUE,
            "pattern",18,   // ball
            NULL);

        gst_element_link_many( pluginContainer<GstElement>::FindNamedPlugin("videotestsrc"), 
            pluginContainer<GstElement>::FindNamedPlugin("capsfilter"), 
            pluginContainer<GstElement>::FindNamedPlugin(m_progress),
            pluginContainer<GstElement>::FindNamedPlugin(m_encoder), 
            NULL);

        AddGhostPads(NULL,m_encoder);
    }
};

#endif // _myelementbins_guard
