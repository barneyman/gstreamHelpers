#ifndef _GSTREAMPIPELINE_H
#define _GSTREAMPIPELINE_H

#include <gst/gst.h>
#include <vector>
#include <string>

#define _USE_GST_NET_LIBS
#ifdef _USE_GST_NET_LIBS
#include <gst/net/gstnet.h>
#endif

#include "json/json.hpp"


#define MAX_GATHER      (20*GST_SECOND)

#define _DO_SEEK_MSG    "DoSeek"


template <class parentType>
class pluginContainer
{
protected:

    void pluginContainerSetParent(parentType* theParent)
    {
        m_parent=theParent;   
    }
    parentType *m_parent;

public:

    int AddPlugin(const char *plugin, const char *name=NULL, nlohmann::json *settings=NULL)
    {
        if(!name)
            name=plugin;

        GST_INFO_OBJECT (m_parent, "Adding plugin '%s' (as %s)... ",plugin, name);

        // make sure it's not already there
        if(FindNamedPlugin(name, false))
        {
            GST_ERROR_OBJECT(m_parent, "plugin %s already present", plugin);
            return 1;
        }

        // then settings
        std::string overridePlugin=plugin;
        if(settings && settings->contains(name))
        {
            if(settings->at(name).contains("use"))
            {
                overridePlugin=settings->at(name).at("use").get<std::string>();
                GST_INFO_OBJECT (m_parent, "using '%s' instead of requested '%s'", overridePlugin.c_str(),plugin);
            }
        }


        // create it
        GstElement* newplugin = gst_element_factory_make (overridePlugin.c_str(), name);

        if(!newplugin)
        {
            GST_ERROR_OBJECT(m_parent, "failed to make plugin '%s'", overridePlugin.c_str());
            return 2;
        }

        GST_INFO_OBJECT (m_parent, "OK");

        return AddElement(name,newplugin,settings);

    }    


    GstElement* FindNamedPlugin(const char *name, bool complainOnMiss=true)
    {
        if(!name)
            return NULL;

        for(auto each=m_allPlugins.begin();each!=m_allPlugins.end();each++)
        {
            if(each->first==name)
            {
                return each->second;
            }
        }
        // default is to complain, addplugin uses this function to make sure it's not repeating itself, so error is actually success
        if(complainOnMiss)
        {
            GST_ERROR_OBJECT (m_parent, "Failed to find plugin %s\n",name);
        }

        return NULL;
    }


    int AddElement(const char* name, GstElement *newplugin, nlohmann::json *settings=NULL)
    {

        m_allPlugins.push_back(std::pair<std::string,GstElement*>(std::string(name),newplugin));

        // and add it 
        gst_bin_add(GST_BIN (m_parent),newplugin);

        // then settings
        if(settings)
        {
            if(settings->contains(name))
            {
                //if(settings[name].contains("properties"))
                if(settings->at(name).contains("properties"))
                {
                    //pluginProperties=
                    auto pluginProperties=settings->at(name).at("properties").get<nlohmann::json>(); //settings[name]["properties"].get<nlohmann::json>();
                    //auto pluginProperties=settings->at(name).get<nlohmann::json>();

                    // iterate thru them and set them
                    for(auto each=pluginProperties.begin();each!=pluginProperties.end();each++)
                    {
                        if(each->contains("propertyName") && each->contains("propertyType") && each->contains("propertyValue"))
                        {
                            std::string propName=(*each)["propertyName"].get<std::string>();

                            // check for it
                            if(!g_object_class_find_property(G_OBJECT_GET_CLASS(newplugin),propName.c_str()))
                            {
                                GST_ERROR_OBJECT (m_parent, "Plugin '%s' has no property '%s'",GST_ELEMENT_NAME(newplugin), propName.c_str());
                                continue;

                            }

                            // https://code.woboq.org/gtk/include/glib-2.0/gobject/gtype.h.html
                            switch((*each)["propertyType"].get<int>())
                            {
                                case G_TYPE_BOOLEAN: // 5<<2
                                    GST_INFO_OBJECT (m_parent, "Setting BOOL property '%s' with value '%s'",propName.c_str(), (*each)["propertyValue"].get<bool>()?"TRUE":"FALSE");
                                    g_object_set ((newplugin), (gchar*)propName.c_str(), (*each)["propertyValue"].get<bool>()?true:false, NULL);
                                    break;

                                case G_TYPE_INT: // 6<<2
                                    GST_INFO_OBJECT (m_parent, "Setting INT property '%s' with value %d",propName.c_str(), (*each)["propertyValue"].get<int>());
                                    g_object_set ((newplugin), (gchar*)propName.c_str(), (*each)["propertyValue"].get<int>(), NULL);
                                    break;

                                case G_TYPE_LONG: // 8<<2
                                    GST_INFO_OBJECT (m_parent, "Setting LONG property '%s' with value %ld",propName.c_str(), (*each)["propertyValue"].get<long>());
                                    g_object_set ((newplugin), (gchar*)propName.c_str(), (*each)["propertyValue"].get<long>(), NULL);
                                    break;

                                case G_TYPE_STRING: // 16<<2
                                    GST_INFO_OBJECT (m_parent, "Setting STRING property '%s' with value %s",propName.c_str(), (*each)["propertyValue"].get<std::string>().c_str());
                                    g_object_set ((newplugin), (gchar*)propName.c_str(), (*each)["propertyValue"].get<std::string>().c_str(), NULL);
                                    break;

                                case G_TYPE_DOUBLE: // 15<<2
                                    GST_INFO_OBJECT (m_parent, "Setting DOUBLE property '%s' with value %lf",propName.c_str(), (*each)["propertyValue"].get<double>());
                                    g_object_set ((newplugin), (gchar*)propName.c_str(), (*each)["propertyValue"].get<double>(), NULL);
                                    break;
                                
                                case -1: // special case - GST caps
                                    GST_INFO_OBJECT (m_parent, "Setting GstCaps property '%s' with value %s",propName.c_str(), (*each)["propertyValue"].get<std::string>().c_str());
                                    g_object_set ((newplugin), (gchar*)propName.c_str(), gst_caps_new_full(gst_structure_from_string((*each)["propertyValue"].get<std::string>().c_str(),NULL),NULL), NULL);
                                    break;

                                case -2: // special case GstStructure
                                    GST_INFO_OBJECT (m_parent, "Setting GstStructure property '%s' with value %s",propName.c_str(), (*each)["propertyValue"].get<std::string>().c_str());
                                    g_object_set ((newplugin), (gchar*)propName.c_str(), gst_structure_from_string ((*each)["propertyValue"].get<std::string>().c_str(),NULL), NULL);

                                    break;
                                default:
                                    GST_ERROR_OBJECT (m_parent, "unsupported type %d\n", (*each)["propertyType"].get<int>());
                                    break;
                            }

                        }
                        else
                        {
                            GST_ERROR_OBJECT (m_parent, "malformed property item %s\n", each->dump().c_str());
                        }

                    }
                }
                else
                {
                    GST_WARNING_OBJECT (m_parent, "No json properties for Plugin '%s'",GST_ELEMENT_NAME(newplugin));

                }

            }
            else
            {
                (*settings)[name]=nlohmann::json::array();
            }

        }

        return 0;

    }    

protected:

