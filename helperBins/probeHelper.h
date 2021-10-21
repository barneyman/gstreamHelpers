#include "../gstreamPipeline.h"

class padProber
{

protected:

    pluginContainer<GstElement> *m_parent;
    GstPadProbeType m_probeType;

public:

    padProber(pluginContainer<GstElement> *parent, GstPadProbeType probeType=GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM):
        m_parent(parent),
        m_probeType(probeType)
    {

    }

    bool attachProbes(const char*elementName, bool src=true)
    {
        // find the element
        GstElement *found=m_parent->FindNamedPlugin(elementName);

        if(!found)
        {
            return FALSE;
        }

        // get it's pads
        GList *interestedPads=(src?found->srcpads:found->sinkpads);

        for(interestedPads;interestedPads;interestedPads=interestedPads->next)
        {
            GstPad *eachSourcePad=(GstPad *)interestedPads->data;
            //GST_INFO_OBJECT (m_parent, "adding BLOCK to %s:%s",GST_ELEMENT_NAME(found),GST_ELEMENT_NAME(eachSourcePad));
            
            gst_pad_add_probe(eachSourcePad, m_probeType, staticPadProbe, this, NULL);
            // if(!parent->BlockPadForSeek(eachSourcePad))
            // {
            //     GST_ERROR_OBJECT (m_parent, "BlockPadForSeek failed for %s:%s", GST_ELEMENT_NAME(mq),GST_ELEMENT_NAME(eachSourcePad));                    
            // }
        }
    }

    protected:

    static GstPadProbeReturn staticPadProbe(GstPad * pad,GstPadProbeInfo * info,gpointer user_data)
    {
        padProber*pp=(padProber*)user_data;
        return pp->padProbe(pad, info);
    }

    virtual GstPadProbeReturn padProbe(GstPad * pad,GstPadProbeInfo * info)
    {

        switch(info->type & (unsigned)m_probeType)
        {
            case GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM:
                eventProbe(pad,info);
                break;

            case GST_PAD_PROBE_TYPE_BUFFER:
                bufferProbe(pad,info);
                break;

            case GST_PAD_PROBE_TYPE_BUFFER_LIST:
                bufferListProbe(pad,info);
                break;

            default:
                g_printerr ("padProbe unexpected probetype %0x\n", info->type & m_probeType);                    
                break;

        }

        return GST_PAD_PROBE_OK;

    }

    virtual void eventProbe(GstPad * pad,GstPadProbeInfo * info)
    {
        GstEvent *ev=gst_pad_probe_info_get_event(info);
        g_print("saw event %s on pad %s\n", GST_EVENT_TYPE_NAME(ev), GST_PAD_NAME(pad));   
    }

    virtual void bufferProbe(GstPad * pad,GstPadProbeInfo * info)
    {
        GstBuffer *bf=gst_pad_probe_info_get_buffer (info);
        g_print("saw buffer at %" GST_TIME_FORMAT " on pad %s\n",GST_TIME_ARGS(bf->pts), GST_PAD_NAME(pad));   
    }

    virtual void bufferListProbe(GstPad * pad,GstPadProbeInfo * info)
    {
        g_print("saw bufferlist probe\n");
    }

};

class ptsPadProber : public padProber
{

    GstClockTime m_lastSeen;

public:
   
    ptsPadProber(pluginContainer<GstElement> *parent):
        padProber(parent, GST_PAD_PROBE_TYPE_BUFFER),m_lastSeen(GST_CLOCK_TIME_NONE)
    {

    }

    virtual void bufferProbe(GstPad * pad,GstPadProbeInfo * info)
    {
        GstBuffer *bf=gst_pad_probe_info_get_buffer (info);
        if(m_lastSeen!=GST_CLOCK_TIME_NONE)
        {
            if(bf->pts==GST_CLOCK_TIME_NONE)
            {
                g_printerr("Saw GST_TIME_NONE on pad %s - previous pts was %" GST_TIME_FORMAT "\n", GST_PAD_NAME(pad), GST_TIME_ARGS(m_lastSeen));
            }
            else if(bf->pts<=m_lastSeen)
            {
                g_printerr("Saw duplicated pts %" GST_TIME_FORMAT "  - previous pts was %" GST_TIME_FORMAT " on pad %s\n",GST_TIME_ARGS(bf->pts),GST_TIME_ARGS(m_lastSeen), GST_PAD_NAME(pad));
            }

            if(GST_BUFFER_IS_DISCONT(bf) )
            {
                g_printerr("DISCONT seen at %" GST_TIME_FORMAT "\n",GST_TIME_ARGS(bf->pts));
            }
        }
        m_lastSeen=bf->pts;
    }




};