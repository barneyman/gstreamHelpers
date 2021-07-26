#ifndef _mydemuxbins_guard
#define _mydemuxbins_guard

#include "gstreamBin.h"


class demuxInfo
{
public:
    demuxInfo():
        m_binDuration(0)
    {

    }

    ~demuxInfo()
    {
        for(auto each=m_demuxSrcs.begin();each!=m_demuxSrcs.end();each++)
        {
            gst_caps_unref(*each);
        }
    }

    virtual demuxInfo& operator=(const demuxInfo &other)
    {
        // copy the other vector adding ref
        for(auto each=other.m_demuxSrcs.begin();each!=other.m_demuxSrcs.end();each++)
        {
            GstCaps *thisOne=*each;
            gst_caps_ref(thisOne);
            m_demuxSrcs.push_back(thisOne);
        }
        m_binDuration=other.m_binDuration;
        return *this;
    }

    bool isEmpty() { return m_demuxSrcs.size()==0; }

    unsigned numSrcStreams() { return m_demuxSrcs.size(); }
    unsigned numVideoStreams() { return countTheseCaps("video/x-h264"); }
    unsigned numAudioStreams() { return countTheseCaps("audio/ANY"); }
    unsigned numSubtitleStreams() { return countTheseCaps("text/x-raw"); }

    GstClockTime muxerDuration() { return m_binDuration; }

    std::vector<GstCaps*> m_demuxSrcs;

protected:
    unsigned countTheseCaps(const char *caps)
    {
        GstCaps *videoCaps=gst_caps_new_simple (caps,NULL,NULL);
        unsigned count=countIntersects(videoCaps); 
        gst_caps_unref(videoCaps);
        return count;
    }

    unsigned countIntersects(GstCaps *with)
    {
        unsigned count=0;
        for(auto each=m_demuxSrcs.begin();each!=m_demuxSrcs.end();each++)
        {
            if(gst_caps_can_intersect (*each,with))
                count++;
        }
        return count;
    }



    gint64 m_binDuration;

};


// used to work out what's in a mux'd stream
// opens it up to a demux then counts what that finds
class gstDemuxDecodeBinExamine : public gstreamListeningBin, public demuxInfo
{
protected:

    gstMultiQueueWithTailBin m_faketail;

public:

    gstDemuxDecodeBinExamine(gstreamPipeline *parent,const char*mkvName,const char *demuxer,const char *name="gstDemuxDecodeBinExamine"):
        gstreamListeningBin(name,parent),
        m_faketail(this,"fakesink")
    {
        // we always need a demux
        AddPlugin(demuxer,"demuxer");
        
        // work out if we have a single ource, or it's up to the demuxer to work it out
        bool splitDemux=(demuxer=="splitmuxsrc");

        if(!splitDemux)            
        {
            AddPlugin("filesrc");
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("filesrc"), 
                "location", mkvName, NULL);
            gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("filesrc"),pluginContainer<GstElement>::FindNamedPlugin("demuxer"));
        }
        else
        {
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("demuxer"), 
                "location", mkvName, NULL);
        }


        ConnectLate("demuxer", m_faketail);


    }

    bool Run(gstreamPipeline *parent)
    {

        if(!parent->PreRoll())
            return false;

        // spin until the demux has prerolled into the queue - this should probably timeout, probably ...
        while(!m_demuxerFinished) 
        {
            sched_yield();
        }

        parent->DumpGraph("gstDemuxDecodeBinExamine");

        // can we query the muxsrc for duration?
        if(gst_element_query_duration (pluginContainer<GstElement>::FindNamedPlugin("demuxer"),GST_FORMAT_TIME, &m_binDuration))
        {
            GST_INFO_OBJECT (m_parent, "duration %" GST_TIME_FORMAT "", GST_TIME_ARGS((GstClockTime)m_binDuration));
        }
        else
        {
            GST_WARNING_OBJECT (m_parent, "Could not query duration");
        }

        // then count the number of srcpads on the demuxer
        GstElement *demux=pluginContainer<GstElement>::FindNamedPlugin("demuxer");

        for(GList *sourcePads=demux->srcpads;sourcePads;sourcePads=sourcePads->next)
        {
            GstPad *eachSourcePad=(GstPad *)sourcePads->data;

            //GstCaps *padCaps=gst_pad_query_caps(eachSourcePad,NULL);
            GstCaps *padCaps=gst_pad_get_current_caps(eachSourcePad);

            GST_INFO_OBJECT (m_parent, "adding pad with %s",gst_caps_to_string(padCaps));

            m_demuxSrcs.push_back(padCaps);

        }
        parent->Stop();

        return true;

    }

    ~gstDemuxDecodeBinExamine()
    {
        // get my nails out of fakesink
        releaseRequestedPads();
    }