    std::vector<std::pair<std::string,GstElement*> > m_allPlugins;


};



template <class pipelineType>
class gstreamPipelineBase : public pluginContainer<pipelineType>
{

protected:

    gstreamPipelineBase():m_bus(NULL),m_pipeline(NULL),m_pipelineState(GST_STATE_NULL ),
        m_eosSeen(false), m_blockedPins(0), m_seekLateEvent(NULL), m_seekLateOn(NULL),
        m_exitOnBool(NULL),m_networkClock(NULL)
    {
        // assume gst_init is called by derived class
    }

    void initialiseGstreamer()
    {
        // spin up gstreamer
        if(!m_initCount)
        {
            gst_init (NULL, NULL);    
        }

        g_atomic_int_inc(&m_initCount);

    }

public:

    gstreamPipelineBase(const char *pipelineName):m_bus(NULL),m_pipeline(NULL),m_pipelineState(GST_STATE_NULL ),
        m_eosSeen(false), m_blockedPins(0), m_seekLateEvent(NULL), m_seekLateOn(NULL),
        m_exitOnBool(NULL),m_networkClock(NULL)
    {
        initialiseGstreamer();

        // create a pipeline
        m_pipeline=gst_pipeline_new (pipelineName);
        pluginContainer<pipelineType>::pluginContainerSetParent(m_pipeline);

        // get to the bus
        m_bus = gst_element_get_bus (m_pipeline);

    }

    ~gstreamPipelineBase()
    {
        if(m_bus)
        {
            gst_object_unref (m_bus);
            m_bus=NULL;
        }

        if(m_pipeline)  
        {
            gst_element_set_state ((GstElement*)m_pipeline, GST_STATE_NULL);
            gst_object_unref (m_pipeline);
            m_pipeline=NULL;
        }

        if(m_networkClock)
        {
            gst_object_unref(m_networkClock);
            m_networkClock=NULL;
        }


        if(g_atomic_int_dec_and_test(&m_initCount))
        {
            gst_deinit();
        }


    }


  

