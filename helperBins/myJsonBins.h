#ifndef _myjsonbins_guard
#define _myjsonbins_guard

#include "myElementBins.h"
#include "../myplugins/gstjsonparse.h"
#include "../json/json.hpp"
#include <time.h>

// uses the myplugin 'jsonparse' - peeks the utf stream, via a callback, tries to parse the json, and passes it up the vtable
// a derived bin will overload PeekJson to work out what push out the src pad
class gstJsonParseBin : public gstreamBin
{

public:

    gstJsonParseBin(pluginContainer<GstElement> *parent):gstreamBin("JsonParseBin",parent)
    {
        jsonparse_registerRunTimePlugin();

        pluginContainer<GstElement>::AddPlugin("jsonparse");

        GstJsonparse *jsonParser=GST_JSONPARSE(pluginContainer<GstElement>::FindNamedPlugin("jsonparse"));
        if(jsonParser)
            jsonParser->myClassPointer=this;

        AddGhostPads("jsonparse","jsonparse");
    }

    // if bufOut == NULL, we're in_place active
    virtual void PeekBuffer(GstBuffer * buf, GstBuffer *bufOut)
    {

        GstMemory *memory=gst_buffer_peek_memory(buf,0);

        GstMapInfo info;
        gst_memory_map(memory,&info,GST_MAP_READ);
        std::string jsonString((const char*)info.data, info.size);

        // TODO - check for failure
        try {
            auto jData = nlohmann::json::parse(jsonString.c_str());

            // send this up the vpipe
            PeekJson(buf,jData,bufOut);
        }
        catch(...)
        {
            // oops
            GST_ERROR_OBJECT (m_parent, "Exception parsing json '%s'", jsonString.c_str());
            // try to recover
            nlohmann::json jsonNull;
            gstJsonParseBin::PeekJson(buf,jsonNull,bufOut);
        }

        gst_memory_unmap(memory,&info);


    }

    // if outbuf != NULL we're not in_place - up to you dear reader to 'fill' the outbuf
    // this base will just do a copy ...
    virtual void PeekJson(GstBuffer *buf, nlohmann::json &jsondata,GstBuffer *outbuf)
    {
        if(outbuf)
        {
            // ok - copy src to dest
            GstMemory *memory=gst_buffer_peek_memory(buf,0);
            gst_buffer_replace_all_memory(outbuf,gst_memory_share(memory,0,-1));

        }
    }



};





// base class for turning json into pango
// overload TurnJsonToPango
class gstJsonToPangoBin : public gstJsonParseBin
{
public:
    gstJsonToPangoBin(pluginContainer<GstElement> *parent):gstJsonParseBin(parent)
    {
        // for this to work we need to NOT be an inplace transform ....
        // so, turn it off
        GstJsonparse *jsonParser=GST_JSONPARSE(pluginContainer<GstElement>::FindNamedPlugin("jsonparse"));
        GstBaseTransform *base=GST_BASE_TRANSFORM(jsonParser);
        gst_base_transform_set_in_place(base, FALSE);

    }

    virtual void PeekJson(GstBuffer *buf, nlohmann::json &jsondata,GstBuffer *outbuf)
    {
        // call up the vtable
        std::string pango=TurnJsonToPango(jsondata, buf);

        int len=pango.length();
        gst_buffer_fill (outbuf, 0, pango.c_str(), len);
        gst_buffer_set_size (outbuf, len);

        // we need to preserve META
        gst_buffer_copy_into(outbuf,buf,GST_BUFFER_COPY_METADATA ,0,-1);


    }

    virtual std::string TurnJsonToPango(nlohmann::json &jsondata,GstBuffer *)
    {
        return "overload TurnJsonToPango";
    }

};









#define PANGO_BUFFER  200



