#pragma once

#include <linux/gpio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string>
#include <stdexcept>


class GpioOutput {
public:
    explicit GpioOutput(unsigned int offset, bool inoutFlag);
    ~GpioOutput();

    GpioOutput(const GpioOutput&) = delete;
    GpioOutput& operator=(const GpioOutput&) = delete;

    void setLevel(bool value);

private:
    int chipFd_ = -1;
    int lineFd_ = -1;
    bool outputMode_{false};
    std::string consumer{ "GpioOutput" };
};