    int ConnectPipeline(const char*first, const char *last, const char*mid=NULL, bool mustConnectAll=false)
    {
        // find them
        GstElement *source=pluginContainer<pipelineType>::FindNamedPlugin(first);
        GstElement *sink=pluginContainer<pipelineType>::FindNamedPlugin(last);
        GstElement *middle=mid?pluginContainer<pipelineType>::FindNamedPlugin(mid):NULL;

        GST_INFO_OBJECT (m_pipeline, "Connecting %s with %s", first,last);
        if(mid)
        {
            GST_INFO_OBJECT (m_pipeline, "   via %s", mid);
        }

        if(!source || !sink || (mid && !middle))
        {
            GST_ERROR_OBJECT(m_pipeline, "No Elements to be linked.");
            return 1;
        }

        // join them
        if(middle)
        {
            if (LinkAllSourcePadsToDestAny (source, middle) != true)     
            {
                GST_ERROR_OBJECT(m_pipeline, "Elements %s & %s could not be linked.",first, mid);
                return 2;
            }
            else if (LinkAllSourcePadsToDestAny (middle, sink) != true)     
            {
                GST_ERROR_OBJECT(m_pipeline, "Elements %s & %s could not be linked.", mid, last);
                return 3;
            }

        }
        else if (LinkAllSourcePadsToDestAny (source, sink) != true) 
        {
            GST_ERROR_OBJECT(m_pipeline, "Elements %s & %s could not be linked.",first, last);
            return 4;
        }

        gst_element_sync_state_with_parent(source);
        if(middle)
        {
            gst_element_sync_state_with_parent(source);
        }
        gst_element_sync_state_with_parent(sink);

        return 0;
    }

    int ConnectPipeline(std::vector<std::string> &inOrderList, bool mustConnectAll=false)
    {
        // sanity check
        if(inOrderList.size()<2)
        {
            GST_ERROR_OBJECT (m_pipeline, "Vector has too few members\n");
            return 1;
        }

        // just walk thru them
        for(auto firstOne=inOrderList.begin(), nextOne=firstOne+1;nextOne!=inOrderList.end();nextOne++,firstOne++)
        {
            int res=ConnectPipeline(firstOne->c_str(),nextOne->c_str(),NULL, mustConnectAll);
            if(res!=0)
            {
                return res;
            }
        }

        return 0;
    }

    int ConnectPipeline(bool mustConnectAll=false)
    {
        // build it

        std::vector<std::string> all;
        for(auto each=pluginContainer<pipelineType>::m_allPlugins.begin();each!=pluginContainer<pipelineType>::m_allPlugins.end();each++)   
        {
            all.push_back(each->first);
        }

        return ConnectPipeline(all, mustConnectAll);
    }



    // some pads don't turn up until the media is flowing (ie muxes)
    int ConnectPipelineLate(const char*src, const char*dest, bool blockSrcPads=false)
    {
        // have we already listened for this on this source?
        bool signalListen=true;
        GstElement *srcElement=pluginContainer<pipelineType>::FindNamedPlugin(src);
        GstElement *destElement=pluginContainer<pipelineType>::FindNamedPlugin(dest);

        return ConnectPipelineLate(srcElement,destElement,blockSrcPads);

    }    

    int ConnectPipelineLate(GstElement*srcElement, GstElement*destElement, bool blockSrcPads=false)
    {
        // have we already listened for this on this source?
        bool signalListen=true;
        for(auto each=m_sometimesConnects.begin();each!=m_sometimesConnects.end();each++)
        {
            if(std::get<0>(*each)==srcElement)
            {
                signalListen=false;
                break;
            }
        }

        // listens for a pad-added signal
        if(signalListen)
        {
            g_signal_connect (srcElement, "pad-added", G_CALLBACK (staticNewPadToConnectTo), this);
            g_signal_connect (srcElement, "no-more-pads", G_CALLBACK (staticNoMoreToConnectTo), this);
        }            

        // add this relationship
        m_sometimesConnects.push_back(std::make_tuple(srcElement,destElement,blockSrcPads));

        return m_sometimesConnects.size();

    }

    bool SeekOnMuxLate(unsigned startSeconds,unsigned endSeconds, const char* seekOn)
    {
        return SeekOnElementLate(startSeconds,endSeconds,pluginContainer<pipelineType>::FindNamedPlugin(seekOn));
    }

    bool SeekOnElementLateSeconds(unsigned startSeconds,unsigned endSeconds, GstElement *onElement)
    {
        return SeekOnElementLate(startSeconds*GST_SECOND,endSeconds*GST_SECOND,onElement);
    }


