#include "gstreamPipeline.h"
#include <vector>
#include <ges/ges.h>


#if 1 //!GST_CHECK_VERSION(1,16,1)
#define _GESSRC_NOT_AVAILABLE
#else
#define _GESSRC_AVAILABLE
#endif

// ignore the GESPipeline, and use timeline as an element
#define _USE_TIMELINE_AS_GSTELEMENT

#define _TRY_HANDLING_SUBTITLES



class gstreamEditor : 
#ifdef _USE_TIMELINE_AS_GSTELEMENT
    public gstreamPipeline
#else
    public gstreamPipelineBase<GESPipeline>
#endif    
{

    public:

    gstreamEditor(const char *pipelineName, unsigned numTracks=1, bool autoTransitions=false);
    ~gstreamEditor();

    bool AddClipToLayer(const char *sourceFile, int durectionSecs=-1, int seekTime=0,  int presentationTimeMilliSecs=0,bool presentationTimeIsRelative=true, int priority=-1, unsigned layerNum=0);
    bool AddAssetToLayer(const char *sourceFile, unsigned layerNum=0);

    bool AddVideoOutputTrack();
    bool AddAudioOutputTrack();

#ifdef _TRY_HANDLING_SUBTITLES    
    bool AddPangoTextTrack() { return AddTextTrack(gst_caps_new_simple ("text/x-raw","format", G_TYPE_STRING, "pango-markup",NULL)); }
    bool AddUTF8TextTrack() { return AddTextTrack(gst_caps_new_simple ("text/x-raw","format", G_TYPE_STRING, "utf8",NULL)); }
    bool AddTextTrack(GstCaps *);
#endif    


    bool AddVisibleLayers(unsigned);
#ifdef _USE_TIMELINE_AS_GSTELEMENT
    bool ConnectToTimeline(const char *firstSink, const char*viaSink=NULL, bool forceUpstreamReconfig=false);

    bool Run(const char *sink=NULL);
#else
    bool Run();
#endif    

    void CommitTimeline()
    {
        ges_timeline_commit (m_timeline);
    }

    protected:

    static gboolean staticAutoplugHandler(GstElement *bin,GstPad *pad,GstCaps *caps,gpointer *udata)
    {
        return ((gstreamEditor*)udata)->autoplugHandler(bin,pad,caps);
    }

    virtual gboolean autoplugHandler(GstElement *bin,GstPad *pad,GstCaps *caps);

    GstEncodingProfile *create_my_profile(void);
    bool DowngradeSpecific(const char *name, int rank, bool store=true);
    bool DowngradeCodecs();
    void RestoreRanks();

    bool internalAddTrack(GESTrack *newTrack);

    // does the heavy lifting
     GESTimeline *m_timeline;


    bool JoinToGESOutputToEgress(GESTrack*newTrack);

#ifdef _CHAINS
    // don't remember what this was for
    class outputChain
    {
        GESTrack *m_track;
        GstPad *m_sourcePad;
    };

    std::vector<gstreamEditor::outputChain> m_chains;
#endif    

    std::vector<GESTrack*> m_allTracks;
    std::vector<GESLayer*> m_layers;
    std::vector<std::pair<std::string,int> > m_preservedRanks;

};