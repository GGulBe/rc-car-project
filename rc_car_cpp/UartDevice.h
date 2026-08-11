#pragma once
#include <string>
#include <cstddef>
#include <cstdint>
class UartDevice
{
public:
    explicit UartDevice(const std::string& devicePath = "/dev/serial0");
    ~UartDevice();

    std::string readLine();

private:
    int fd_ = -1;
    void configure();

};