    bool SeekOnElementLate(GstClockTime startAt,GstClockTime endAt, GstElement *onElement)
    {
        if(m_seekLateEvent)
            return false;

        m_seekLateOn=onElement;//pluginContainer<pipelineType>::FindNamedPlugin(seekOn);

        if(!m_seekLateOn)
            return false;

        m_seekLateEvent=gst_event_new_seek(
            1.0,
            GST_FORMAT_TIME,
            // flush is needed to get the 'padblocked' probe call
            (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
            GST_SEEK_TYPE_SET,
            startAt,
            GST_SEEK_TYPE_SET,
            endAt

        );

        return true;

    }

    bool SeekRun(bool doReady=true)
    {
        if(doReady)
        {
            if(!Ready())
            {
                return false;
            }
        }

        if(!Pause())
            return false;

//        // play is performed when the demux is unblocked
//        if(!Play())
//            return false;

        // then wait for us to play out
        internalRun(0);

        return true;

    }



    bool Run(unsigned long timeoutSeconds=0, bool doPreroll=true, bool doReady=true)
    {
        if(m_seekLateEvent)
            return SeekRun(doReady);

        GST_INFO_OBJECT (m_pipeline, "Running for %lu seconds",timeoutSeconds);

        DumpGraph("Ready");
        
        if(doReady)
        {
            if(!Ready())
            {
                DumpGraph("Ready FAILED");
                return false;
            }
        }

        if(doPreroll)
        {
            GST_INFO_OBJECT (m_pipeline, "PREROLL request");
            if(!PreRoll())
            {
                DumpGraph("PREROLL FAILED");
                return false;
            }
            GST_INFO_OBJECT (m_pipeline, "PREROLL");
            DumpGraph("Paused");
        }


        GST_INFO_OBJECT (m_pipeline, "PLAY request");
        if(!Play())
        {
            return false;
        }

//        IterateThruAllChildren();

        GST_INFO_OBJECT (m_pipeline, "PLAY");
        DumpGraph("Playing");

        internalRun(timeoutSeconds);

        return true;
    }


    bool PreRoll() { return ChangeStateAndWait(GST_STATE_PAUSED); }
    bool Pause() { return ChangeStateAndWait(GST_STATE_PAUSED); }
    bool Ready() { return ChangeStateAndWait(GST_STATE_READY); }
    bool Play() { return ChangeStateAndWait(GST_STATE_PLAYING); }
    bool Stop() { return ChangeStateAndWait(GST_STATE_NULL); }

    GstState CurrentPipelineState() 
    {
        GstState currentState=GST_STATE_VOID_PENDING;
        gst_element_get_state((GstElement*)m_pipeline, &currentState, NULL, 2*GST_SECOND);

        return (currentState);
    }

    bool AwaitState(GstState newState, GstClockTime waitFor=2*GST_SECOND)
    {
        GstState currentState=GST_STATE_VOID_PENDING,pendingState;
        while(currentState!=newState)
        {
            GstStateChangeReturn ret=gst_element_get_state((GstElement*)m_pipeline, &currentState, &pendingState, 2*GST_SECOND);

            switch(ret)
            {
                case GST_STATE_CHANGE_SUCCESS:
                    // worked
                    break;
                case GST_STATE_CHANGE_FAILURE:
                    GST_ERROR_OBJECT(m_pipeline, "Unable to get pipeline state.");
                    DumpGraph("GetState Failed bailing");
                    return false;
                case GST_STATE_CHANGE_ASYNC:
                    PumpMessages();
                    DumpGraph("ASYNCwaiting");
                    break;
                default:
                    GST_DEBUG_OBJECT(m_pipeline, "gst_element_get_state returned %d\n", ret);
                    break;
            }
        }

        return true;
    }


    bool ChangeStateAndWait(GstState newState)
    {
        GstStateChangeReturn ret = gst_element_set_state ((GstElement*)m_pipeline, newState);

        if (ret == GST_STATE_CHANGE_FAILURE) 
        {
            GST_ERROR_OBJECT(m_pipeline, "Unable to set the pipeline to '%s' state.", gst_element_state_get_name(newState));
            return false;
        }

        if(ret==GST_STATE_CHANGE_SUCCESS || (newState==GST_STATE_PAUSED && ret==GST_STATE_CHANGE_NO_PREROLL))
        {
            return true;
        }

        GST_INFO_OBJECT (m_pipeline, "gst_element_set_state returned %d",ret);

        GstState currentState=GST_STATE_VOID_PENDING,pendingState;
        bool stateChangeSuccess=false;
        // i changed this so seekrun will work ... in the background (when seeking) i issue a play call
        // while we are still pausing (and ergo in this loop)
        //while(currentState!=newState)
        while(!stateChangeSuccess)
        {
            ret=gst_element_get_state((GstElement*)m_pipeline, &currentState, &pendingState, 2*GST_SECOND);

            switch(ret)
            {
                case GST_STATE_CHANGE_SUCCESS:
                    stateChangeSuccess=true;
                    // worked
                    break;
                case GST_STATE_CHANGE_FAILURE:
                    GST_ERROR_OBJECT(m_pipeline, "Unable to get pipeline state.");
                    DumpGraph("GetState Failed");
                    return false;
                case GST_STATE_CHANGE_ASYNC:
                    PumpMessages();
                    DumpGraph("pumping");
                    break;
                default:
                    GST_DEBUG_OBJECT(m_pipeline, "gst_element_get_state returned %d\n", ret);
                    break;
            }

        }
        
        return stateChangeSuccess;
    }

    static void eachChild(const GValue * item, gpointer user_data)
    {
/*        
        GstElement *child=(GstElement*)g_value_get_object(item);
        GstClockTime baseTime=gst_element_get_base_time(child);
        GstClockTime startTime=gst_element_get_start_time(child);

        g_print("child %20s basetime: %" GST_TIME_FORMAT " starttime %" GST_TIME_FORMAT "\n",gst_element_get_name(child),GST_TIME_ARGS(baseTime),GST_TIME_ARGS(startTime));
*/
    }

    void IterateThruAllChildren()
    {
        GstIterator *allKids=gst_bin_iterate_recurse(GST_BIN(m_pipeline));

        if(!allKids)
        {
            return;
        }

        gst_iterator_foreach(allKids,eachChild,this);

    }


    void DumpGraph(const char *toFile)
    {
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN (m_pipeline), GST_DEBUG_GRAPH_SHOW_VERBOSE, toFile);
        //GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN (m_pipeline), GST_DEBUG_GRAPH_SHOW_ALL, toFile);
    }

