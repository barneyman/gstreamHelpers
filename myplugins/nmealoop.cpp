#include "nmealoop.h"
#include "nmeaparse/NMEAParser.h"
#include "nmeaparse/GPSService.h"
#include "json/json.hpp"
#include <fcntl.h>


void* gpsMonitorThreadEntry(void *arg);
void pushJson(nmea_threadInfo *filter, nlohmann::json &jsonData);

void startMonitorThread(struct nmea_threadInfo &filter)
{
  // put something in
  nlohmann::json jsonData;
  jsonData["msg"]="Starting Thread";
  pushJson(&filter,jsonData);

  filter.gpsPolling=true;
  pthread_create(&filter.gpsThreadId,NULL,&gpsMonitorThreadEntry,&filter);

}

void stopMonitorThread(struct nmea_threadInfo &filter)
{
    filter.gpsPolling=false;
   
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

// thread that looks after the GPS details
void* gpsMonitorThreadEntry(void *arg)
{

  nmea_threadInfo *filter=(nmea_threadInfo*)arg;

  nmea::NMEAParser parser;
  nmea::GPSService gps(parser);

  filter->gpsPolling=true;

  // requires enable_uart=1 in config.txt
  FILE* gpsFeed = fopen ("/dev/serial0", "r"); 

  nlohmann::json jsonData;

  if(!gpsFeed)
  {
    jsonData["msg"]="No GPS Serial Feed";

    pushJson(filter,jsonData);

  }
  else
  {

    while(filter->gpsPolling)
    {

      char lineBuffer[255];
      memset(&lineBuffer,0,sizeof(lineBuffer));

      // nmea::NMEAParser parser;
      // nmea::GPSService gps(parser);

      // parse it

      if(gpsFeed)
      {

        while(fgets(lineBuffer,sizeof(lineBuffer)-1,gpsFeed) && filter->gpsPolling)
        {


            jsonData.clear();

            try
            {
              parser.readLine(lineBuffer);
            }
            catch(const std::exception& e)
            {
              continue;
            }


            // build the json
            jsonData["gpsLocked"]=gps.fix.locked();
            if(gps.fix.locked())
            {
              
              jsonData["speedKMH"]=(int)gps.fix.speed;
              jsonData["altitudeM"]=(int)gps.fix.altitude;
              jsonData["latitudeN"]=((int)(gps.fix.latitude*10000))/10000.0;
              jsonData["longitudeE"]=((int)(gps.fix.longitude*10000))/10000.0;
              jsonData["bearingDeg"]=(int)gps.fix.travelAngle;
              jsonData["satelliteCount"]=gps.fix.trackingSatellites;
              
              char timebuf[25];
              snprintf(timebuf,sizeof(timebuf)-1, "%d-%02d-%02dT%02d:%02d:%02dZ",
                gps.fix.timestamp.year,
                gps.fix.timestamp.month,
                gps.fix.timestamp.day,
                gps.fix.timestamp.hour,
                gps.fix.timestamp.min,
                (int)gps.fix.timestamp.sec);

              jsonData["utc"]=timebuf;

            }
            else
            {
              jsonData["msg"]="No GPS Lock";
            }

            pushJson(filter,jsonData);
        }
      }
    }
  }


  return NULL;

}
