#include "I2CDevice.h"
#include "PwmController.h"

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

PwmController::PwmController(I2cDevice& i2c) : i2c_(i2c) {}

void PwmController::configureServoTimer() 
{
    setTimer(0, 351, 4095);
}

void PwmController::configureMotorTimer() 
{
    setTimer(3, 847, 849);
}

void PwmController::setDutyPercent(int channel, double percent) {
    if (channel < 0 || channel > 13) throw std::out_of_range("PWM channel must be between 0 and 13");

    const int timer = channel / 4;
    const std::uint16_t period = periods_.at(static_cast<std::size_t>(timer));

    if (period == 0) throw std::runtime_error("PWM timer is not configured");

    percent = std::clamp(percent, 0.0, 100.0);
    const auto value = static_cast<std::uint16_t>(std::lround(static_cast<double>(period) * percent / 100.0));

    setChannelValue(channel, value);
}

void PwmController::setChannelValue(int channel, std::uint16_t value) {
    if (channel < 0 || channel > 13) throw std::out_of_range("PWM channel must be between 0 and 13");

    const int timer = channel / 4;
    const std::uint16_t period = periods_.at(static_cast<std::size_t>(timer));

    if (period == 0) throw std::runtime_error("PWM timer is not configured");

    const std::uint8_t channelRegister = static_cast<std::uint8_t>(0x20 + channel);
    i2c_.writeRegister16(channelRegister, std::min(value, period));
}

void PwmController::setTimer(int timer, std::uint16_t prescalerRegisterValue, std::uint16_t period) {
    if (timer < 0 || timer > 3) throw std::out_of_range("PWM timer must be between 0 and 3");

    const std::uint8_t prescalerRegister = static_cast<std::uint8_t>(0x40 + timer);
    const std::uint8_t periodRegister = static_cast<std::uint8_t>(0x44 + timer);

    i2c_.writeRegister16(periodRegister, period);
    i2c_.writeRegister16(prescalerRegister, prescalerRegisterValue);

    periods_.at(static_cast<std::size_t>(timer)) = period;
}
