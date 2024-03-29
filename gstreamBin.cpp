#include "gstreamBin.h"

#include "myplugins/gstmybin.h"


gstreamBin::gstreamBin(const char *binName, pluginContainer<GstElement> *parent):
    m_parent(parent),
    m_binName(binName)
{
    // create my own gstbin that stores the ptr back to this class, handy for calling up the vtable 
    m_myBin=GST_ELEMENT(g_object_new (GST_TYPE_MYBIN,NULL));

    // preserve my this
    GstMyBin *mybininstance=GST_MYBIN(m_myBin);
    mybininstance->myClassPointer=this;
    gst_element_set_name (mybininstance,binName);

    pluginContainerSetParent(m_myBin);

    // add myself to pipeline
    parent->AddElement(binName,GST_ELEMENT(m_myBin));

}    

gstreamBin::~gstreamBin()
{
    GST_INFO_OBJECT (m_myBin, "killing bin %s",Name());                

    // check
    if(m_requestedPadsGhosted.size())
    {
        // remove request pads
        releaseRequestedPads();
    }



    // then unref the targets of ghosts
    for(auto each=m_ghostPadsSrcs.begin();each!=m_ghostPadsSrcs.end();each++)
    {
        GST_INFO_OBJECT (m_myBin, "releasing ghost '%s' from '%s'",GST_PAD_NAME(*each),GST_ELEMENT_NAME(m_myBin));
        gst_element_remove_pad(m_myBin,GST_PAD(*each));
    }

    for(auto each=m_ghostPadsSinks.begin();each!=m_ghostPadsSinks.end();each++)
    {
        GST_INFO_OBJECT (m_myBin, "releasing ghost '%s' from '%s'",GST_PAD_NAME(*each),GST_ELEMENT_NAME(m_myBin));
        gst_element_remove_pad(m_myBin,GST_PAD(*each));
    }


}



// normally called by an owning class's dtor, so you can unlink from it
void gstreamBin::releaseRequestedPads()
{
    for(auto each=m_padsToBeReleased.begin();each!=m_padsToBeReleased.end();each++)
    {
        GST_INFO_OBJECT (m_myBin, "Asking element '%s' to release pad '%s'",GST_ELEMENT_NAME(each->first),GST_PAD_NAME(each->second));


        GstElementClass *oclass=GST_ELEMENT_GET_CLASS(each->first);
        if (oclass->release_pad)
        {
            if(GST_PAD_PARENT(each->second))
            {
                GST_INFO_OBJECT (m_myBin, "owner is '%s'",GST_ELEMENT_NAME(GST_PAD_PARENT(each->second)));
            }
            else
            {
                GST_ERROR_OBJECT (m_myBin, "owner is NULL");
            }

        }

        releaseSingleRequestedPin(each->first,each->second);


    }    

    m_padsToBeReleased.clear();
}




GstPad *gstreamBin::request_new_pad (GstElement * element,GstPadTemplate * templ,const gchar * name,const GstCaps * caps)
{
    return NULL;
}


bool gstreamBin::release_requested_pad(GstElement*el, GstPad *pad)
{
    // what do we do here !?
    bool ret=false;
    // check it IS a ghostpin
    if(GST_IS_GHOST_PAD(pad))
    {
        // all requested ghosts should be in m_requestedPadsGhosted
        for(auto each=m_requestedPadsGhosted.begin();each!=m_requestedPadsGhosted.end() && !ret;)
        {
            if(*each==pad)
            {
                // found it
                GstPad *target=gst_ghost_pad_get_target (GST_GHOST_PAD(*each));
                // find the thing it's ghosting in padsToBeReleased
                for(auto reqPad=m_padsToBeReleased.begin();reqPad!=m_padsToBeReleased.end() && !ret;)
                {
                    if(reqPad->second==target)
                    {
                        // and let go of them
                        releaseSingleRequestedPin(reqPad->first,reqPad->second);
                        // cull it
                        reqPad=m_padsToBeReleased.erase(reqPad);
                        // now, the ghost ?
                        gst_element_remove_pad(m_myBin,GST_PAD(*each));
                        // cull it
                        each=m_requestedPadsGhosted.erase(each);
                        ret=true;
                    }
                    else
                    {
                        reqPad++;
                    }
                }
                // unref!
                gst_object_unref(target);
            }
            else
            {
                each++;
            }
        }
        if(!ret)
        {
            // there's an edge case with 'handleUnfurnishedRequest' where an encoder is dynamically created and
            // ghosted as a sink pin, but it was ghosted as a result of a request_pad call
            for(auto eachSinkGhost=m_ghostPadsSinks.begin();eachSinkGhost!=m_ghostPadsSinks.end();eachSinkGhost++)
            {
                if(*eachSinkGhost==pad)
                {
                    ret=true;
                    GST_INFO_OBJECT (m_myBin, "Found SinkGhost  '%s' pad from '%s' to release it",GST_PAD_NAME(pad),GST_ELEMENT_NAME(el));                
                    // the dtor will clean up the sink ghost
                }
            }

            if(!ret)
            {
                GST_ERROR_OBJECT (m_myBin, "Could not find  '%s' pad from '%s' to release it",GST_PAD_NAME(pad),GST_ELEMENT_NAME(el));                
            }
        }
    }
    else
    {
        GST_WARNING_OBJECT (m_myBin, "Non Ghost Pad passed  '%s' pad from '%s' to release it",GST_PAD_NAME(pad),GST_ELEMENT_NAME(el));                
    }


    return ret;
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
        if(allowedCaps)
            gst_caps_unref(allowedCaps);
    }
    // same with sources
    if(sourceElement)
    {
        GstCaps *allowedCaps=srccaps?gst_caps_new_full(gst_structure_from_string(srccaps,NULL),NULL):NULL;
        IterateAndGhost(sourceElement->srcpads,m_ghostPadsSrcs,allowedCaps);
        if(allowedCaps)
            gst_caps_unref(allowedCaps);
    }

    return true;
    
}



