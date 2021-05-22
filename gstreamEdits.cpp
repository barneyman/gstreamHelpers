#include "gstreamEdits.h"

#define _USE_MULTIQ

#ifdef _GESSRC_NOT_AVAILABLE
#define _TIMELINE_Q "multiqueue"
#else
#define _TIMELINE_Q "gessrc"
#endif
#define _TIMELINE_Q_NAME  "timelineEgress"


gstreamEditor::gstreamEditor(const char *pipelineName, unsigned numTracks, bool autoTransitions):
#ifdef _USE_TIMELINE_AS_GSTELEMENT
  gstreamPipeline()
#else
  gstreamPipelineBase<GESPipeline>()
#endif  
{
  
  initialiseGstreamer();

  // init ges, after base class has initialised gst
  ges_init();

  // turn omxdec off
  DowngradeCodecs();

  // get me a timeline
  m_timeline = ges_timeline_new ();

  if(!m_timeline)
  {
    GST_ERROR_OBJECT (m_pipeline, "NO TIMELINE\n");
  }

  // enable transitions
  ges_timeline_set_auto_transition(m_timeline, autoTransitions?TRUE:FALSE);


#ifdef _USE_TIMELINE_AS_GSTELEMENT
  // create a pipeline
  m_pipeline=gst_pipeline_new (pipelineName);

  // get to the bus
  m_bus = gst_element_get_bus (m_pipeline);

  // subscribe to signals
  // gst_bus_add_signal_watch (m_bus);
  // g_signal_connect (G_OBJECT (m_bus), "autoplug-continue", (GCallback)staticAutoplugHandler, this);



  // add a multiqueue, which we will use to interface between the timeline and the onward sink
  AddPlugin(_TIMELINE_Q,_TIMELINE_Q_NAME);

/*
  g_object_set (FindNamedPlugin(_TIMELINE_Q_NAME),
      "max-size-buffers", (guint64) 20,
      "max-size-bytes", (guint64) 0,
      "max-size-time", (guint64) 0, 
      "use-buffering", TRUE,
      NULL
      );
*/


#else

  m_pipeline = ges_pipeline_new();
  // get to the bus
  m_bus = gst_element_get_bus (GST_ELEMENT (m_pipeline));

#endif


    // add a layer
    AddVisibleLayers(numTracks);
    
} 

gstreamEditor::~gstreamEditor()
{
  RestoreRanks();
  ges_deinit();
}

gboolean gstreamEditor::autoplugHandler(GstElement *bin,GstPad *pad,GstCaps *caps)
{
    return FALSE;
}


// at presentationTimeSecs in the timeline, play durectionSecs of the source, seeking to seekTimeSecs first
// if presentationTimeIsRelative==true, 0 snaps to the end of the layer, -ve creates a transition, + creates a gap
// false, it's up to you
bool gstreamEditor::AddClipToLayer(const char *sourceFile, int durationSecs, int seekTimeSecs, int presentationTimeMilliSecs,bool presentationTimeIsRelative, int priority, unsigned layerNum)
{
  gchar *uri = gst_filename_to_uri (sourceFile, NULL);
  GESClip *ret = GES_CLIP (ges_uri_clip_new (uri));

  GstClockTime clipDuration, maxDuration;
  g_object_get(ret, "duration", &clipDuration, "max-duration", &maxDuration, NULL);

  if(durationSecs==-1)
  {
    clipDuration=maxDuration-(seekTimeSecs*GST_SECOND);
  }
  else
  {
    clipDuration=durationSecs*GST_SECOND;
  }

  GstClockTime actualPresentation;  

  if(presentationTimeIsRelative)
  {
    GstClockTime layerDuration=ges_layer_get_duration(m_layers[layerNum]); 
    actualPresentation=layerDuration;
    // sanity check
    if(presentationTimeMilliSecs<0)
    {
      // don't seek before the start of the layer
      if((abs(presentationTimeMilliSecs)*GST_MSECOND) < layerDuration)
      {
        actualPresentation-=(abs(presentationTimeMilliSecs)*GST_MSECOND);  
      }
    }
    else
    {
      if(layerDuration)
        actualPresentation+=(presentationTimeMilliSecs*GST_MSECOND);
    }
  }
  else
  {
    actualPresentation=presentationTimeMilliSecs*GST_MSECOND;    
  }

  // by default, for transitions, the prior is more important
  if(priority==-1)    
    priority=layerNum;

  g_object_set (ret,
      "start", (guint64) actualPresentation,
      "duration", (guint64) clipDuration,
      "priority", (guint32) priority, 
      "in-point", (guint64) seekTimeSecs*GST_SECOND, 
      NULL
      );


  bool retadd=ges_layer_add_clip (m_layers[layerNum], ret);

  g_free(uri);

  return retadd;

}