protected:





    

};

// the above class is very powerful, but can kill a well tended Pipeline, so this one 
// takes all the pain, and give you the result
class gstreamDemuxExamineDiscrete
{
public:

    gstreamDemuxExamineDiscrete(const char*mkvName,const char *demuxer)
    {
        // create a pipeline
        gstreamPipeline quickPipe("DiscreteQuickPipe");
        gstDemuxDecodeBinExamine examine(&quickPipe, mkvName, demuxer);

        examine.Run(&quickPipe);

        m_info=examine;

    }

    demuxInfo m_info;

    bool isValid() { return !m_info.isEmpty(); }


};



// this sets itself up by sampling the source file first, and prepping
// all the pathways after that - generic - concretes below use specific muxers
class gstDemuxDecodeBin : public gstreamListeningBin
{
public:

    demuxInfo streamInfo;

public:
    gstDemuxDecodeBin(gstreamPipeline *parent,const char*mkvName,const char *demuxer, GstClockTime startAt=GST_CLOCK_TIME_NONE ,GstClockTime endAt=GST_CLOCK_TIME_NONE ,bool decode=true, demuxInfo *cached=NULL, const char *name="demuxDecodeBin"):
        gstreamListeningBin(name,parent),
        m_fatal(false)
    {
        bool splitDemux=(demuxer=="splitmuxsrc");

        if(!cached || cached->isEmpty())
        {
            // now build our demux pipeline based on what we know is there
            // by creating a toy that exposes all available srcs
            gstreamDemuxExamineDiscrete examine(mkvName,demuxer);
            if(examine.isValid())
            {
                streamInfo=examine.m_info;
                // store it forward
                if(cached)
                    *cached=examine.m_info;
            }
        }
        else
        {
            streamInfo=*cached;
        }

        // check
        if(streamInfo.isEmpty())
        {
            GST_ERROR_OBJECT (m_parent, "Failed to get demux stream information");            
            m_fatal=true;
            return;
        }

        std::vector<GstCaps*> demuxSrcs=streamInfo.m_demuxSrcs;

        bool seeking=(startAt!=GST_CLOCK_TIME_NONE  && endAt!=GST_CLOCK_TIME_NONE)?true:false;

        pluginContainer<GstElement>::AddPlugin(demuxer,"demuxer");

        if(!splitDemux)            
        {
            pluginContainer<GstElement>::AddPlugin("filesrc");
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("filesrc"), 
                "location", mkvName, NULL);
        }
        else
        {
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("demuxer"), 
                "location", mkvName, NULL);

        }

        pluginContainer<GstElement>::AddPlugin("multiqueue","multiQdemux");

        if(!splitDemux)
        {
            gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("filesrc"),pluginContainer<GstElement>::FindNamedPlugin("demuxer"));
        }


        // i have to ghost pads that don't even exist yet ... so put tails on, and ghost them
        int padCount=0;
        for(auto each=demuxSrcs.begin();each!=demuxSrcs.end();each++,padCount++)
        {
            char capsFilterName[20];
            snprintf(capsFilterName,sizeof(capsFilterName)-1,"capsfilt_%u",padCount);
            pluginContainer<GstElement>::AddPlugin("capsfilter",capsFilterName);

            // set the caps, add a ref?
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin(capsFilterName), 
                "caps", gst_caps_copy (*each), NULL);

            // depends on the caps
            if(capIntersects(*each,"video/x-h264"))
            {
                if(decode)
                {
                    // need a parser and a decoder
                    char parserName[20], decoderName[20];
                    snprintf(parserName,sizeof(parserName)-1,"parse_%u",padCount);
                    snprintf(decoderName,sizeof(decoderName)-1,"decode_%u",padCount);

                    // this will implicitly get the video stream
                    pluginContainer<GstElement>::AddPlugin("h264parse",parserName);

    //                if(pluginContainer<GstElement>::AddPlugin("vaapih264dec",decoderName))
                    {
    //                    GST_DEBUG_OBJECT (m_parent, "Failed to create vaapih264dec, trying v4l2h264dec");

                        // don't complain to logs if this fails
                        if(pluginContainer<GstElement>::AddPlugin("v4l2h264dec",decoderName,NULL,false))
                        {
                            GST_DEBUG_OBJECT (m_parent, "Failed to create v4l2h264dec, trying avdec_h264");

                            if(pluginContainer<GstElement>::AddPlugin("avdec_h264",decoderName))
                            {
                                GST_ERROR_OBJECT (m_parent, "Failed to create avdec_h264 - no decoder - pretty fatal");
                            }
                        }
                    }

                    gst_element_link_many(  pluginContainer<GstElement>::FindNamedPlugin(capsFilterName),
                        pluginContainer<GstElement>::FindNamedPlugin("multiQdemux"),
                        pluginContainer<GstElement>::FindNamedPlugin(parserName),
                        pluginContainer<GstElement>::FindNamedPlugin(decoderName),
                        NULL);

                    ConnectLate(pluginContainer<GstElement>::FindNamedPlugin("demuxer"),pluginContainer<GstElement>::FindNamedPlugin(capsFilterName));

                    AddGhostPads(NULL,decoderName);
                }
                else
                {
                    gst_element_link_many(pluginContainer<GstElement>::FindNamedPlugin(capsFilterName),pluginContainer<GstElement>::FindNamedPlugin("multiQdemux"),NULL);

                    // hijack the parent's late linking framework
                    ConnectLate(pluginContainer<GstElement>::FindNamedPlugin("demuxer"),pluginContainer<GstElement>::FindNamedPlugin(capsFilterName));

                    AddGhostPads(NULL,"multiQdemux");

                }

            }
            else if(capIntersects(*each,"text/x-raw"))
            {
                gst_element_link_many(pluginContainer<GstElement>::FindNamedPlugin(capsFilterName),pluginContainer<GstElement>::FindNamedPlugin("multiQdemux"),NULL);

                // hijack the parent's late linking framework
                ConnectLate(pluginContainer<GstElement>::FindNamedPlugin("demuxer"),pluginContainer<GstElement>::FindNamedPlugin(capsFilterName));

                AddGhostPads(NULL,"multiQdemux");

            }
            else
            {
                GST_ERROR_OBJECT (m_parent, "Connecting '%s' pad not yet impl'd", gst_caps_to_string(*each));
            }
        }

        if(seeking)
        {
            // demux is single threaded, so having a blocked pad on it, stops the other pads connecting, so block the q's srcs
            GstElement *mq=pluginContainer<GstElement>::FindNamedPlugin("multiQdemux");
            //for(GList *sourcePads=mq->srcpads;sourcePads;sourcePads=sourcePads->next)
            for(GList *sourcePads=mq->sinkpads;sourcePads;sourcePads=sourcePads->next)
            {
                GstPad *eachSourcePad=(GstPad *)sourcePads->data;
                GST_INFO_OBJECT (m_parent, "adding BLOCK to %s:%s",GST_ELEMENT_NAME(mq),GST_ELEMENT_NAME(eachSourcePad));
                if(!parent->BlockPadForSeek(eachSourcePad))
                {
                    GST_ERROR_OBJECT (m_parent, "BlockPadForSeek failed for %s:%s", GST_ELEMENT_NAME(mq),GST_ELEMENT_NAME(eachSourcePad));                    
                }
            }


            parent->SeekOnElementLate(startAt,endAt, pluginContainer<GstElement>::FindNamedPlugin("demuxer"));
        }
      

        // a 'feature' of ConnectPipelineLate is that it will connect all the pins ... 
        // so my subtitle pad should get linked to multiqueue too


    }

    ~gstDemuxDecodeBin()
    {
        releaseRequestedPads();
    }


    bool fatalError() { return m_fatal; }

