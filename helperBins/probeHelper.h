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

    bool attachProbes(const char*elementName,GstPadProbeType probeType=GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM, bool src=true,pluginContainer<GstElement> *parent=NULL)
    {
        // find the element
        GstElement *found=(parent?parent:m_parent)->FindNamedPlugin(elementName);

        if(!found)
        {
            return false;
        }

        // get it's pads
        GList *interestedPads=(src?found->srcpads:found->sinkpads);

        for(interestedPads;interestedPads;interestedPads=interestedPads->next)
        {
            GstPad *eachSourcePad=(GstPad *)interestedPads->data;
            //GST_INFO_OBJECT (m_parent, "adding BLOCK to %s:%s",GST_ELEMENT_NAME(found),GST_ELEMENT_NAME(eachSourcePad));
            
            gst_pad_add_probe(eachSourcePad, probeType, staticPadProbe, this, NULL);
            // if(!parent->BlockPadForSeek(eachSourcePad))
            // {
            //     GST_ERROR_OBJECT (m_parent, "BlockPadForSeek failed for %s:%s", GST_ELEMENT_NAME(mq),GST_ELEMENT_NAME(eachSourcePad));                    
            // }
        }
        return true;
    }

    protected:

    static GstPadProbeReturn staticPadProbe(GstPad * pad,GstPadProbeInfo * info,gpointer user_data)
    {
        padProber*pp=(padProber*)user_data;
        return pp->padProbe(pad, info);
    }

    virtual GstPadProbeReturn padProbe(GstPad * pad,GstPadProbeInfo * info)
    {
        if(info->type & GST_PAD_PROBE_TYPE_BLOCK)
        {
            return blockProbe(pad,info);            
        }
        else
        {
            if(info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM)
            {
                eventProbe(pad,info);            
            }

            if(info->type & GST_PAD_PROBE_TYPE_BUFFER)
            {
                bufferProbe(pad,info);            
            }

            if(info->type & GST_PAD_PROBE_TYPE_BUFFER_LIST)
            {
                bufferListProbe(pad,info);            
            }
        }

        return GST_PAD_PROBE_OK;

    }

    std::string getProbeNames(GstPadProbeInfo * info)
    {   
        if(!info)
        {
            return "";
        }
        return getProbeNames(info->type);
    }

    std::string getProbeNames(GstPadProbeType type)
    {
        std::string ret("");
        if(type & GST_PAD_PROBE_TYPE_IDLE)
            ret+="IDLE ";
        if(type & GST_PAD_PROBE_TYPE_BLOCK)
            ret+="BLOCK ";
        /* flags to select datatypes */
        if(type & GST_PAD_PROBE_TYPE_BUFFER)
            ret+="BUFFER ";
        if(type & GST_PAD_PROBE_TYPE_BUFFER_LIST)
            ret+="BUFLIST ";
        if(type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM)
            ret+="DS ";
        if(type & GST_PAD_PROBE_TYPE_EVENT_UPSTREAM)
            ret+="US ";
        if(type & GST_PAD_PROBE_TYPE_EVENT_FLUSH)
            ret+="FLUSH ";
        if(type & GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM)
            ret+="QDS ";
        if(type & GST_PAD_PROBE_TYPE_QUERY_UPSTREAM)
            ret+="QUS ";
        /* flags to select scheduling mode */
        if(type & GST_PAD_PROBE_TYPE_PUSH)
            ret+="PUSH ";
        if(type & GST_PAD_PROBE_TYPE_PULL)
            ret+="PULL ";

        return ret;
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

    virtual GstPadProbeReturn blockProbe(GstPad * pad,GstPadProbeInfo * info)
    {
        return GST_PAD_PROBE_OK;
    }

};

class ptsPadProber : public padProber
{

    GstClockTime m_lastSeen;

public:
   
    ptsPadProber(pluginContainer<GstElement> *parent):
        padProber(parent),m_lastSeen(GST_CLOCK_TIME_NONE)
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