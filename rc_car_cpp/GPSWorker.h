#pragma once

#include "UartDevice.h"

#include <atomic>
#include <mutex>


extern UartDevice::gpsdata latestGps;
extern std::mutex gpsMutex;


void gpsWorker(
    UartDevice& gps,
    std::atomic<bool>& running
);