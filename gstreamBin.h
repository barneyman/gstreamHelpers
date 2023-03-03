#ifndef _GSTREAMBIN_H
#define _GSTREAMBIN_H

#include "gstreamPipeline.h"

// TODO calls that leak - https://gstreamer.freedesktop.org/documentation/coretracers/leaks.html?gi-language=c
// gst_pad_query_caps
// gst_element_request_pad

class gstreamAdvertisingBin;

class gstreamBin : public pluginContainer<GstElement>
{
public:
    gstreamBin(const char *binName, pluginContainer<GstElement> *parent);

    virtual ~gstreamBin();

    const char *Name() { return m_binName.c_str(); }
    operator const char*() { return Name(); }

    bool AddGhostPads(const char*sink,const char*source=NULL, const char *sinkcaps=NULL, const char *srccaps=NULL);
    bool AddGhostPads(GstElement *sinkElement,GstElement*sourceElement=NULL, const char *sinkcaps=NULL, const char *srccaps=NULL);


    virtual GstPad *request_new_pad (GstElement * element,GstPadTemplate * templ,const gchar * name,const GstCaps * caps);
    virtual bool release_requested_pad(GstElement*el, GstPad *pad);

    // normally called by an owning class's dtor, so you can unlink from it
    virtual void releaseRequestedPads();


    virtual void PeekBuffer(GstBuffer * buf, GstBuffer *bufOut)
    {}

    virtual void segment_event(GstEvent *ev)
    {}

    void IterateAndGhost(GList *elementPads, std::vector<GstPad*> &results, GstCaps *allowedCaps=NULL);

protected:

    GstPad*GhostSingleRequestPad(GstPad *eachPad) { return GhostSinglePad(eachPad,m_requestedPadsGhosted); }
    GstPad*GhostSingleSinkPad(GstPad *eachPad) { return GhostSinglePad(eachPad,m_ghostPadsSinks); }
    GstPad*GhostSingleSrcPad(GstPad *eachPad) { return GhostSinglePad(eachPad,m_ghostPadsSrcs); }
    GstPad*GhostSinglePad(GstPad *eachPad, std::vector<GstPad*> &results);

    GstPad*GhostSingleSinkPad(const char *name) { return GhostSingleSinkPad(FindNamedPlugin(name)); }
    GstPad*GhostSingleSinkPad(GstElement *sink);



    void releaseSingleRequestedPin(GstElement*element, GstPad *pad)
    {
        gst_element_release_request_pad(element,pad);
        // we grabbed a ref when we asked
        gst_object_unref (element);
        // release the pad
        gst_object_unref (pad);

    }


    GstClockTime GetRunningTime()
    {

        GstClockTime baseTime=gst_element_get_base_time(GST_ELEMENT(m_myBin));        
        GstClock *clock=gst_element_get_clock(GST_ELEMENT(m_myBin));
        GstClockTime currentTime=gst_clock_get_time(clock);
        gst_object_unref(clock);

        return currentTime-baseTime;
    }

    void setBinFlags(unsigned flags)
    {
        GST_OBJECT_FLAG_SET(m_myBin, flags);
    }


    // caps helpers
    bool doCapsIntersect(const char *caps1txt,const char*caps2txt) 
    {
        GstCaps *caps1=gst_caps_from_string(caps1txt);
        bool ret=doCapsIntersect(caps1,caps2txt);
        gst_caps_unref(caps1);
        return ret;
    }

    bool doCapsIntersect(const GstCaps* caps1,const char*caps2txt) 
    {
        GstCaps *caps2=gst_caps_from_string(caps2txt);
        bool ret=doCapsIntersect(caps1,caps2);
        gst_caps_unref(caps2);
        return ret;
    }