    static void staticNewPadToConnectTo(GstElement *element,GstPad *pad,gpointer data)
    {
        ((gstreamPipelineBase*)data)->newPadToConnectTo(element,pad);
    }

    static void staticNoMoreToConnectTo(GstElement *element,gpointer data)
    {
        ((gstreamPipelineBase*)data)->noMoreToConnectTo(element);
    }


    static GstPadProbeReturn staticPadBlocked (GstPad *pad,GstPadProbeInfo *info, gpointer data)
    {
        return ((gstreamPipelineBase*)data)->padBlocked(pad,info);
    }

    GstPadProbeReturn padBlocked (GstPad *pad,GstPadProbeInfo *info)
    {
        if(!g_atomic_int_get(&m_blockedPins))
        {
            GST_INFO_OBJECT (m_pipeline, "removing BLOCK from %s",GST_ELEMENT_NAME(pad));
            return GST_PAD_PROBE_REMOVE;
        }

        // otherwise 
        if (g_atomic_int_dec_and_test (&m_blockedPins))
        {
            gst_bus_post (m_bus, gst_message_new_application (GST_OBJECT_CAST (m_pipeline),gst_structure_new_empty (_DO_SEEK_MSG)));            
        }

        return GST_PAD_PROBE_OK;

    }

    virtual void noMoreToConnectTo(GstElement *element)
    {
        GST_INFO_OBJECT (m_pipeline, "no more late pad from %s",GST_ELEMENT_NAME(element));
    }


    bool ConnectSrcToSink(GstPad*srcPad, GstElement *sinkElement)
    {
        // get the caps of the srcPad;
        GstCaps *queryCaps=gst_pad_query_caps(srcPad,NULL);
        GST_INFO_OBJECT (m_pipeline, "Query caps are %s", gst_caps_to_string(queryCaps));
        gst_caps_unref(queryCaps);

        // gst_pad_query_caps for splitmuxsrc seems to return static, so always said ANY, which caused chaos
        // need to confirm this works with other demuxes
        GstCaps *srcCaps=gst_pad_get_current_caps(srcPad);
        GST_INFO_OBJECT (m_pipeline, "Current caps are %s", gst_caps_to_string(srcCaps));
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
                GST_INFO_OBJECT (m_pipeline, "Sink caps for '%s' are %s", GST_ELEMENT_NAME(eachSinkPad), gst_caps_to_string(sinkCaps));
                gst_caps_unref(sinkCaps);
            }
        }
        if(!succeeded)
        {
            // let's request the pad we need - null caps are fatal for gst_pad_template_new, so just, don't!
            if(srcCaps)
            {
                GstPad *newSinkPad=gst_element_request_pad(sinkElement,gst_pad_template_new("src_%u",GST_PAD_SINK,GST_PAD_REQUEST,srcCaps),NULL,NULL);
                if(newSinkPad)
                {
                    if(GST_PAD_LINK_OK==gst_pad_link(srcPad,newSinkPad))
                    {
                        // yay!
                        succeeded=true;
                    }
                }
            }
        }

        if(srcCaps)
            gst_caps_unref(srcCaps);

        return succeeded;
    }

    long BlockPadForSeek(GstPad *pad)
    {
        long ret=gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM, (GstPadProbeCallback) staticPadBlocked, this, NULL)?true:false;
        g_atomic_int_inc(&m_blockedPins);
        return ret;
        
    }

    virtual void newPadToConnectTo(GstElement *element,GstPad *pad)
    {
        GST_INFO_OBJECT (m_pipeline, "late pad arrive %s:%s",GST_ELEMENT_NAME(element),GST_ELEMENT_NAME(pad));
        // walk the vector, looking for the element
        for(auto each=m_sometimesConnects.begin();each!=m_sometimesConnects.end();each++)
        {
            if(std::get<0>(*each)==element)
            {
                // force a connect
                //if(LinkAllSourcePadsToDestAny(std::get<0>(*each),std::get<1>(*each),true,pad))
                if(ConnectSrcToSink(pad,std::get<1>(*each)))
                {
                    gst_element_sync_state_with_parent(std::get<1>(*each));

                    // if we are blocking the pad ...
                    if(std::get<2>(*each))
                    {   
                        GST_INFO_OBJECT (m_pipeline, "adding BLOCK to %s:%s",GST_ELEMENT_NAME(element),GST_ELEMENT_NAME(pad));
                        // https://gstreamer.freedesktop.org/documentation/application-development/advanced/pipeline-manipulation.html?gi-language=c#play-a-section-of-a-media-file
                        if(!BlockPadForSeek(pad))
                        {
                            GST_ERROR_OBJECT (m_pipeline, "BlockPadForSeek failed %s:%s",GST_ELEMENT_NAME(element),GST_ELEMENT_NAME(pad));
                        }
                    }

                    break;
                }
                else
                {
                    GST_WARNING_OBJECT (m_pipeline, "Failed to link late pad from %s:%s to %s",GST_ELEMENT_NAME(element),GST_ELEMENT_NAME(pad),GST_ELEMENT_NAME(std::get<1>(*each)));
                }
                
                // and round again in case there are multiple joins
            }
        }
        DumpGraph("pad-added");

    }

    void setExitVar(volatile bool *sendEOSonTrue)
    {
        m_exitOnBool=sendEOSonTrue;
    }

