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

    int AddPlugin(const char *plugin, const char *name=NULL, nlohmann::json *settings=NULL, bool complainOnErr=true)
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

        // if we're not complaining, bail on the most frequent problem, early
        if(!complainOnErr)
        {
            GstElementFactory *factory=gst_element_factory_find (overridePlugin.c_str());
            if(!factory)
                return 3;
            gst_object_unref(factory);
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

    void SendActionSignal(const char *plugin,const char*action)
    {
        g_signal_emit_by_name (FindNamedPlugin(plugin), action);
    }

protected:

    std::vector<std::pair<std::string,GstElement*> > m_allPlugins;


};



template <class pipelineType>
class gstreamPipelineBase : public pluginContainer<pipelineType>
{

protected:

    gstreamPipelineBase():m_bus(NULL),m_pipeline(NULL),m_pipelineState(GST_STATE_NULL),
        m_blockedPins(0), m_seekLateEvent(NULL), m_seekLateOn(NULL),
        m_networkClock(NULL)
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
        m_blockedPins(0), m_seekLateEvent(NULL), m_seekLateOn(NULL),
        m_networkClock(NULL)
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

        if(m_mainLoop)
        {
            g_main_loop_unref(m_mainLoop);
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

        m_seekLateOn=onElement;

        if(!m_seekLateOn)
            return false;

        m_seekLateEvent=gst_event_new_seek(
            1.0,
            GST_FORMAT_TIME,
            // flush is needed to get the 'padblocked' probe call
            (GstSeekFlags)(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
            startAt?GST_SEEK_TYPE_SET:GST_SEEK_TYPE_NONE,
            startAt,
            endAt?GST_SEEK_TYPE_SET:GST_SEEK_TYPE_NONE,
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


        // then wait for us to play out
        return internalRun(0);

    }

    void setLatency(GstClockTime latMillis)
    {
        gst_pipeline_set_latency(GST_PIPELINE(m_pipeline),latMillis*GST_MSECOND);
    }

    static gboolean static_bus_handler (GstBus * bus, GstMessage * message, gpointer data)
    {
        return ((gstreamPipelineBase*)data)->bus_handler (bus, message);
    }

    gboolean bus_handler (GstBus * bus, GstMessage * message)
    {
        CrackMessage(message);

        return TRUE;
    }

    time_t m_looptimeout=0;
    time_t m_startedAt=0;

    bool Run(unsigned long timeoutSeconds=0, bool doReady=true)
    {
        if(m_seekLateEvent)
            return SeekRun(doReady);

        GST_INFO_OBJECT (m_pipeline, "Running for %lu seconds",timeoutSeconds);
        
        if(doReady)
        {
            if(!Ready())
            {
                return false;
            }
        }

        DumpGraph("Ready");

        return internalRun(timeoutSeconds);


    }


    bool PreRoll() { return ChangeStateAndWait(GST_STATE_PAUSED); }
    bool Pause() { return ChangeStateAndWait(GST_STATE_PAUSED); }
    bool Ready() { return ChangeStateAndWait(GST_STATE_READY); }
    bool Play() { return ChangeStateAndWait(GST_STATE_PLAYING); }
    bool Stop() { return ChangeStateAndWait(GST_STATE_NULL); }

    GstState CurrentPipelineState() 
    {
        return (m_pipelineState);
    }




    bool ChangeStateAndWait(GstState newState)
    {
        GstStateChangeReturn ret = gst_element_set_state ((GstElement*)m_pipeline, newState);

        bool retval=true;

        if (ret == GST_STATE_CHANGE_FAILURE) 
        {
            GST_ERROR_OBJECT(m_pipeline, "Unable to set the pipeline to '%s' state.", gst_element_state_get_name(newState));
            return false;
        }

        m_target_state=newState;

        if(ret==GST_STATE_CHANGE_NO_PREROLL)
        {
            GST_WARNING_OBJECT (m_pipeline, "Pipeline is live and does not need PREROLL");
            return true;
        }

        if(ret==GST_STATE_CHANGE_SUCCESS)
        {
            GST_INFO_OBJECT (m_pipeline, "state change to %s succeeded", gst_element_state_get_name(newState));
        }

        // otherwise it's ASYNC ...
        return true;

    }

    // requires GST_DEBUG_DUMP_DOT_DIR to be set
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
        GstCaps *srcCaps=gst_pad_query_caps(srcPad,NULL);
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

            // if we don't have fixed caps, accept will barf
            GstCaps *sinkCaps=gst_pad_query_caps(eachSinkPad,srcCaps);

            // hurrah! will it accept the caps we need?
            if(gst_caps_can_intersect(sinkCaps,srcCaps))
            {
                // try to link them
                if(GST_PAD_LINK_OK==gst_pad_link(srcPad,eachSinkPad))
                {
                    gst_pad_set_active(eachSinkPad,TRUE);
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

            gst_caps_unref(sinkCaps);
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
                if(ConnectSrcToSink(pad,std::get<1>(*each)))
                {

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

    GMainLoop *m_mainLoop = NULL;
    volatile GstState m_target_state = GST_STATE_PAUSED;
    volatile bool *m_exitOnBool=NULL;
    bool m_eosSent=false;

    bool sendEOStoEnd()
    {
        if(!m_eosSent)
        {
            m_eosSent=gst_element_send_event(GST_ELEMENT(m_pipeline),gst_event_new_eos());
            if(!m_eosSent)
            {
                GST_ERROR_OBJECT (m_pipeline, "sendEOStoEnd FAILED");
            }
        }
        return m_eosSent;
    }

    static gboolean staticTimeoutFunction(gpointer data)
    {
        return ((gstreamPipelineBase*)data)->timeoutFunction();        
    }

    gboolean timeoutFunction()
    {
        if(m_exitOnBool && *m_exitOnBool)
        {
            // exit the main loop
            sendEOStoEnd();
            return false;
        }

        if(m_looptimeout!=0 && m_startedAt!=0)
        {
            if(time(NULL)-m_startedAt>(m_looptimeout))
            {
                sendEOStoEnd();
                return false;
            }
        }
        return true;
    }

    bool internalRun(unsigned long timeoutSeconds)
    {
        gst_bus_add_watch(m_bus,static_bus_handler,this);

        GST_INFO_OBJECT (m_pipeline, "PREROLL request");
        if(!PreRoll())
        {
            DumpGraph("PREROLL FAILED");
            return false;
        }
        GST_INFO_OBJECT (m_pipeline, "PREROLL");
        DumpGraph("Paused");

        m_looptimeout=timeoutSeconds;

        m_mainLoop=g_main_loop_new (NULL, FALSE);
        // millis
        g_timeout_add(250,staticTimeoutFunction,this);

        g_main_loop_run (m_mainLoop);

        GST_INFO_OBJECT (m_pipeline, "Exiting pump loop");

        gst_bus_remove_watch(m_bus);

        return true;
    }

    GstClockTime GetRunningTime()
    {
        if(CurrentPipelineState()!=GST_STATE_PLAYING)
        {
            GST_ERROR_OBJECT (m_pipeline, "Asked for running time before playing");
            return GST_CLOCK_TIME_NONE;
        }

        GstClockTime baseTime=gst_element_get_base_time(GST_ELEMENT(m_pipeline));        
        GstClock *clock=gst_pipeline_get_clock(GST_PIPELINE(m_pipeline));
        GstClockTime currentTime=gst_clock_get_time(clock);
        gst_object_unref(clock);

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

            if(ConnectSrcToSink(eachSourcePad,destElement))
            {
                numberConnected++;
                continue;
            }

            GST_INFO_OBJECT (m_pipeline, mustConnectAll?"Element - Element FAILED":"fail ignored");

            if(mustConnectAll)
            {
                return false;
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
                    newClockMessageHandler(msg);
                    break;
                case GST_MESSAGE_LATENCY :
                    latencyMessageHandler(msg);
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
                    //break;
                case GST_MESSAGE_EOS:
                    // flag and out
                    eosMessageHandler(msg);
                    break;

                case GST_MESSAGE_SEGMENT_START:
                    genericMessageHandler(msg,"Segment Start");
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

                case GST_MESSAGE_PROGRESS:
                    asyncProgressHandler(msg);
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


public:

    // ensure you check for GST_CLOCK_TIME_NONE coming back!
    GstClockTime GetTimeSinceEpoch()
    {
    
        GstClock *pipeClock=gst_pipeline_get_clock(GST_PIPELINE(m_pipeline));
        if(pipeClock)
        {
            // systemclock returns time from 1970, NTPclock from 1900 ... so 
            GstClockTime ret=FixTimeForEpoch(gst_clock_get_time(pipeClock));
            gst_object_unref(pipeClock);
            return ret;
        }
        GST_ERROR_OBJECT (m_pipeline, "GetTimeSinceEpoch returning NONE");
        return GST_CLOCK_TIME_NONE;
    }


    GstClockTime FixTimeForEpoch(GstClockTime in)
    {
        // systemclock returns time from 1970, NTPclock from 1900 ... so 
        GstClock *pipeClock=gst_pipeline_get_clock(GST_PIPELINE(m_pipeline));
        if(pipeClock)
        {
            // real time?
            int clockType=0;
            g_object_get(pipeClock, "clock-type", &clockType, NULL);

            if(clockType!=(int)GST_CLOCK_TYPE_REALTIME)
            {
                GST_ERROR_OBJECT (m_pipeline, "Could not fix clock for Epoch %s",GST_OBJECT_NAME (pipeClock));
                gst_object_unref(pipeClock);
                return GST_CLOCK_TIME_NONE;

            }

            // clock type GST_TYPE_NTP_CLOCK
            if(GST_IS_NTP_CLOCK(pipeClock))
            {
                gst_object_unref(pipeClock);
                // RFC868 has you fam
                // https://datatracker.ietf.org/doc/html/rfc868 pp2
                in-=(2208988800L*GST_SECOND);
                return in;
            }

            if(GST_IS_NET_CLIENT_CLOCK(pipeClock))
            {
                gst_object_unref(pipeClock);
                // untested!
                // TODO
                exit(1);
            }

            if(GST_IS_SYSTEM_CLOCK(pipeClock))
            {
                gst_object_unref(pipeClock);
                return in;
            }
            gst_object_unref(pipeClock);
        }
        GST_ERROR_OBJECT (m_pipeline, "FixTimeforEpoch returning NONE");
        return GST_CLOCK_TIME_NONE;

    }


    // start a net clock up
    bool ProvideNetClock(unsigned port, GstClock *theClock)
    {
        if(m_networkClock)
            return false;

        m_networkClock=gst_net_time_provider_new(theClock, NULL, port);

        return m_networkClock?true:false;
    }

    bool UseNTPv4Clock(const char *host="0.au.pool.ntp.org", unsigned port=123, bool waitForSync=true, int timeoutS=-1)
    {


        // https://archive.fosdem.org/2016/schedule/event/syncgst_ntp_clock_newhronised_gstreamer/attachments/slides/889/export/events/attachments/synchronised_gstreamer/slides/889/synchronised_multidevice_media_playback_with_GStreamer.pdf
        GstClock *netClock=gst_ntp_clock_new("networkClock", host, port, 0);//gst_clock_get_time(gstSystemClk));
        if(netClock)
        {
            g_object_set(netClock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
            gst_pipeline_use_clock ((GstPipeline*)m_pipeline, netClock);
            if(waitForSync)
            {
                if(!gst_clock_wait_for_sync (netClock,timeoutS==-1?GST_CLOCK_TIME_NONE:GST_SECOND*timeoutS))
                {
                    GST_ERROR_OBJECT (m_pipeline, "Failed to NTP sync from %s:%u",host,port);
                    return false;
                }
                else
                {
                    GstClockTime now=gst_clock_get_time(netClock);
                    GST_INFO_OBJECT (m_pipeline, "Sync'd at %" GST_TIME_FORMAT "",GST_TIME_ARGS(now));
                }
                return true;
            }
        }
        else
        {
            GST_ERROR_OBJECT (m_pipeline, "Failed to NTP from %s:%u",host,port);
        }

        return netClock?true:false;
    }

    void setRealtimeClock()
    {
        
        // if we've already set a clock 
        GstClock *pipeclock=gst_pipeline_get_clock((GstPipeline*)m_pipeline);
        if(pipeclock)
        {
            g_object_set(pipeclock, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
            gst_object_unref(pipeclock);
        }
        else
        {
            GstClock* gstSystemClk = gst_system_clock_obtain();
            g_object_set(gstSystemClk, "clock-type", GST_CLOCK_TYPE_REALTIME, NULL);
            gst_object_unref(gstSystemClk);
        }
    }

protected:

    virtual void newClockMessageHandler(GstMessage*msg)
    {
        GstClock *clock;
        gst_message_parse_new_clock (msg, &clock);
        GST_INFO_OBJECT (m_pipeline, "New clock %s",GST_OBJECT_NAME (clock));

    }

    // virtual message handlers
    virtual void eosMessageHandler(GstMessage*msg)
    {
        // just check!
        if((GstElement*)(msg->src)==(GstElement*)m_pipeline)
        {
            GST_INFO_OBJECT (m_pipeline, "Pipeline seen EOS - going to ready");
            Ready();
        }
        else
        {
            GST_INFO_OBJECT (m_pipeline, "Saw EOS from %s",GST_ELEMENT_NAME(msg->src));
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
            GST_WARNING_OBJECT (m_pipeline, "Queue %s is empty",
                GST_OBJECT_NAME (msg->src));
        }
        else if(!percent)
        {
            GST_WARNING_OBJECT (m_pipeline, "Queue %s is full",// - avail %.1f s left ( %d %d )\n",
                GST_OBJECT_NAME (msg->src));
        }
        else
        {
            GST_WARNING_OBJECT (m_pipeline, "%s (avg in %d out %d) Room used %d %% %lu ms",
                GST_OBJECT_NAME (msg->src),
                in, out, percent, left==GST_CLOCK_TIME_NONE?0:left);
        }


        //genericMessageHandler(msg,"Buffer");
    }

    volatile bool m_asyncInProgress=false, m_prerolled=false;

    virtual void asyncProgressHandler(GstMessage*msg)
    {
        GstProgressType type;
        gchar *code, *text;

        gst_message_parse_progress (msg, &type, &code, &text);

 

        switch (type) {
          case GST_PROGRESS_TYPE_START:
          case GST_PROGRESS_TYPE_CONTINUE:
            m_asyncInProgress = true;
            break;
          case GST_PROGRESS_TYPE_COMPLETE:
          case GST_PROGRESS_TYPE_CANCELED:
          case GST_PROGRESS_TYPE_ERROR:
            m_asyncInProgress = false;
            break;
          default:
            break;
        }

        GST_INFO_OBJECT (m_pipeline, "progress msg - %s - async %s prerolled %s target %d", text,
            m_asyncInProgress?"TRUE":"FALSE",
            m_prerolled?"TRUE":"FALSE",
            m_target_state
            );

        if (!m_asyncInProgress && m_prerolled && m_target_state == GST_STATE_PAUSED) 
        {
            Play();
        }

    }

    virtual void qosMessageHandler(GstMessage*msg)
    {
        gboolean islive=FALSE;
        guint64 running, stream, timestamp, duration;
        gst_message_parse_qos(msg,&islive,&running, &stream, &timestamp, &duration);

        GstFormat format;
        guint64 processed, dropped;
        gst_message_parse_qos_stats(msg, &format, &processed, &dropped);

        GST_WARNING_OBJECT (m_pipeline, "QOS msg - %s %s - %lu dropped %lu processed",// - avail %.1f s left ( %d %d )\n",
            GST_OBJECT_NAME (msg->src),
            islive?"LIVE":"",
            dropped, processed);


    }

    virtual void errorMessageHandler(GstMessage*msg)
    {
        genericMessageHandler(msg,"Error");
    }

    virtual void stateChangeMessageHandler(GstMessage*msg)
    {
        GstState old_state, new_state, pendingState;
        gst_message_parse_state_changed(msg, &old_state, &new_state, &pendingState);

        GST_INFO_OBJECT (m_pipeline, "%s State change '%s' -> '%s'" ,GST_ELEMENT_NAME(msg->src),gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));

        if((GstElement*)(msg->src)==(GstElement*)m_pipeline)
        {
            pipelineStateChangeMessageHandler(msg, old_state, new_state,pendingState);
            m_pipelineState=new_state;
        }

        if(m_seekLateOn == (GstElement*)(msg->src) && new_state==GST_STATE_PAUSED)
        {
            // if we are seeking ..
            if(m_seekLateEvent)
            {
                GST_INFO_OBJECT (m_pipeline, "sending seek event");
                if(!gst_element_send_event(m_seekLateOn,m_seekLateEvent))
                {
                    GST_ERROR_OBJECT (m_pipeline, "seek event FAILED");
                }
                m_seekLateEvent=NULL;
                // this will cause another preroll ...
            }

        }
    }

    // important virtual - call me if you override me
    virtual void pipelineStateChangeMessageHandler(GstMessage*msg, GstState old_state,GstState new_state, GstState pendingState)
    {
        if (m_target_state==new_state)
        {
            switch(m_target_state)                        
            {

                case GST_STATE_PAUSED:

                    // we go paused->paused if we do a seekrun()
                    if(old_state==GST_STATE_READY || old_state==GST_STATE_PAUSED)
                    {
                        // we are paused, kick it into play and set target state
                        m_prerolled=true;

                        // if nothing async is in motion ...
                        if(!m_asyncInProgress)
                        {
                            Play();
                        }
                    }
                    break;


                case GST_STATE_PLAYING:
                    DumpGraph("Playing");
                    m_startedAt=time(NULL);
                    break;

                case GST_STATE_READY:
                    if(old_state==GST_STATE_PAUSED)
                    {
                        // we are closing down - stop
                        GST_INFO_OBJECT (m_pipeline, "Stopping main loop");
                        g_main_loop_quit(m_mainLoop);
                        Stop();
                    }
                    break;

                case GST_STATE_NULL:
                    if(old_state==GST_STATE_READY)
                    {
                        g_main_loop_quit(m_mainLoop);
                    }
                    break;


            }

        }
    }

    virtual void elementMessageHandler(GstMessage*msg)
    {
        genericMessageHandler(msg,"Element");
    }

    void latencyMessageHandler(GstMessage*msg)
    {
        genericMessageHandler(msg,"Latency");

        gst_bin_recalculate_latency (GST_BIN (m_pipeline));


    }

    void genericMessageHandler(GstMessage*msg, const char*text)
    {
        GST_DEBUG_OBJECT (m_pipeline, "Message '%s' seen from '%s'",text, GST_OBJECT_NAME (msg->src));
    }



protected:

    GstBus *m_bus;
    pipelineType *m_pipeline;

    GstNetTimeProvider *m_networkClock;

    volatile GstState m_pipelineState;


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