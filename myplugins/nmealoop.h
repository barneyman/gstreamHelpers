#ifndef _NMEALOOP_H
#define _NMEALOOP_H

#include <pthread.h>
#include <string>
#include <gst/gst.h>
#include <mutex>

  struct nmea_threadInfo
  {
    bool gpsPolling;
    pthread_t gpsThreadId;
    
    struct {
      std::string gpsOutput;
      GstClockTime pts;
    } sample;

    unsigned long framesFilled;
    GstClockTime runningTime, frameTimeDelta;

    unsigned frameRate;
    bool useLocalTime;
    int tsoffsetms;

    std::mutex gpsMutex;
  } ;


void startMonitorThread(struct nmea_threadInfo&);
void stopMonitorThread(struct nmea_threadInfo&);

#endif