protected:

    volatile bool *m_exitOnBool;

    void internalRun(unsigned long timeoutSeconds=0)
    {
        time_t startTime=time(NULL);
        bool eosSent=false;

        int graphCount=0;

        while(!m_eosSeen)
        {
            if(timeoutSeconds || m_exitOnBool)
            {

                if(!eosSent)
                {
                    bool sendEOS=false;

                    if(m_exitOnBool && *m_exitOnBool)
                        sendEOS=true;
                    else if (timeoutSeconds && (time(NULL)-startTime)>=timeoutSeconds)
                        sendEOS=true;
                    
                    if(sendEOS)
                    {
                        GstIterator *sources=gst_bin_iterate_sources (GST_BIN(m_pipeline));
                        if(sources)
                        {
                            GValue item = G_VALUE_INIT;
                            while (gst_iterator_next (sources, &item)==GST_ITERATOR_OK) 
                            {
                                GST_INFO_OBJECT (m_pipeline, "Sending EOS to '%s' ... ",gst_element_get_name((GstElement*)g_value_get_object(&item)));

                                bool result=gst_element_send_event((GstElement*)g_value_get_object(&item),gst_event_new_eos());

                                if(!result)
                                    GST_ERROR_OBJECT (m_pipeline, " Sending EOS FAILED");


                                g_value_reset(&item);
                            }
                            eosSent=true;

                            gst_iterator_free(sources);
                        }
                        
                    }

                }
            }

            PumpMessages();

        }

        GST_INFO_OBJECT (m_pipeline, "Exiting pump loop");

        PreRoll();
        Ready();
        Stop();        
    }

    GstClockTime GetRunningTime()
    {
        // TODO check state of pipeline
        GstClockTime baseTime=gst_element_get_base_time(GST_ELEMENT(m_pipeline));        
        GstClockTime currentTime=gst_clock_get_time(gst_pipeline_get_clock(GST_PIPELINE(m_pipeline)));

        return currentTime-baseTime;
    }