protected:

    bool capIntersects(GstCaps *caps, const char *testcapsstr)
    {
        GstCaps *testCaps=gst_caps_new_simple (testcapsstr,NULL,NULL);
        bool intersect=(gst_caps_can_intersect (caps,testCaps))?true:false;
        gst_caps_unref(testCaps);
        return intersect;

    }


    bool m_fatal;


};


class gstMKVDemuxDecodeBin : public gstDemuxDecodeBin
{
public:
    gstMKVDemuxDecodeBin(gstreamPipeline *parent,const char*location,GstClockTime startAt=GST_CLOCK_TIME_NONE ,GstClockTime endAt=GST_CLOCK_TIME_NONE ,const char *name="demuxDecodeBin"):
        gstDemuxDecodeBin(parent,location,"matroskademux",startAt,endAt,name)
        {

        }


};

class gstMP4DemuxDecodeBin : public gstDemuxDecodeBin
{
public:
    gstMP4DemuxDecodeBin(gstreamPipeline *parent,const char*location,GstClockTime startAt=GST_CLOCK_TIME_NONE ,GstClockTime endAt=GST_CLOCK_TIME_NONE ,bool decode=true,demuxInfo *info=NULL,const char *name="demuxMP4DecodeBin"):
        gstDemuxDecodeBin(parent,location,"qtdemux",startAt,endAt,decode,info,name)
        {

        }

};

