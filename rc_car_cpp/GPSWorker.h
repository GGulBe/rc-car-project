#pragma once

#include "UartDevice.h"
#include "PhoneGpsReceiver.h"
#include <atomic>
#include <mutex>


extern UartDevice::gpsdata latestGps;
extern std::mutex gpsMutex;


void gpsWorker(
    PhoneGpsReceiver& gps,
    std::atomic<bool>& running
);