// Nothing connects - false
// mustConnectAll=true && All sources connect - true, else false
// mustConnectAll=false && at least one connects - true

    bool LinkAllSourcePadsToDestAny(GstElement *sourceElement, GstElement *destElement, bool mustConnectAll=false, GstPad *srcPad=NULL)
    {
        // gst_element_link succeeds early, and doesn't connect all source pins, so do the heavy lifting here
        // get the list of source pins

        unsigned numberConnected=0;

        for(GList *sourcePads=sourceElement->srcpads;sourcePads;sourcePads=sourcePads->next)
        {
            GstPad *eachSourcePad=(GstPad *)sourcePads->data;

            // if we've been asked to link a specific pad, and this isn't it, next ...
            if(srcPad && (srcPad!=eachSourcePad))
            {
                continue;
            }

            // already linked, ignore?
            if(!gst_pad_is_linked (eachSourcePad))
            {
                // iterate thru each dest pad
                bool padConnected=false;

#define _LINK_TO_ANY_PAD_ON_DEST
#ifdef _LINK_TO_ANY_PAD_ON_DEST
                GST_INFO_OBJECT (m_pipeline, "   linking '%s:%s' with '%s:NULL'", GST_ELEMENT_NAME(sourceElement),GST_ELEMENT_NAME(eachSourcePad), GST_ELEMENT_NAME(destElement));//, GST_ELEMENT_NAME(eachDestPad));

                // not specifying a dest pad invokes 'request' 
                if(gst_element_link_pads(sourceElement,gst_pad_get_name(eachSourcePad),destElement,NULL))
                {
                    GST_INFO_OBJECT (m_pipeline, "succeeded");
                    padConnected=true;
                    numberConnected++;
                    continue;
                }
                else
                {
                    GST_INFO_OBJECT (m_pipeline, "failed");
                }
                        
#else // _LINK_TO_ANY_PAD_ON_DEST
// this needs to request pads - matroskamux is an example!


                // get the list of sinks on the dest
                for(GList *sinkIterator=destElement->sinkpads;sinkIterator;sinkIterator=sinkIterator->next)
                {
                    GstPad *eachDestPad=GST_PAD(sinkIterator->data);

                    GST_INFO_OBJECT (m_pipeline, "   linking '%s:%s' with '%s:%s'\n", GST_ELEMENT_NAME(sourceElement),GST_ELEMENT_NAME(eachSourcePad), GST_ELEMENT_NAME(destElement), GST_ELEMENT_NAME(eachDestPad));

                    if(gst_pad_is_linked (eachDestPad))
                    {
                        GST_INFO_OBJECT (m_pipeline, " destpad already connected\n");
                        continue;
                    }


                    if(gst_element_link_pads(sourceElement,gst_pad_get_name(eachSourcePad),destElement,gst_pad_get_name(eachDestPad)))
                    {
                        GST_INFO_OBJECT (m_pipeline, "succeeded\n");
                        padConnected=true;
                        numberConnected++;
                        break;
                    }
                    else
                    {
                        GST_INFO_OBJECT (m_pipeline, "failed\n");
                    }
                }

#endif // _LINK_TO_ANY_PAD_ON_DEST

                if(!padConnected)
                {
                    GST_INFO_OBJECT (m_pipeline, mustConnectAll?"Element - Element FAILED":"fail ignored");

                    if(mustConnectAll)
                    {
                        return false;
                    }
                    else            
                    {
#ifdef _LINK_TO_ANY_PAD_ON_DEST
                        //return numberConnected?true:false;
                        continue;
#else
#endif                        
                    }
                }
            }
            else
            {
                GST_INFO_OBJECT (m_pipeline, "source already linked to something");
                numberConnected++;
            }

        }

        bool ret=numberConnected?true:false;
        return ret;
    }

    virtual void CrackMessage(GstMessage *msg)
    {
            // https://gstreamer.freedesktop.org/documentation/gstreamer/gstmessage.html?gi-language=c#GstMessageType
            switch (GST_MESSAGE_TYPE (msg))
            {
                case GST_MESSAGE_DURATION_CHANGED :
                    genericMessageHandler(msg,"DurationChanged");
                    break;
                case GST_MESSAGE_STREAM_STATUS : //8192
                    genericMessageHandler(msg,"StreamStatus");
                    break;
                case GST_MESSAGE_NEW_CLOCK :
                    genericMessageHandler(msg,"NewClock");
                    break;
                case GST_MESSAGE_LATENCY :
                    genericMessageHandler(msg,"Latency");
                    break;
                case GST_MESSAGE_STREAM_START :
                    genericMessageHandler(msg,"StreamStart");
                    break;
                case GST_MESSAGE_ASYNC_DONE :
                    genericMessageHandler(msg,"AsyncDone");
                    break;
                case GST_MESSAGE_TAG:
                    genericMessageHandler(msg,"Tag");
                    break;
                case GST_MESSAGE_RESET_TIME :
                    genericMessageHandler(msg,"ResetTime");
                    break;
                case GST_MESSAGE_STATE_CHANGED: // 64
                    stateChangeMessageHandler(msg);
                    break;

                case GST_MESSAGE_ELEMENT:
                    elementMessageHandler(msg);
                    break;

                // i have to understand segments better
                case GST_MESSAGE_SEGMENT_DONE:
                    genericMessageHandler(msg,"Segment Done");
                    //m_eosSeen=true;
                    //break;
                case GST_MESSAGE_EOS:
                    // flag and out
                    eosMessageHandler(msg);
                    break;

                case GST_MESSAGE_BUFFERING:
                    bufferMessageHandler(msg);
                    break;

                case GST_MESSAGE_ERROR:
                    errorMessageHandler(msg);
                    break;

                case GST_MESSAGE_WARNING:
                    genericMessageHandler(msg,"Warning");
                    break;

                case GST_MESSAGE_QOS:
                    qosMessageHandler(msg);
                    break;


                case GST_MESSAGE_APPLICATION:
                    if (gst_message_has_name (msg, _DO_SEEK_MSG))
                    {
                        GST_INFO_OBJECT (m_pipeline, "Sending seeklate event from msgapp");
                        // do a seek
                        gst_element_send_event(m_seekLateOn,m_seekLateEvent);
                        m_seekLateEvent=NULL;

                        // and roll!!
                        Play();

                    }
                    genericMessageHandler(msg,"Application");
                    break; 

                default:

                    GST_INFO_OBJECT (m_pipeline, "UNKNOWN message %d", GST_MESSAGE_TYPE (msg));

                    break;
            }
    }

    int PumpMessages(unsigned long timeoutMS=200)
    {

        GstMessage* msg =NULL;

        while(msg=gst_bus_timed_pop (m_bus, timeoutMS*GST_MSECOND))
        {
            CrackMessage(msg);

            // finished, unref
            gst_message_unref(msg);
        }

        return 1;
    }    

    // start a net clock up
    // https://archive.fosdem.org/2016/schedule/event/synchronised_gstreamer/attachments/slides/889/export/events/attachments/synchronised_gstreamer/slides/889/synchronised_multidevice_media_playback_with_GStreamer.pdf
    bool ProvideNetworkClock(unsigned port, GstClock *theClock)
    {
        if(m_networkClock)
            return false;

        m_networkClock=gst_net_time_provider_new(theClock, NULL, port);

        return m_networkClock?true:false;
    }

    bool UseNetworkClock(const char *host, unsigned port)
    {
        GstClock *netClock=gst_net_client_clock_new("networkClock", host, port, 0);

        return netClock?true:false;
    }