    bool doCapsIntersect(const GstCaps* caps1,const GstCaps* caps2) 
    {
        gchar *caps1String=gst_caps_to_string(caps1);
        gchar *caps2String=gst_caps_to_string(caps2);
        GST_INFO_OBJECT (m_myBin, "Caps intersection '%s' in bin '%s' caps '%s'",(caps1String),Name(), caps2String);
        g_free((gpointer)caps1String);
        g_free((gpointer)caps2String);

        return gst_caps_can_intersect(caps1,caps2);
    }
protected:

    pluginContainer<GstElement> *m_parent;
    std::string m_binName;
    GstElement* m_myBin;

    std::vector<GstPad*> m_ghostPadsSinks,m_ghostPadsSrcs,m_requestedPadsGhosted;

protected:



    void addPadToBeReleased(GstElement*el,GstPad*pad)
    {
        GST_INFO_OBJECT (m_myBin, "Adding pad '%s' to element '%s'",GST_PAD_NAME(pad),GST_ELEMENT_NAME(el));

        // grab a ref on the parent while we're here
        gst_object_ref (el);
        m_padsToBeReleased.push_back(std::pair<GstElement*,GstPad*>(el,pad));
    }

    std::vector<std::pair<GstElement*,GstPad*> > m_padsToBeReleased;


};

/*
#include "helperBins/myElementBins.h"

// used to advertise 'on request' pads from an element
class gstreamAdvertisingBin : public gstreamBin
{
public:
    gstreamAdvertisingBin(pluginContainer<GstElement> *parent,GstCaps*caps):
        gstreamBin("advertisingBin",parent),
        m_capsFilter(this,caps)
    {
        AddGhostPads(m_capsFilter,NULL);
    }

    ~gstreamAdvertisingBin()
    {

    }

    void advertise(GstElement *element)
    {

    }

protected:

    gstCapsFilterSimple m_capsFilter;

};
*/


#include <tuple>

// as above, but listens for pad-added and no-more pads
// normally sent by a demuxer when preroll works out what's there
class gstreamListeningBin : public gstreamBin
{
public:
    gstreamListeningBin(const char *binName, pluginContainer<GstElement> *parent):
        gstreamBin(binName,parent),m_demuxerFinished(false)

    {
    }

    // connect late and create a ghost pad (special dest==NULL behaviour)
    bool ConnectLateThenGhostSrc(const char*source)
    {
        return ConnectLate(source,NULL);
    }

    bool ConnectLate(const char*source, const char*dest)
    {
        return ConnectLate(FindNamedPlugin(source),FindNamedPlugin(dest));
    }


    bool ConnectLate(GstElement *source, GstElement *dest);

protected:

    static void staticPadAdded(GstElement *element,GstPad *pad,gpointer data)
    {
        ((gstreamListeningBin*)data)->padAdded(element,pad);
    }

    static void staticNoMoreToConnectTo(GstElement *element,gpointer data)
    {
        ((gstreamListeningBin*)data)->noMoreToConnectTo(element);
        // and tell anyone who's listening to me 
    }

    virtual void padAdded(GstElement*src,GstPad*padAdded);
    virtual void noMoreToConnectTo(GstElement*);


    // stolen from gstreamPipeline.h
    // TODO rationalise with gstreamPipeline.h
    bool ConnectSrcToSink(GstPad*srcPad, GstElement *sinkElement);

protected:

    // src element, dest element
    std::vector<std::tuple<GstElement*,GstElement*> > m_lateSrcs;
    volatile bool m_demuxerFinished;

};

class gstreamListeningBinQueued : public gstreamListeningBin
{
public:
    gstreamListeningBinQueued(const char *binName, pluginContainer<GstElement> *parent):
        gstreamListeningBin(binName,parent)
    {
        AddPlugin("multiqueue","demuxmultiqueue");
    }

    bool ConnectLate(const char*source, const char*dest=NULL)
    {
        if(!dest)
            dest="demuxmultiqueue";

        return gstreamListeningBin::ConnectLate(FindNamedPlugin(source),FindNamedPlugin(dest));
    }

    ~gstreamListeningBinQueued()
    {
        releaseRequestedPads();   
    }
};



#endif // _GSTREAMBIN_H