bool gstreamEditor::AddAssetToLayer(const char *sourceFile, unsigned layerNum)
{
  gchar *uri = gst_filename_to_uri (sourceFile, NULL);

  GError *err=NULL;

  // 
  GESUriClipAsset  *newAsset=ges_uri_clip_asset_request_sync (uri, &err);

  if(!newAsset)
  {
    GST_ERROR_OBJECT (m_pipeline, "Adding asset %s failed\n", uri);
    return false;
  }

  // GstDiscovererInfo *info = ges_uri_clip_asset_get_info (newAsset);
  // GstEncodingProfile *profile = make_profile_from_info (info);
  // gchar *encName=gst_encoding_profile_get_name (profile);

  guint64 duration = ges_uri_clip_asset_get_duration (newAsset);

  ges_layer_add_asset (m_layers[layerNum],
      GES_ASSET (newAsset),
      ges_timeline_get_duration (m_timeline),
      0, duration, ges_clip_asset_get_supported_formats (GES_CLIP_ASSET (newAsset)));

  return true;

}


GstEncodingProfile *gstreamEditor::create_my_profile(void)
{
 GstEncodingContainerProfile *prof;
 GstCaps *caps;

 caps = gst_caps_from_string("video/quicktime");
 //caps = gst_caps_from_string("video/x-matroska");
 prof = gst_encoding_container_profile_new(NULL,
    NULL,
    caps, NULL);
 gst_caps_unref (caps);

 caps = gst_caps_from_string("video/x-h264");
 bool addResult=gst_encoding_container_profile_add_profile(prof,
      (GstEncodingProfile*) gst_encoding_video_profile_new(caps, NULL, NULL, 0));
 gst_caps_unref (caps);

//  caps = gst_caps_from_string("audio/mpeg");
//  gst_encoding_container_profile_add_profile(prof,
//       (GstEncodingProfile*) gst_encoding_video_profile_new(caps, NULL, NULL, 0));
//  gst_caps_unref (caps);


 return (GstEncodingProfile*) prof;
}


#ifdef _USE_TIMELINE_AS_GSTELEMENT
bool gstreamEditor::Run(const char *sink)
#else
bool gstreamEditor::Run()
#endif
{

  CommitTimeline();

  DumpGraph("1 - about to edit");

#ifdef _USE_TIMELINE_AS_GSTELEMENT

  // add the timeline (as a bin) to the pipeline
  if(!gst_bin_add(GST_BIN (m_pipeline),GST_ELEMENT(m_timeline)))
  {
    GST_ERROR_OBJECT (m_pipeline, "Failed to add timeline to pipeline\n");
  }

  if(!sink)
  {
    sink="fakesink";
    // add a fake sink
    if(AddPlugin(sink))
    {
      GST_ERROR_OBJECT (m_pipeline, "failed to add fakesink\n");
      return false;
    }
  }

  // connect each track to (my) timeline egress point
  for(auto eachTrack=m_allTracks.begin();eachTrack!=m_allTracks.end();eachTrack++)
  {
    JoinToGESOutputToEgress(*eachTrack);
  }

  // connect my timeline egress point to the rest of my pipeline
  ConnectToTimeline(sink,FALSE,true);

  DumpGraph("2 - timeline set");

#else

  ges_pipeline_set_timeline ((GESPipeline*)m_pipeline, m_timeline);
  DumpGraph("2 - timeline set");

  ges_pipeline_set_render_settings ((GESPipeline*)m_pipeline, "file:///home/pi/source/gstreamToy/edited.mp4", create_my_profile());
  ges_pipeline_set_mode((GESPipeline*)m_pipeline, GES_PIPELINE_MODE_SMART_RENDER );

  DumpGraph("3 - urisink set");

#endif

    Ready();

    if(!PreRoll())
    {
      GST_ERROR_OBJECT (m_pipeline, "Failed to preroll\n");
      DumpGraph("2a - failed pause");
      return false;
    }

  if(!Play())
  {
    DumpGraph("2b - failed play");
    GST_ERROR_OBJECT (m_pipeline, "Failed to play\n");
    return false;
  }

  DumpGraph("3 - editing");

  internalRun();

  return true;

}

#ifdef _TRY_HANDLING_SUBTITLES
bool gstreamEditor::AddTextTrack(GstCaps *caps)
{
  GESTrack *textTrack=GES_TRACK(ges_track_new(GES_TRACK_TYPE_CUSTOM ,caps));

  return internalAddTrack(textTrack);

}
#endif

bool gstreamEditor::AddVideoOutputTrack()
{
  GESTrack *trackv = GES_TRACK (ges_video_track_new());

  return internalAddTrack(trackv);
  
}

