/*

bins that wrap elements, so complex property behaviour is easier to reuse

*/
#ifndef _myelementbins_guard
#define _myelementbins_guard

#include "gstreamBin.h"




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

// adds a progress report
class gstFrameBufferProgress : public gstreamBin
{
public:
    gstFrameBufferProgress(pluginContainer<GstElement> *parent,unsigned frequencyS=15, const char *name="progress"):gstreamBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("progressreport");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("progressreport"), 
            "update-freq", frequencyS, 
            NULL);

        AddGhostPads("progressreport","progressreport");

    }

};

class gstQueue : public gstreamBin
{
public:
    gstQueue(pluginContainer<GstElement> *parent,const char *name):gstreamBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("queue",name);
        AddGhostPads(name,name);
    }

};


class gstQueueMinimum : public gstQueue
{
public:
    gstQueueMinimum(pluginContainer<GstElement> *parent,const char *name, unsigned minSecs, unsigned maxSecs=0):
        gstQueue(parent,name)
    {
        if(minSecs)
        {
            if(maxSecs<minSecs)
                maxSecs=minSecs+5;
            g_object_set (pluginContainer<GstElement>::FindNamedPlugin(name), 
                "max-size-time", maxSecs*GST_SECOND, 
                "max-size-buffers", 0,
                "max-size-bytes", 0,
                "min-threshold-time", GST_SECOND*minSecs,
                NULL);
        }       

    }

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



// muxers need threads on their sinks, but they connect late
// so this gets in the way and adds a queue
class gstMultiQueueBin : public gstreamBin
{

public:

    gstMultiQueueBin(pluginContainer<GstElement> *parent,unsigned numSinks, bool buffering=false, int numberOfSeconds=2, bool syncOnRunning=true, const char *name="multiQBin"):gstreamBin(name,parent)
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
            "max-size-bytes", numberOfSeconds*4000000,
            "max-size-time", 0, //numberOfSeconds*GST_SECOND,
            "high-watermark", 0.75,
            NULL);


        for(auto each=0;each<numSinks;each++)
        {
            GstPadTemplate *sinkTemplate=gst_pad_template_new("sink_%d",GST_PAD_SINK,GST_PAD_REQUEST,gst_caps_new_any());
            GstPad *newPad=gst_element_request_pad(
                pluginContainer<GstElement>::FindNamedPlugin("multiqueueDemux"),
                sinkTemplate,
                NULL,NULL);
            if(newPad)
            {
                addPadToBeReleased(pluginContainer<GstElement>::FindNamedPlugin("multiqueueDemux"),newPad);
            }

        }

        AddGhostPads("multiqueueDemux","multiqueueDemux");

        // connect to the overrun signal
        g_signal_connect (pluginContainer<GstElement>::FindNamedPlugin("multiqueueDemux"), "overrun", G_CALLBACK (staticBufferOverrun), this);


    }


protected:

    static void staticBufferOverrun(GstElement *element,gpointer data)
    {
        return ((gstMultiQueueBin*)data)->bufferOverrun();
    }

    virtual void bufferOverrun()
    {
        // TODO find out which buffer is full
        GST_ERROR_OBJECT (this, "buffer exhausted %s", Name());
    }

};


class gstCapsFilterSimple : public gstreamBin
{
public:
    gstCapsFilterSimple(pluginContainer<GstElement> *parent,const char*caps,const char *name="capsFilterBin"):
        gstreamBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("capsfilter");
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("capsfilter"), 
            "caps", gst_caps_new_simple(caps, NULL, NULL), NULL);

        AddGhostPads("capsfilter","capsfilter");
    }
};

class gstFakeSinkForCapsSimple : public gstreamBin
{
public:    
    gstFakeSinkForCapsSimple(gstreamPipeline *parent,const char*caps,const char *name="fakeSinkBin"):
        gstreamBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("fakesink");
        pluginContainer<GstElement>::AddPlugin("capsfilter");
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("capsfilter"), 
            "caps", gst_caps_new_simple(caps, NULL, NULL), NULL);

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

protected:

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


class gstFrameRateBin :  public gstreamBin
{
public:
    gstFrameRateBin(gstreamPipeline *parent,unsigned framerate,const char *name="frameRateBin"):gstreamBin(name,parent)
    {
        pluginContainer<GstElement>::AddPlugin("videorate");
        pluginContainer<GstElement>::AddPlugin("capsfilter");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("capsfilter"), 
            "caps", gst_caps_new_simple("video/x-raw","framerate",GST_TYPE_FRACTION, framerate,1 , NULL), NULL);

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

    gstNamedVideo(pluginContainer<GstElement> *parent, const char *text, int valign=1, int halign=1,const char* fontDesc="Sans, 12"):gstreamBin(text,parent)
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
class gstVideoScaleBin : public gstreamBin
{
public:
    gstVideoScaleBin(pluginContainer<GstElement> *parent,int width, int height, int offsetTop=0, int offsetLeft=0, const char *name="vidbox"):gstreamBin(name,parent)
    {
        // optimisation - if offsets==0 then exclude the videobox, it will make the pipeline a lot quicker

        // add a scaler
        pluginContainer<GstElement>::AddPlugin("videoscale");
        // and the caps for it
        pluginContainer<GstElement>::AddPlugin("capsfilter");
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("capsfilter"), 
            "caps", gst_caps_new_simple("video/x-raw","width",G_TYPE_INT, width,"height",G_TYPE_INT,height, NULL), NULL);

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
class gstVideoMixerBin : public gstreamBin
{
public:

    // if we're doing composite mixing, and there is border-padding, we need to add an alpha channel to each incoming video
    gstVideoMixerBin(pluginContainer<GstElement> *parent, unsigned numSinks,bool addAlpha=false, const char *name="videoMixerBin"):gstreamBin(name,parent)
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
            pluginContainer<GstElement>::AddPlugin("capsfilter");

            g_object_set (pluginContainer<GstElement>::FindNamedPlugin("capsfilter"), 
                "caps", gst_caps_new_simple("video/x-raw","format",G_TYPE_STRING, "RGB", NULL), NULL);

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
            }
        }

        if(addAlpha)
            AddGhostPads("multiqueueMixer","capsfilter");
        else
            AddGhostPads("multiqueueMixer","videomixer");



    }


};

#endif // _myelementbins_guard