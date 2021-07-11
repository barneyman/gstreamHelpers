#include "gstreamBin.h"
#include "myplugins/gstjsonparse.h"
#include "json/json.hpp"

// uses the myplugin 'jsonparse' - peeks the utf stream, via a callback, tries to parse the json, and passes it up the vtable
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

// toy parser i was using to create sections ... dead now - but an example
class gstJsonParseSatellitesBin : public gstJsonParseBin
{
public:
    gstJsonParseSatellitesBin(pluginContainer<GstElement> *parent):gstJsonParseBin(parent),
        m_capturing(false)
    {

    }

    virtual void PeekJson(GstBuffer *buf, nlohmann::json &jsondata,GstBuffer *outbuf)
    {
        // check for number of satellites
        if(jsondata.contains("satelliteCount"))
        {
            unsigned numSats=jsondata["satelliteCount"].get<unsigned>();
            if(!(numSats %2))
            {
                if(!m_capturing)
                {
                    m_capturing=true;
                    // save the seek time
                    m_findings.push_back(std::pair<GstClockTime, unsigned>(buf->pts,numSats));

                    //g_print("pushing %u %" GST_TIME_FORMAT "\n", numSats, GST_TIME_ARGS(buf->pts));
                }
            }
            else
            {
                m_capturing=false;
            }
        }

        // then call the base - that will do a simple in -> out copy
        gstJsonParseBin::PeekJson(buf, jsondata, outbuf);

    }


    std::vector<std::pair<GstClockTime, unsigned>> m_findings;

    bool m_capturing;

};



// base class for turning json into pango
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
        return "Steve";
    }

};





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


    gstJsonToPangoRenderBinT(pluginContainer<GstElement> *parent, const char *name="pangoBin", unsigned halign=1, unsigned valign=1,bool shaded=true):
        gstreamBin(name,parent),
        m_jsonPeek(this)
    {
        pluginContainer<GstElement>::AddPlugin("textoverlay");

        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("textoverlay"),
            "valignment", valign, 
            "halignment", halign, 
            "shaded-background", shaded?TRUE:FALSE, 
            NULL);


        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("textoverlay"), 
            "wait-text", true, NULL);

        // connect them together
        // subtitle
        // video        

        gst_element_link(   pluginContainer<GstElement>::FindNamedPlugin(m_jsonPeek),
                            pluginContainer<GstElement>::FindNamedPlugin("textoverlay"));


        AddGhostPads(m_jsonPeek, NULL);
        AddGhostPads("textoverlay", "textoverlay");

    }

private:
    parser m_jsonPeek;    

};




#define PANGO_BUFFER  200

// 
class gstJsonNMEAtoSpeed : public gstJsonToPangoBin
{
public:    
    gstJsonNMEAtoSpeed(pluginContainer<GstElement> *parent):gstJsonToPangoBin(parent)
    {
    }

    virtual std::string TurnJsonToPango(nlohmann::json &jsondata,GstBuffer *)
    {
        char msg[PANGO_BUFFER];
        int len=0;

        if(jsondata.contains("speedKMH") && jsondata.contains("bearingDeg") && jsondata.contains("satelliteCount"))
        {

            len=snprintf(msg, sizeof(msg), "<span foreground=\"white\" size=\"small\">%d km/h %.1f° %2d sats</span>",
                jsondata["speedKMH"].get<int>(),
                jsondata["bearingDeg"].get<float>(),
                jsondata["satelliteCount"].get<int>());

            return msg;  
        }

        return "Subtitle Error";
    }
};


class gstJsonFrameNumber : public gstJsonToPangoBin
{
public:    
    gstJsonFrameNumber(pluginContainer<GstElement> *parent):gstJsonToPangoBin(parent)
    {
    }

    virtual std::string TurnJsonToPango(nlohmann::json &,GstBuffer *inbuf)
    {
        char msg[PANGO_BUFFER];
        int len=0;

        len=snprintf(msg, sizeof(msg), "<span foreground=\"white\" size=\"small\">DTS %" GST_TIME_FORMAT " PTS %" GST_TIME_FORMAT " Dur %" GST_TIME_FORMAT "</span>",
        //len=snprintf(msg, sizeof(msg), "<span foreground=\"white\" size=\"small\">Frame %lu DTS %" GST_TIME_FORMAT " PTS %" GST_TIME_FORMAT " Dur %" GST_TIME_FORMAT "</span>",
        //    inbuf->offset,
            GST_TIME_ARGS(inbuf->dts),
            GST_TIME_ARGS(inbuf->pts),
            GST_TIME_ARGS(inbuf->duration)
            );

        return msg;  
    }
};




class gstJsonNMEAtoLongLat : public gstJsonToPangoBin
{
public:    
    gstJsonNMEAtoLongLat(pluginContainer<GstElement> *parent):gstJsonToPangoBin(parent)
    {
    }

