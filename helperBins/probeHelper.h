#include "../gstreamPipeline.h"

class padProber
{

protected:

    pluginContainer<GstElement> *m_parent;

public:

    padProber(pluginContainer<GstElement> *parent):
        m_parent(parent)
    {

    }

    bool attachProbes(const char*elementName, unsigned probetype=GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, bool src=true)
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
            
            gst_pad_add_probe(eachSourcePad,GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, staticPadProbe, this, NULL);
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

        switch(info->type & GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM)
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
                g_printerr ("padProbe unexpected probetype %0x\n", info->type & GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM);                    
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