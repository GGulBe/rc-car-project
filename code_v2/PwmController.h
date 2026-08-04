#pragma once

#include "I2CDevice.h"

#include <array>
#include <cstdint>

class PwmController {
public:
    explicit PwmController(I2cDevice& i2c);

    void configureServoTimer();
    void configureMotorTimer();

    void setDutyPercent(int channel, double percent);
    void setChannelValue(int channel, std::uint16_t value);

private:

    void setTimer(int timer, std::uint16_t prescalerRegisterValue, std::uint16_t period);
    I2cDevice& i2c_;
    std::array<std::uint16_t, 7> periods_{};
};