template <class parser>
class gstJsonToPangoRenderBinT : public gstreamBin
{

public:
/*
halign
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


    gstJsonToPangoRenderBinT(pluginContainer<GstElement> *parent, const char *name="pangoBin", unsigned halign=1, unsigned valign=1,bool shaded=false):
        gstreamBin(name,parent),
        m_jsonPeek(this)
    {
        pluginContainer<GstElement>::AddPlugin("textoverlay");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("textoverlay"),
            "valignment", valign, 
            "halignment", halign, 
            "shaded-background", shaded?TRUE:FALSE, 
            "draw-outline", FALSE, 
            NULL);


        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("textoverlay"), 
            "wait-text", true, NULL);

        // connect them together
        // subtitle
        // video        

        gst_element_link_many(   pluginContainer<GstElement>::FindNamedPlugin(m_jsonPeek),
                            pluginContainer<GstElement>::FindNamedPlugin("textoverlay"),
                            NULL);

        // ghost my sink for json / utf-8
        AddGhostPads(m_jsonPeek, NULL);
        // ghost the video/raw pad for text overlay
        AddGhostPads("textoverlay", "textoverlay");

    }

private:
    parser m_jsonPeek;    

};



// build a chain of pango renderers 
class gstMultiJsonToPangoRenderBin : public gstCapsFilterBaseBin
{
public:    
    gstMultiJsonToPangoRenderBin(pluginContainer<GstElement> *parent):
        gstCapsFilterBaseBin(parent,gst_caps_new_simple ("text/x-raw", "format", G_TYPE_STRING, "utf8", NULL),"multiPangoBin"),
        m_first(NULL), m_last(NULL)
    {
        // video -> pango1 video -> pango2 video
        // subtitle -> tee -> mq -> pango1
        //                 \> mq -> pango2


        // the t is for splitting the subtitles up for the kids, video passes thru all of them
        pluginContainer<GstElement>::AddPlugin("tee");
        pluginContainer<GstElement>::AddPlugin("queue");
        // so join them up
        gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("capsfilter"),FindNamedPlugin("tee"));        
        // every tee goes thru the mq, threads for free
        pluginContainer<GstElement>::AddPlugin("multiqueue");

        // ghost the subs magnet for now
        AddGhostPads("capsfilter");
        AddGhostPads("queue");

//#define _TRY_OPENGL
#ifdef _TRY_OPENGL
        pluginContainer<GstElement>::AddPlugin("glupload");
        pluginContainer<GstElement>::AddPlugin("glcolorconvert");
        pluginContainer<GstElement>::AddPlugin("glcolorbalance");
        pluginContainer<GstElement>::AddPlugin("gloverlaycompositor");
        pluginContainer<GstElement>::AddPlugin("gldownload");
        pluginContainer<GstElement>::AddPlugin("videoconvert","vidconvout");

        gst_element_link_many(  pluginContainer<GstElement>::FindNamedPlugin("glupload"),
                                pluginContainer<GstElement>::FindNamedPlugin("glcolorconvert"),
                                pluginContainer<GstElement>::FindNamedPlugin("glcolorbalance"),
                                pluginContainer<GstElement>::FindNamedPlugin("gloverlaycompositor"),
                                pluginContainer<GstElement>::FindNamedPlugin("gldownload"),
                                pluginContainer<GstElement>::FindNamedPlugin("vidconvout"),
                                NULL);
        AddGhostPads(NULL,"vidconvout");
#endif        

    }

    ~gstMultiJsonToPangoRenderBin()
    {
        for(auto each=m_pangoBins.begin();each!=m_pangoBins.end();each++)
        {
            delete *each;
        }
    }

    template <class parser>
    bool add(const char*name,unsigned halign, unsigned valign)
    {
        // create a new one
        gstJsonToPangoRenderBinT<parser> *newone=new gstJsonToPangoRenderBinT<parser>(this,name,halign,valign);

        if(!newone)
            return false;

        gst_element_link_many(pluginContainer<GstElement>::FindNamedPlugin("tee"),
            pluginContainer<GstElement>::FindNamedPlugin("multiqueue"),
            pluginContainer<GstElement>::FindNamedPlugin(*newone),
            NULL);

        if(!m_first)
        {
            m_last=m_first=newone;
            // ghost the raw video and raw text pins of the first pango renderer
            gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("queue"),pluginContainer<GstElement>::FindNamedPlugin(*m_first));
        }
        else
        {
            gst_element_link(pluginContainer<GstElement>::FindNamedPlugin(*m_last), pluginContainer<GstElement>::FindNamedPlugin(*newone));
            m_last=newone;
        }


        m_pangoBins.push_back(newone);   

        return true;
    }

    void finished()
    {
        // TODO test this!
        if(!m_pangoBins.size())
        {
            // need to sink the subs somewhere
            pluginContainer<GstElement>::AddPlugin("fakesink");
            gst_element_link_many(pluginContainer<GstElement>::FindNamedPlugin("tee"),
                                    pluginContainer<GstElement>::FindNamedPlugin("fakesink"),
                                    NULL
                                    );
            // and ghost 
            AddGhostPads(NULL,"queue");
        }
        else
        {
#ifdef _TRY_OPENGL
            gst_element_link(pluginContainer<GstElement>::FindNamedPlugin(*m_last), pluginContainer<GstElement>::FindNamedPlugin("glupload"));
#else
            AddGhostPads(NULL, *m_last);
#endif        
        }
    }

protected:

    std::vector<gstreamBin*> m_pangoBins;
    gstreamBin *m_first, *m_last;

};




#endif //_myjsonbins_guard