#include <glib.h>

class gstMP4DemuxDecodeSparseBin : public gstDemuxDecodeBin
{
public:
    gstMP4DemuxDecodeSparseBin(gstreamPipeline *parent,const char*location,GstClockTime startAt=GST_CLOCK_TIME_NONE ,GstClockTime endAt=GST_CLOCK_TIME_NONE ,bool decode=true,demuxInfo *info=NULL,const char *name="demuxMP4DecodeSparseBin"):
        gstDemuxDecodeBin(parent,location,"splitmuxsrc",startAt,endAt,decode,info,name)
        {

        }

    // ctor that know about multiple files (needs gstreamer 1.8)
    gstMP4DemuxDecodeSparseBin(gstreamPipeline *parent,std::vector<std::string> &locations,GstClockTime startAt=GST_CLOCK_TIME_NONE ,GstClockTime endAt=GST_CLOCK_TIME_NONE ,const char *name="demuxMP4DecodeSparseBin"):
        gstDemuxDecodeBin(parent,"tmp","splitmuxsrc",startAt,endAt,name)
        {
            g_signal_connect (pluginContainer<GstElement>::FindNamedPlugin("splitmuxsrc"), "format-location", G_CALLBACK (staticFormatLocation), this);
            m_locations=locations;
        }


protected:

    static GStrv *staticFormatLocation(GstElement *element,gpointer data)
    {
        return ((gstMP4DemuxDecodeSparseBin*)data)->formatLocation(element);
    }


    GStrv *formatLocation(GstElement *element)
    {
        //GStrvBuilder *builder=g_strv_builder_new();
        auto builder=g_ptr_array_new_with_free_func (g_free);

        for(auto each=m_locations.begin();each!=m_locations.end();each++)
        {
            //g_strv_builder_add (builder, each->c_str());
            g_ptr_array_add (builder, g_strdup (each->c_str()));
        }

        //GStrv *array=g_strv_builder_end(builder);
        g_ptr_array_add (builder, NULL);
        //auto array=g_ptr_array_steal (builder, NULL);

        //g_strv_builder_unref (builder);
        //g_object_unref(builder);

        return (GStrv*)builder;
    }

    std::vector<std::string> m_locations;


};