void gstreamBin::IterateAndGhost(GList *elementPads, std::vector<GstPad*> &results, GstCaps *allowedCaps)
{
    iteratePadsLambda(elementPads,
        [&](GstPad *eachPad){

            if(allowedCaps)
            {
                GstCaps *padCaps=gst_pad_query_caps(eachPad,NULL);
                if(padCaps)
                {
                    bool intersect=gst_caps_can_intersect(padCaps,allowedCaps);
                    gst_caps_unref(padCaps);
                    if(!intersect)
                    {
                        return true;
                    }
                }
            }

            GstPad *result=GhostSinglePad(eachPad,results);
            return true;

        });
    
}

GstPad* gstreamBin::GhostSingleSinkPad(GstElement *sink)
{
    GstPad *pad=(GstPad*)sink->sinkpads->data;
    return GhostSingleSinkPad(pad);
}

GstPad* gstreamBin::GhostSingleRequestedSinkPad(GstElement *sink)
{
    GstPad *pad=(GstPad*)sink->sinkpads->data;
    return GhostSingleRequestPad(pad);
}


GstPad* gstreamBin::GhostSinglePad(GstPad *eachPad, std::vector<GstPad*> &results)
{
    // if it's already connected, bail (this also finds pads already ghosted)
    if(gst_pad_is_linked(eachPad))
        return NULL;

    //GstCaps *queryCaps=gst_pad_query_caps(eachPad,NULL);
    GstCaps *srcCaps=gst_pad_get_current_caps(eachPad);

    //GST_INFO_OBJECT (m_myBin, "Query caps are %s", gst_caps_to_string(queryCaps));
    GST_INFO_OBJECT (m_myBin, "Current caps are %s", gst_caps_to_string(srcCaps));

    //gst_caps_unref(queryCaps);
    if(srcCaps)
        gst_caps_unref(srcCaps);

    GstPadDirection dir=gst_pad_get_direction(eachPad);

    GST_INFO_OBJECT (m_myBin, "Pad to be ghosted is %s", dir==GST_PAD_SRC?"Source":dir==GST_PAD_SINK?"Sink":"unknown");


    GstPad *ghostPad=gst_ghost_pad_new(NULL,eachPad);
    //and tell it who owns it

    // ok - the logic here is this ...
    /// we've been called by a demuxer (probably) - current caps are it, the end.
    // so create a CAPS event from that, and send it to the ghost
    // TODO: enabling this returns 'sending evenbt wrong direction"
    // gst_pad_send_event(ghostPad,gst_event_new_caps(srcCaps));
    gst_pad_mark_reconfigure (ghostPad);



    // force a reconfig?
    // if(ghostPad->direction==GST_PAD_SRC)
    // {
    //     GstEvent *reconfig=gst_event_new_reconfigure();
    //     gst_pad_send_event(ghostPad,reconfig);
    // }

    // activate the new pad
    if(!gst_pad_set_active(ghostPad, TRUE))
    {
        GST_ERROR_OBJECT (m_myBin, "gst_pad_set_active %s failed", GST_ELEMENT_NAME(ghostPad) );
    }

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
    GST_INFO_OBJECT (m_myBin, "padAdded %s:%s", GST_ELEMENT_NAME(src),GST_ELEMENT_NAME(padAdded));    

    // walk thru the late bind items, and hook things up
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
            gst_caps_unref(sinkCaps);
        }
    }

    if(!succeeded)
    {
        // let's request the pad we need
        GstPadTemplate *padt=gst_pad_template_new("sink_%u",GST_PAD_SINK,GST_PAD_REQUEST,srcCaps);
        GstPad *newSinkPad=gst_element_request_pad(sinkElement,padt,"sink_%u",srcCaps);
        gst_object_unref(padt);
        if(newSinkPad)
        {
            addPadToBeReleased(sinkElement,newSinkPad);
            if(GST_PAD_LINK_OK==gst_pad_link(srcPad,newSinkPad))
            {
                // yay!
                succeeded=true;
            }
        }
    }

    if(srcCaps)
    {
        gst_caps_unref(srcCaps);
    }

    return succeeded;
}
