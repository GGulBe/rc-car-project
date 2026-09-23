#pragma once

#include <string>

#include "UartDevice.h"


class PhoneGpsReceiver
{
public:
    explicit PhoneGpsReceiver(const std::string& phoneIp, int port = 5000);
    ~PhoneGpsReceiver();

    PhoneGpsReceiver(const PhoneGpsReceiver&) = delete;
    PhoneGpsReceiver& operator=(const PhoneGpsReceiver&) = delete;

    std::string readRmc();

    UartDevice::gpsdata parseRmc();

private:
    void waitForClient();

    int serverFd_ = -1;
    int clientFd_ = -1;

    std::string buffer_;
};