    virtual std::string TurnJsonToPango(nlohmann::json &jsondata,GstBuffer *)
    {
        char msg[PANGO_BUFFER];
        int len=0;

        if(jsondata.contains("longitudeE") && jsondata.contains("latitudeN"))
        {

            len=snprintf(msg, sizeof(msg), "<span foreground=\"white\" size=\"small\">Long:%.4f Lat:%.4f</span>",
                jsondata["longitudeE"].get<float>(),
                jsondata["latitudeN"].get<float>()
                );

            return msg;  
        }

        return "Subtitle Error";
    }
};


class gstJsonNMEAtoUTC : public gstJsonToPangoBin
{
public:    
    gstJsonNMEAtoUTC(pluginContainer<GstElement> *parent):gstJsonToPangoBin(parent)
    {
    }

    virtual std::string TurnJsonToPango(nlohmann::json &jsondata,GstBuffer *)
    {
        char msg[PANGO_BUFFER];
        int len=0;

        if(jsondata.contains("utc"))
        {
            len=snprintf(msg, sizeof(msg), "<span foreground=\"white\" size=\"small\">%s</span>",
                jsondata["utc"].get<std::string>().c_str()
                );

            return msg;  
        }

        return "Subtitle Error";
    }
};


class gstMultiJsonToPangoRenderBin2 : public gstreamBin
{
public:    
    gstMultiJsonToPangoRenderBin2(pluginContainer<GstElement> *parent):
        gstreamBin("multiPangoBin",parent),
        m_first(NULL), m_last(NULL)
    {
        // video -> pango1 video -> pango2 video
        // subtitle -> tee -> mq -> pango1
        //                 \> mq -> pango2


        // we expose this as a sink to grab subtitles
        pluginContainer<GstElement>::AddPlugin("capsfilter","capsSub");
        g_object_set (pluginContainer<GstElement>::FindNamedPlugin("capsSub"), 
            "caps", gst_caps_new_simple ("text/x-raw", "format", G_TYPE_STRING, "utf8", NULL), NULL);

        // the t is for splitting the subtitles up for the kids, video passes thru all of them
        pluginContainer<GstElement>::AddPlugin("tee");
        // so join them up
        gst_element_link(pluginContainer<GstElement>::FindNamedPlugin("capsSub"),FindNamedPlugin("tee"));        
        // every tee goes thru the mq, threads for free
        pluginContainer<GstElement>::AddPlugin("multiqueue");



        // ghost the subs magnet for now
        AddGhostPads("capsSub");

    }

    ~gstMultiJsonToPangoRenderBin2()
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
            AddGhostPads(*m_first);
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
        AddGhostPads(NULL, *m_last);
    }

protected:

    std::vector<gstreamBin*> m_pangoBins;
    gstreamBin *m_first, *m_last;

};

#define _PANGO_SPEED    1
#define _PANGO_LONGLAG  2
#define _PANGO_UTC      4
#define _PANGO_FRAME    8
class gstMultiJsonToPangoRenderBin : public gstMultiJsonToPangoRenderBin2
{
public:
    gstMultiJsonToPangoRenderBin(pluginContainer<GstElement> *parent, unsigned mask = 0x0f):
        gstMultiJsonToPangoRenderBin2(parent)
    {
        if(mask&_PANGO_SPEED)
            add<gstJsonNMEAtoSpeed>("pangoBin1", 2, 1);
        if(mask&_PANGO_LONGLAG)
            add<gstJsonNMEAtoLongLat>("pangoBin2", 0, 1);
        if(mask&_PANGO_UTC)
            add<gstJsonNMEAtoUTC>("pangoBin3", 1, 2);
        if(mask&_PANGO_FRAME)
            add<gstJsonFrameNumber>("pangobin4", 1, 4);

        finished();
    }


};


// a toy - dead now i think
class jsonParserExamine : public gstreamBin
{
public:

    jsonParserExamine(gstreamPipeline *parent,const char *source):gstreamBin("jsonParseBin",parent),
        m_source(parent,source),
        m_progress(parent),
        m_jsonParser(parent)
    {
        // no clock sync, and pull the data - fast
        parent->AddPlugin("fakesink","fakesink_subs");
        g_object_set (parent->pluginContainer<GstElement>::FindNamedPlugin("fakesink_subs"), 
            "sync",FALSE,NULL);//"can-activate-pull",TRUE,"can-activate-push",FALSE,NULL);

        parent->ConnectPipeline(m_source,m_jsonParser);
        parent->ConnectPipeline(m_jsonParser,"fakesink_subs", m_progress);

        parent->Run();

    }

    gstJsonParseSatellitesBin m_jsonParser;
    gstFrameBufferProgress m_progress;
    gstDemuxMP4SubsOnlyDecodeBin m_source;


};


