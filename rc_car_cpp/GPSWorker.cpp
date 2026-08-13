#include "GPSWorker.h"


UartDevice::gpsdata latestGps;
std::mutex gpsMutex;


void gpsWorker(UartDevice& gps, std::atomic<bool>& running)
{
    while (running.load()) {

        UartDevice::gpsdata gpsdata = gps.parseRmc();

        {
            std::lock_guard<std::mutex> lock(gpsMutex);

            latestGps = gpsdata;
        }
    }
}