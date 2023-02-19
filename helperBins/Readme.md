# Helperbins #
a bunch of bins that aggregate elements to be easier to link

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
`### gstH264MuxOutBin ###
`Creates an h264 `gstMuxOutBin`, trying various encoders via `gstH264encoderBin`
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