class gstDemuxSubsOnlyDecodeBin : public gstCapsFilterBaseBin
{
public:
    gstDemuxSubsOnlyDecodeBin(gstreamPipeline *parent,const char*mkvName,const char *demuxer, GstClockTime startAt=GST_CLOCK_TIME_NONE ,GstClockTime endAt=GST_CLOCK_TIME_NONE , const char *name="demuxDecodeBin"):
        gstCapsFilterBaseBin(parent,gst_caps_new_simple("text/x-raw","format",G_TYPE_STRING, "utf8", NULL),name)
    {
        bool seeking=(startAt!=GST_CLOCK_TIME_NONE  && endAt!=GST_CLOCK_TIME_NONE)?true:false;

        // hack!!
        bool splitDemux=(demuxer=="splitmuxsrc");

        if(!splitDemux)
        {
            pluginContainer<GstElement>::AddPlugin("filesrc");
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("filesrc"), 
                "location", mkvName, NULL);
        }

        pluginContainer<GstElement>::AddPlugin(demuxer,"demuxer");

        if(splitDemux)
        {
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("demuxer"), 
                "location", mkvName, NULL);
        }

#ifdef _EXIT_THRU_MQ_GIFT_SHOP        
        pluginContainer<GstElement>::AddPlugin("multiqueue");
#endif

        if(!splitDemux)
        {
            gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("filesrc"),pluginContainer<GstElement>::FindNamedPlugin("demuxer"));
        }

#ifdef _EXIT_THRU_MQ_GIFT_SHOP
        gst_element_link_many(pluginContainer<GstElement>::FindNamedPlugin("capsfilter"),pluginContainer<GstElement>::FindNamedPlugin("multiqueue"),NULL);
#endif        

        // hijack the parent's late linking framework
        parent->ConnectPipelineLate(pluginContainer<GstElement>::FindNamedPlugin("demuxer"),pluginContainer<GstElement>::FindNamedPlugin("capsfilter"));

#ifdef _EXIT_THRU_MQ_GIFT_SHOP
        AddGhostPads(NULL,"multiqueue");
#else        
        AddGhostPads(NULL,"capsfilter");
#endif        

        if(seeking)
        {
            // demux is single threaded, so having a blocked pad on it, stops the other pads connecting, so block the q's srcs
            GstElement *demux=pluginContainer<GstElement>::FindNamedPlugin("capsfilter");
            for(GList *sourcePads=demux->srcpads;sourcePads;sourcePads=sourcePads->next)
            {
                GstPad *eachSourcePad=(GstPad *)sourcePads->data;
                GST_INFO_OBJECT (m_parent, "adding BLOCK to %s:%s",GST_ELEMENT_NAME(demux),GST_ELEMENT_NAME(eachSourcePad));
                parent->BlockPadForSeek(eachSourcePad);
            }

            parent->SeekOnElementLate(startAt,endAt, pluginContainer<GstElement>::FindNamedPlugin("demuxer"));
        }
      

        // a 'feature' of ConnectPipelineLate is that it will connect all the pins ... 
        // so my subtitle pad should get linked to multiqueue too


    }


protected:


};

// this doesn't work - it blocks in the splitmuxsrc,
// probably because the internal demuxer video pin IS connected to something
// internal to the splitmuxsrc bin, and not to me
class gstDemuxMP4SubsOnlyDecodeSparseBin: public gstDemuxSubsOnlyDecodeBin
{
public:
    gstDemuxMP4SubsOnlyDecodeSparseBin(gstreamPipeline *parent,const char*location,GstClockTime startAt=GST_CLOCK_TIME_NONE ,GstClockTime endAt=GST_CLOCK_TIME_NONE ,const char *name="demuxDecodeBinSparse"):
        gstDemuxSubsOnlyDecodeBin(parent,location,"splitmuxsrc",startAt,endAt,name)        
        {

        }

};


class gstDemuxMP4SubsOnlyDecodeBin: public gstDemuxSubsOnlyDecodeBin
{
public:
    gstDemuxMP4SubsOnlyDecodeBin(gstreamPipeline *parent,const char*location,GstClockTime startAt=GST_CLOCK_TIME_NONE ,GstClockTime endAt=GST_CLOCK_TIME_NONE ,const char *name="demuxDecodeBin"):
        gstDemuxSubsOnlyDecodeBin(parent,location,"qtdemux",startAt,endAt,name)        
        {

        }

};

#endif // _mydemuxbins_guard

