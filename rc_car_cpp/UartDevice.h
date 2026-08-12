#pragma once
#include <string>
#include <cstddef>
#include <cstdint>
#include <string>
class UartDevice
{
public:
    struct gps
    {
        bool gpsfix;//위성이 잡혔는지 확인하는 변수
        double lat;
        double lon;
        std::string utc;
        std::string date;
    };
    
    explicit UartDevice(const std::string& devicePath = "/dev/serial0");
    ~UartDevice();
    std::string readRmc()
    std::string readLine();
    gps parseRmc(const std::string& line);
private:
    int fd_ = -1;
    void configure();

};