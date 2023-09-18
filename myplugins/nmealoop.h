#ifndef _NMEALOOP_H
#define _NMEALOOP_H

#include <pthread.h>
#include <string>
#include <gst/gst.h>
#include <mutex>
#include "../json/json.hpp"


  struct nmea_threadInfo
  {
    volatile bool gpsPolling, gpsPollingStopped;
    pthread_t gpsThreadId;
    
    struct {
      nlohmann::json gpsOutput;
      GstClockTime pts;
    } sample;

    unsigned long framesFilled;
    GstClockTime runningTime, frameTimeDelta, firstFrame;

    unsigned frameRate;
    bool usePipelineTime;

    std::mutex gpsMutex;
  } ;


void startMonitorThread(struct nmea_threadInfo&);
void stopMonitorThread(struct nmea_threadInfo&);

#endif