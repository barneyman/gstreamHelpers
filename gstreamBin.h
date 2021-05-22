#ifndef _GSTREAMBIN_H
#define _GSTREAMBIN_H

#include "gstreamPipeline.h"

#define _USE_MY_BIN // use the mybin plugin subclass of gstbin


class gstreamBin : public pluginContainer<GstElement>
{
public:
    gstreamBin(const char *binName, pluginContainer<GstElement> *parent);

    const char *Name() { return m_binName.c_str(); }
    operator const char*() { return Name(); }

    bool AddGhostPads(const char*sink,const char*source=NULL, const char *sinkcaps=NULL, const char *srccaps=NULL);
    bool AddGhostPads(GstElement *sinkElement,GstElement*sourceElement=NULL, const char *sinkcaps=NULL, const char *srccaps=NULL);


    virtual GstPad *request_new_pad (GstElement * element,GstPadTemplate * templ,const gchar * name,const GstCaps * caps)
    {
        return NULL;
    }

    virtual void PeekBuffer(GstBuffer * buf)
    {}


    void IterateAndGhost(GList *elementPads, std::vector<GstPad*> &results, GstCaps *allowedCaps=NULL);

protected:

    GstPad*GhostSingleSinkPad(GstPad *eachPad) { return GhostSinglePad(eachPad,m_ghostPadsSinks); }
    GstPad*GhostSingleSrcPad(GstPad *eachPad) { return GhostSinglePad(eachPad,m_ghostPadsSrcs); }
    GstPad*GhostSinglePad(GstPad *eachPad, std::vector<GstPad*> &results);


protected:

    pluginContainer<GstElement> *m_parent;
    std::string m_binName;
    GstElement* m_myBin;

    std::vector<GstPad*> m_ghostPadsSinks,m_ghostPadsSrcs;

};

#include <tuple>

// as above, but listens for pad-added and no-more pads
class gstreamListeningBin : public gstreamBin
{
public:
    gstreamListeningBin(const char *binName, pluginContainer<GstElement> *parent):
        gstreamBin(binName,parent),m_demuxerFinished(false)

    {
    }

    bool ConnectLate(const char*source, const char*dest=NULL)
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


    // stollen from gstreamPipeline.h
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

};



#endif // _GSTREAMBIN_H