protected:

    // virtual message handlers
    virtual void eosMessageHandler(GstMessage*msg)
    {
        genericMessageHandler(msg,"EOS");
        // just check!
        if((GstElement*)(msg->src)==(GstElement*)m_pipeline)
        {
            m_eosSeen=true;
        }
    }

    virtual void bufferMessageHandler(GstMessage*msg)
    {
        int percent, in, out;
        GstBufferingMode mode;
        gint64 left;

        gst_message_parse_buffering(msg,&percent);
        gst_message_parse_buffering_stats (msg, &mode, &in, &out, &left);

        switch(mode)
        {
            case GST_BUFFERING_STREAM:
                GST_INFO_OBJECT (m_pipeline, "Queue %s is STREAMBUFFERING",
                    GST_OBJECT_NAME (msg->src));
                break;
            case GST_BUFFERING_DOWNLOAD:
                GST_INFO_OBJECT (m_pipeline, "Queue %s is DOWNLOADBUFFERING",
                    GST_OBJECT_NAME (msg->src));
                break;
            case GST_BUFFERING_TIMESHIFT:
                GST_INFO_OBJECT (m_pipeline, "Queue %s is TIMESHIFTING",
                    GST_OBJECT_NAME (msg->src));
                break;
            case GST_BUFFERING_LIVE:
                GST_INFO_OBJECT (m_pipeline, "Queue %s is LIVEBUFFERING",
                    GST_OBJECT_NAME (msg->src));
                break;
            default:
                GST_INFO_OBJECT (m_pipeline, "Queue %s is UNKNOWN",
                    GST_OBJECT_NAME (msg->src));
                break;
        }

        if(percent==100)
        {
            GST_WARNING_OBJECT (m_pipeline, "Queue %s is full",
                GST_OBJECT_NAME (msg->src));
        }
        else if(!percent)
        {
            GST_WARNING_OBJECT (m_pipeline, "Queue %s is empty",// - avail %.1f s left ( %d %d )\n",
                GST_OBJECT_NAME (msg->src));
        }
        else
        {
            GST_WARNING_OBJECT (m_pipeline, "%s (avg in %d out %d) Room used %d %% %lu ms",
                GST_OBJECT_NAME (msg->src),
                in, out, percent, left);
        }


        //genericMessageHandler(msg,"Buffer");
    }

    virtual void qosMessageHandler(GstMessage*msg)
    {
        gboolean islive=FALSE;
        guint64 running, stream, timestamp, duration;
        gst_message_parse_qos(msg,&islive,&running, &stream, &timestamp, &duration);

        
        GstFormat format;
        guint64 processed, dropped;
        gst_message_parse_qos_stats(msg, &format, &processed, &dropped);

        GST_WARNING_OBJECT (m_pipeline, "QOS msg %s - %lu dropped",// - avail %.1f s left ( %d %d )\n",
            GST_OBJECT_NAME (msg->src),
            dropped);


    }

    virtual void errorMessageHandler(GstMessage*msg)
    {
        genericMessageHandler(msg,"Error");
    }

    virtual void stateChangeMessageHandler(GstMessage*msg)
    {
        GstState old_state, new_state;
        gst_message_parse_state_changed(msg, &old_state, &new_state, NULL);
        GST_DEBUG_OBJECT (m_pipeline, "State change '%s' from '%s' to '%s'",GST_OBJECT_NAME (msg->src), gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));

        // catch our source
        if((GstElement*)(msg->src)==(GstElement*)m_pipeline)
        {
            GST_INFO_OBJECT (m_pipeline, "PIPELINE State change '%s' -> '%s'",gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
            pipelineStateChangeMessageHandler(msg);
            m_pipelineState=new_state;
        }
    }


    virtual void pipelineStateChangeMessageHandler(GstMessage*msg){}

    virtual void elementMessageHandler(GstMessage*msg)
    {
        genericMessageHandler(msg,"Element");
    }



    void genericMessageHandler(GstMessage*msg, const char*text)
    {
        GST_INFO_OBJECT (m_pipeline, "Message '%s' seen from '%s'",text, GST_OBJECT_NAME (msg->src));
        //g_print("Message '%s' seen from '%s'\n",text, GST_OBJECT_NAME (msg->src));
    }



protected:

    GstBus *m_bus;
    pipelineType *m_pipeline;

    GstNetTimeProvider *m_networkClock;

    GstState m_pipelineState;
    bool m_eosSeen;


    // source element, dest element, are we blocking, how many
    std::vector<std::tuple<GstElement*,GstElement*,bool>> m_sometimesConnects;

    static int m_initCount;

    volatile int m_blockedPins;
    GstEvent *m_seekLateEvent;
    GstElement *m_seekLateOn;

};


class gstreamPipeline : public gstreamPipelineBase<GstElement>
{

protected:
    gstreamPipeline(){}

public:

    gstreamPipeline(const char *pipelineName):gstreamPipelineBase(pipelineName)
    {}

};

#endif // _GSTREAMPIPELINE_H