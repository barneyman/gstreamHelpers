#include "gstreamBin.h"

#ifdef _USE_MY_BIN
#include "myplugins/gstmybin.h"
#endif


gstreamBin::gstreamBin(const char *binName, pluginContainer<GstElement> *parent):m_parent(parent),m_binName(binName)
{
    // create one
#ifdef _USE_MY_BIN
    // create my own gstbin that stores the ptr back to this class
    m_myBin=GST_ELEMENT(g_object_new (GST_TYPE_MYBIN,NULL));

    // preserve my this
    GstMyBin *mybininstance=GST_MYBIN(m_myBin);
    mybininstance->myClassPointer=this;
    gst_element_set_name (mybininstance,binName);
#else
    m_myBin=gst_bin_new (binName);
#endif        


    pluginContainerSetParent(m_myBin);

    // add myself to pipeline
    parent->AddElement(binName,GST_ELEMENT(m_myBin));

}    


bool gstreamBin::AddGhostPads(const char*sink,const char*source, const char *sinkcaps, const char *srccaps)
{
    // get all the sink pads from the sink, and ghost them
    GstElement *binSinkElement=pluginContainer<GstElement>::FindNamedPlugin(sink);
    GstElement *binSrcElement=pluginContainer<GstElement>::FindNamedPlugin(source);            

    return AddGhostPads(binSinkElement,binSrcElement,sinkcaps,srccaps);

}


bool gstreamBin::AddGhostPads(GstElement *sinkElement,GstElement*sourceElement, const char *sinkcaps, const char *srccaps)
{
    // get all the sink pads from the sink, and ghost them
    if(sinkElement)
    {
        GstCaps *allowedCaps=sinkcaps?gst_caps_new_full(gst_structure_from_string(sinkcaps,NULL),NULL):NULL;
        IterateAndGhost(sinkElement->sinkpads,m_ghostPadsSinks,allowedCaps);
    }
    // same with sources
    if(sourceElement)
    {
        GstCaps *allowedCaps=srccaps?gst_caps_new_full(gst_structure_from_string(srccaps,NULL),NULL):NULL;
        IterateAndGhost(sourceElement->srcpads,m_ghostPadsSrcs,allowedCaps);
    }

    return true;
    
}



void gstreamBin::IterateAndGhost(GList *elementPads, std::vector<GstPad*> &results, GstCaps *allowedCaps)
{
    for(;elementPads;elementPads=elementPads->next)
    {
        GstPad *eachPad=(GstPad *)elementPads->data;    

        if(allowedCaps)
        {
            GstCaps *padCaps=gst_pad_query_caps(eachPad,NULL);
            if(padCaps)
            {
                if(!gst_caps_can_intersect(padCaps,allowedCaps))
                    continue;
            }
        }

        GstPad *result=GhostSinglePad(eachPad,results);

    }
}

GstPad* gstreamBin::GhostSinglePad(GstPad *eachPad, std::vector<GstPad*> &results)
{
    // if it's already connected, bail (this also finds pads already ghosted)
    if(gst_pad_is_linked(eachPad))
        return NULL;

    GstCaps *queryCaps=gst_pad_query_caps(eachPad,NULL);
    GstCaps *srcCaps=gst_pad_get_current_caps(eachPad);

    GST_INFO_OBJECT (m_myBin, "Query caps are %s", gst_caps_to_string(queryCaps));
    GST_INFO_OBJECT (m_myBin, "Current caps are %s", gst_caps_to_string(srcCaps));

    GstPad *ghostPad=gst_ghost_pad_new(NULL,eachPad);

    // ok - the logic here is this ...
    /// we've been called by a demuxer (probably) - current caps are it, the end.
    // so create a CAPS event from that, and send it to the ghost
    gst_pad_send_event(ghostPad,gst_event_new_caps(srcCaps));



    // force a reconfig?
    // if(ghostPad->direction==GST_PAD_SRC)
    // {
    //     GstEvent *reconfig=gst_event_new_reconfigure();
    //     gst_pad_send_event(ghostPad,reconfig);
    // }

    // do this after the reconfig
    gst_element_add_pad(GST_ELEMENT(m_myBin),ghostPad);

    results.push_back(ghostPad);

    return ghostPad;

}


