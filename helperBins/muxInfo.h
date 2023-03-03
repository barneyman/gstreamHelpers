
#include "../gstreamPipeline.h"
#include <mutex>

#ifndef _DEMUXINFO
#define _DEMUXINFO

class demuxInfo 
{

protected:

    std::mutex m_mutex;
    GstCaps *m_video,*m_audio,*m_subs;

public:
    demuxInfo():
        m_binDuration(0)
    {
        m_video=gst_caps_new_simple("video/x-h264",NULL);
        m_audio=gst_caps_new_simple("audio/ANY",NULL);
        m_subs=gst_caps_new_simple("text/x-raw",NULL);
    }

    demuxInfo(const demuxInfo& other)
    {
        *this=other;
    }

    ~demuxInfo()
    {
        for(auto each=m_demuxSrcs.begin();each!=m_demuxSrcs.end();each++)
        {
            gst_caps_unref(each->second);
        }

        gst_caps_unref(m_video);
        gst_caps_unref(m_audio);
        gst_caps_unref(m_subs);

    }

    bool lock()
    {
        m_mutex.lock();
        return true;
    }

    void unlock()
    {
        m_mutex.unlock();
    }

    virtual demuxInfo& operator=(const demuxInfo &other)
    {
        // copy the other vector adding ref
        for(auto each=other.m_demuxSrcs.begin();each!=other.m_demuxSrcs.end();each++)
        {
            GstCaps *thisOne=each->second;
            std::string thatOne=each->first;
            gst_caps_ref(thisOne);
            m_demuxSrcs.push_back(std::pair<std::string,GstCaps*>(thatOne, thisOne));
        }
        m_binDuration=other.m_binDuration;

        gst_caps_ref(m_video=other.m_video);
        gst_caps_ref(m_audio=other.m_audio);
        gst_caps_ref(m_subs=other.m_subs);

        return *this;
    }

    bool isEmpty() { return m_demuxSrcs.size()==0; }

    unsigned numSrcStreams() { return m_demuxSrcs.size(); }
    unsigned numVideoStreams() { return countIntersects(m_video); }
    unsigned numAudioStreams() { return countIntersects(m_audio); }
    unsigned numSubtitleStreams() { return countIntersects(m_subs); }

    void addStream(GstCaps*streamCaps)
    {
        if(gst_caps_can_intersect(streamCaps,m_video))
        {
            m_demuxSrcs.push_back(std::pair<std::string,GstCaps*>("video_%u",gst_caps_ref(streamCaps)));
        }
        else if(gst_caps_can_intersect(streamCaps,m_audio))
        {
            m_demuxSrcs.push_back(std::pair<std::string,GstCaps*>("audio_%u",gst_caps_ref(streamCaps)));
        } 
        else if(gst_caps_can_intersect(streamCaps,m_subs))
        {
            m_demuxSrcs.push_back(std::pair<std::string,GstCaps*>("subtitle_%u",gst_caps_ref(streamCaps)));
        }
    }

    GstClockTime muxerDuration() { return m_binDuration; }

    std::vector<std::pair<std::string,GstCaps*>> m_demuxSrcs;

protected:
    unsigned countTheseCaps(const char *caps)
    {
        GstCaps *videoCaps=gst_caps_new_simple (caps,NULL,NULL);
        unsigned count=countIntersects(videoCaps); 
        gst_caps_unref(videoCaps);
        return count;
    }


    unsigned countIntersects(GstCaps *with)
    {
        unsigned count=0;
        for(auto each=m_demuxSrcs.begin();each!=m_demuxSrcs.end();each++)
        {
            if(gst_caps_can_intersect (each->second,with))
                count++;
        }
        return count;
    }

    gint64 m_binDuration;

};

#endif // _DEMUXINFO