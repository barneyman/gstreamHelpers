#include "gstreamPipeline.h"
#include <unistd.h>
#include <stdio.h>

// generic
template <class pipelineType>
int gstreamPipelineBase<pipelineType>::m_initCount=0;

// concrete
template int gstreamPipelineBase<GstElement>::m_initCount;


