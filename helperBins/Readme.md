# Helperbins #
a bunch of bins that aggregate elements to be easier to link - the basic idea is that they do all the hard work of plumbing and negotiating pads and you can just slam them together ... for example, this code will join the 6 mp4 sources, decode them, re-encode them and write them to combined.mp4 

```
class joinVidsPipeline : public gstreamPipeline
{
public:
    joinVidsPipeline(std::vector<std::string> &files, const char*destination):
        gstreamPipeline("joinVidsPipeline"),
        m_multisrc(this,files),
        m_out(this,destination)
    {
        ConnectPipeline(m_multisrc,m_out);
    }

protected:

        gstMP4DemuxDecodeSparseBin m_multisrc;
        gstMP4OutBin m_out;

};

void main()
{
    std::vector<std::string> files;
    
    files.push_back("/workspaces/dashcam/out_00000.mp4");
    files.push_back("/workspaces/dashcam/out_00001.mp4");
    files.push_back("/workspaces/dashcam/out_00002.mp4");
    files.push_back("/workspaces/dashcam/out_00003.mp4");
    files.push_back("/workspaces/dashcam/out_00004.mp4");
    files.push_back("/workspaces/dashcam/out_00005.mp4");

    joinVidsPipeline joiner(files,"/workspaces/dashcam/combined.mp4");
    joiner.Run();

}

```
Creates the following pipeline

![Complex Pipeline](https://github.com/barneyman/gstreamHelpers/blob/master/helperBins/twinStreamsMultiSrcJoin.svg)

That may not render well in Github - use `graphviz` to render the dot file.

The observant will notice the sources have two video streams and only one is being decoded and re-encoded; I'll fix that with an additional helperbin at some point



## myDemuxBins ##
### `demuxInfo` ###
Used by `gstDemuxDecodeBinExamine` to cache demux information - i.e. what media streams does this file have.
### `gstDemuxDecodeBinExamine` ###
Examines a muxed file to work out wehat streams it has - useful if you're continually re-encoding the same file 'types' and you don't want the 2-3 second overhead of re-examining the files
### `gstreamDemuxExamineDiscrete` ###
As above, but lives on its own ephemeral pipeline 
### `gstDemuxDecodeBin` ###
Generic bin that demuxes a muxed file. Uses `gstDemuxDecodeBinExamine` to work out what's in the muxed file, pre-prepares the pads and routes them through a `multiqueue`.  
### `gstMKVDemuxDecodeBin` ###
As above, but defaults to `matroskademux`
### `gstMP4DemuxDecodeBin` ###
As above, but defaults to `qtdemux`
### `gstMP4DemuxDecodeSparseBin` ###
As above, but defaults to `splitmuxsrc`
### `gstDemuxSubsOnlyDecodeBin` ###
As above, but only consumes subtitle streams
## myElementBins ##
### `gstIdentityBin` ###
wraps `identity`
### `gstSleepBin` ###
uses `identity` to set `sleep-time`
### `gstTimeOffsetBin` ###
uses `identity` to set `ts-offset`
### `gstFrameBufferProgress` ###
wraps `progressreport`
### `gstQueue` ###
wraps `queue`
### `gstQueue2` ##
wraps `queue2`
### `gstMultiQueueBin` ###
wraps `multiqueue`
### `gstMultiQueueWithTailBin` ###
uses `gstMultiQueueBin` to do something very cool, but I don't recall what - used by the demux bins
### `gstCapsFilterSimple` ###
creates a `capsfilter` element from a string
### `gstFakeSinkForCapsSimple` ###
creates a `fakesink` for specific simple caps
### `gstTeeBin` ###
not sure what this does
### `gstFrameRateBin` ###
wraps `framerate`
### `gstNamedVideo` ###
wraps `textoverlay` and allows you to place some static overlay
### `gstVideoScaleBin` ###
wraps `videoscale`
### `gstVideoMixerBin` ###
makes compositing easier ?!
### `gstH264encoderBin` ###
Tries a number of x264 encoders
### `gstTestSourceBin` ###
wraps `videotestsrc`
### `gstFilesrc` ###
wraps `filesrc`
## myMuxBins ##
### `gstMuxOutBin` ###
joins a mux to a `filesink` or a `FILE` handle
### `gstSplitMuxOutBin` ###
As above but to a `splitmuxsink`
### gstH264MuxOutBin ###
Creates an h264 `gstMuxOutBin`, trying various encoders via `gstH264encoderBin`
### `gstMatroskaOutBin` ###
Uses above and uses `matroskamux`
### `gstMP4OutBin` ###
As above but uses `mp4mux`
## remoteSourceBins ##
### `rtspSourceBin` ###
Creates elements to consume an RTSP stream
### `rtmpSourceBin` ###
Creates elements to consume an RTMP stream
### `multiRemoteSourceBin` ###
Allows the muxing of a number of remote streams