bool gstreamEditor::AddAudioOutputTrack()
{
  GESTrack *tracka = GES_TRACK (ges_audio_track_new ());
  return internalAddTrack(tracka);
}

bool gstreamEditor::internalAddTrack(GESTrack *newTrack)
{
  if(!newTrack)
  {
    GST_ERROR_OBJECT (m_pipeline, "Failed to create track\n");
    return false;
  }

  if(!ges_timeline_add_track (m_timeline, newTrack))
  {
    GST_ERROR_OBJECT (m_pipeline, "Failed to add track\n");
    return false;
  }

  m_allTracks.push_back(newTrack);

  return true;

}



#ifdef _GESSRC_NOT_AVAILABLE
#define _SINK_NAME_BUFFER_SIZE  25

// connect this track to the egress point i added in the ctor
bool gstreamEditor::JoinToGESOutputToEgress(GESTrack*newTrack)
{

  GST_LOG_OBJECT (m_pipeline, "JoinToGESOutputToEgress called\n");

  // get the src pad for this track
  GstPad *trackPad=ges_timeline_get_pad_for_track (m_timeline, newTrack);


  // get tracke caps
  const GstCaps*trackCaps=ges_track_get_caps(newTrack);
  GST_INFO_OBJECT (m_pipeline, "Track caps are '%s'",gst_caps_to_string(trackCaps));
  // depending on the caps of the track
  GstPadTemplate *sinkTemplate=gst_pad_template_new("sink_%d",GST_PAD_SINK,GST_PAD_REQUEST,gst_caps_copy(trackCaps));

  // reqeust a sink
  GstPad *sinkPad=gst_element_request_pad(FindNamedPlugin(_TIMELINE_Q_NAME),sinkTemplate,NULL,NULL);

  if(!sinkPad)
  {
    GST_ERROR_OBJECT (m_pipeline, "failed to create sinkpad\n");
    return false;
  }

  GST_INFO_OBJECT (m_pipeline, "Added pad '%s'",gst_element_get_name((GstElement*)sinkPad));
  gboolean linkresult=gst_pad_link_maybe_ghosting (trackPad, sinkPad);

  if(!linkresult)
    GST_ERROR_OBJECT (m_pipeline, "linking track to %s failed\n","egress");

  return true;

}
#endif

// todo
bool gstreamEditor::AddVisibleLayers(unsigned numLayers)
{
  for(unsigned each=0;each<numLayers;each++)
  {
    GESLayer *newLayer=GES_LAYER (ges_layer_new ());
    g_object_set (newLayer, "priority", (gint32) each, NULL);
    if(ges_timeline_add_layer (m_timeline, newLayer))
    {
      m_layers.push_back(newLayer);
    }
  }

  return true;

}

// these don't play well in the decode/encode bins inside GES
bool gstreamEditor::DowngradeCodecs()
{
  // this won't negotiate caps
  if(!DowngradeSpecific("omxh264dec",GST_RANK_PRIMARY - 1))
     return false;

  // fails to allocate a buffer
  if(!DowngradeSpecific("v4l2h264dec",GST_RANK_PRIMARY - 1))
    return false;

  // if(!DowngradeSpecific("omxh264enc",GST_RANK_PRIMARY - 1))
  //   return false;

  return true;
}

void gstreamEditor::RestoreRanks()
{
  for(auto each=m_preservedRanks.begin();each!=m_preservedRanks.end();each++)
  {
    DowngradeSpecific(each->first.c_str(),each->second, false);
  }
}


// the omxdec on rpi doesn't play nice, so take it off the board
bool gstreamEditor::DowngradeSpecific(const char *name, int rank, bool store)
{
  GstRegistry* reg = gst_registry_get();
  if(!reg)
    return false;

  GstPluginFeature* _decode = gst_registry_lookup_feature(reg, name);

  if(_decode == NULL) {
      return false;
  }
 

  // store its current rank, so we can restoreit later
  if(store)
  {
    m_preservedRanks.push_back(std::pair<std::string,int>(std::string(name), gst_plugin_feature_get_rank(_decode)));
  }

  gst_plugin_feature_set_rank(_decode, rank);

  gst_object_unref(_decode);  

  return true;
}



#ifdef _USE_TIMELINE_AS_GSTELEMENT


 
bool gstreamEditor::ConnectToTimeline(const char *firstSink, const char*viaSink, bool forceUpstreamReconfig)
{
  ConnectPipeline(_TIMELINE_Q_NAME, firstSink, viaSink);

  if(forceUpstreamReconfig)
  {
    GstEvent *reconfig=gst_event_new_reconfigure();
    gst_element_send_event (FindNamedPlugin(_TIMELINE_Q_NAME),reconfig);
  }

  return true;
}

#endif