/////////////////////////////////////////

bool gstreamListeningBin::ConnectLate(GstElement *source, GstElement *dest)
{
    // make sure it's not on the list
    for(auto each=m_lateSrcs.begin();each!=m_lateSrcs.end();each++)
    {
        if(std::get<0>(*each)==source && std::get<1>(*each)==dest)
        {
            return true;
        }
    }

    // add it
    m_lateSrcs.push_back(std::tuple<GstElement*,GstElement*>(source,dest));

    // and listen
    g_signal_connect (source, "no-more-pads", G_CALLBACK (staticNoMoreToConnectTo), this);
    g_signal_connect (source, "pad-added", G_CALLBACK (staticPadAdded), this);


    return true;
}


void gstreamListeningBin::padAdded(GstElement*src,GstPad*padAdded)
{
    // walk thru the late bind items, and hoopk things up
    for(auto each=m_lateSrcs.begin();each!=m_lateSrcs.end();each++)
    {
        if(src!=std::get<0>(*each))
        {
            continue;
        }

        // special case - NULL for sink means 'ghost it straight out'

        if(!std::get<1>(*each))
        {
            GhostSinglePad(padAdded,m_ghostPadsSrcs);
        }
        // try to connect 
        else if(ConnectSrcToSink(padAdded,std::get<1>(*each)))
        {
            // assume this is a src into a sink, (probably a multiqueue)
            // so ghost it out
            AddGhostPads(NULL,std::get<1>(*each));
        }
    }
}

void gstreamListeningBin::noMoreToConnectTo(GstElement*)
{
    m_demuxerFinished=true;
    // TODO this should check for multiple demuxers inside me
    gst_element_no_more_pads(m_myBin);
}

bool gstreamListeningBin::ConnectSrcToSink(GstPad*srcPad, GstElement *sinkElement)
{
    GST_INFO_OBJECT (m_myBin, "ConnectSrcToSink pad '%s' to element '%s'", GST_ELEMENT_NAME(srcPad),GST_ELEMENT_NAME(sinkElement));
    // get the caps of the srcPad;
    //GstCaps *srcCaps=gst_pad_query_caps(srcPad,NULL);
    
    // gst_pad_query_caps for splitmuxsrc seems to return static, so always said ANY, which caused chaos
    // need to confirm this works with other demuxes
    GstCaps *srcCaps=gst_pad_get_current_caps(srcPad);
    GST_INFO_OBJECT (m_myBin, "Source caps are %s", gst_caps_to_string(srcCaps));
    bool succeeded=false;
    // get the sinkPads from the sinkElement
    for(auto eachSinkPadIterator=sinkElement->sinkpads;eachSinkPadIterator && !succeeded;eachSinkPadIterator=eachSinkPadIterator->next)
    {
        GstPad *eachSinkPad=GST_PAD(eachSinkPadIterator->data);
        // if it's already linked, walk away
        if(gst_pad_is_linked(eachSinkPad))
        {
            continue;
        }

        // hurrah! will it accept the caps we need?
        if(gst_pad_query_accept_caps(eachSinkPad,srcCaps))
        {
            // try to link them
            if(GST_PAD_LINK_OK==gst_pad_link(srcPad,eachSinkPad))
            {
                // yay!
                succeeded=true;
            }
        }
        else
        {
            GstCaps *sinkCaps=gst_pad_query_caps(eachSinkPad,NULL);
            GST_INFO_OBJECT (m_myBin, "Sink caps for '%s' are %s", GST_ELEMENT_NAME(eachSinkPad), gst_caps_to_string(sinkCaps));
        }
    }
    if(!succeeded)
    {
        // let's request the pad we need
        GstPad *newSinkPad=gst_element_request_pad(sinkElement,gst_pad_template_new("sink_%u",GST_PAD_SINK,GST_PAD_REQUEST,srcCaps),"sink_%u",srcCaps);
        if(newSinkPad)
        {
            if(GST_PAD_LINK_OK==gst_pad_link(srcPad,newSinkPad))
            {
                // yay!
                succeeded=true;
            }
        }
    }

    return succeeded;
}
