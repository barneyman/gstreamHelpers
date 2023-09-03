#include "nmealoop.h"
#include "../json/json.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>


void* gpsMonitorThreadEntry(void *arg);
void* gpsdMonitorThreadEntry(void *arg);

void pushJson(nmea_threadInfo *filter, nlohmann::json &jsonData);

void startMonitorThread(struct nmea_threadInfo &filter)
{
  // put something in
  nlohmann::json jsonData;
  jsonData["msg"]="Starting Thread";
  pushJson(&filter,jsonData);
  filter.gpsPollingStopped=false;
  //pthread_create(&filter.gpsThreadId,NULL,&gpsMonitorThreadEntry,&filter);
  pthread_create(&filter.gpsThreadId,NULL,&gpsdMonitorThreadEntry,&filter);

}

void stopMonitorThread(struct nmea_threadInfo &filter)
{
    filter.gpsPolling=false;
    while(!filter.gpsPollingStopped)
    {
        usleep(10*1000);
    }
   
}

void pushJson(nmea_threadInfo *filter, nlohmann::json &jsonData)
{

    // get the mutex
    {
      std::lock_guard<std::mutex> guard(filter->gpsMutex);

      // dump the string
      filter->sample.gpsOutput=jsonData.dump();
      filter->sample.pts=GST_CLOCK_TIME_NONE;

    // release the mutex
    }

}

#include <libgpsmm.h>

void* gpsdMonitorThreadEntry(void *arg)
{

  nmea_threadInfo *filter=(nmea_threadInfo*)arg;

  gpsmm gps_rec("burner", DEFAULT_GPSD_PORT);

  if (gps_rec.stream(WATCH_ENABLE|WATCH_JSON) == NULL) 
  {
      return NULL;
  }

  filter->gpsPolling=true;


  while(filter->gpsPolling)
  {
    struct gps_data_t *gps_data;


    nlohmann::json jsonData;

    if (!gps_rec.waiting(50000000))
    {
        continue;
    }

    if ((gps_data = gps_rec.read()) == NULL) 
    {
        continue;
    } 
    else 
    {
      //PROCESS(newdata);
      switch(gps_data->fix.mode)
      {
        case MODE_NOT_SEEN:
            jsonData["msg"]="no sky";
            break;
        case MODE_NO_FIX:
            jsonData["msg"]="not fixed";
            break;
        case MODE_3D:
            if(gps_data->set & ALTITUDE_SET)
                jsonData["altitudeM"]=gps_data->fix.altitude;
            if(gps_data->set & CLIMB_SET)
                jsonData["climb"]=gps_data->fix.climb;
        case MODE_2D:
            
            if(gps_data->set & LATLON_SET)
            {
                jsonData["longitudeE"]=trunc(gps_data->fix.longitude*10000)/10000.0;
                jsonData["latitudeN"]=trunc(gps_data->fix.latitude*10000)/10000.0;
            }
            if(gps_data->set & SATELLITE_SET)
                jsonData["satelliteCount"]=gps_data->satellites_visible;

            if(gps_data->set & TRACK_SET)
                jsonData["bearingDeg"]=(int)(gps_data->fix.track);
            if(gps_data->set & SPEED_SET)
                jsonData["speedKMH"]=(int)(gps_data->fix.speed/1000);
            break;


        }
        pushJson(filter,jsonData);
        gps_data->set=0;
      }

  }

  filter->gpsPollingStopped=true;

  return NULL;

}

