#include "ServoController.h"
#include "PwmController.h"

ServoController::ServoController(PwmController& pwm) : pwm_(pwm) {
    pwm_.configureServoTimer();
}

void ServoController::setCalibration(int channel, const ServoCalibration& calibration) {
    validateChannel(channel);
    calibration_.at(static_cast<std::size_t>(channel)) = calibration;
}

void ServoController::setAngle(int channel, double requestedAngle) {
    validateChannel(channel);
    const ServoCalibration& c = calibration_.at(static_cast<std::size_t>(channel));
    requestedAngle = std::clamp(requestedAngle, c.minAngle, c.maxAngle);
    double physicalAngle = (c.reversed ? -requestedAngle : requestedAngle) + c.centerOffset;
    physicalAngle = std::clamp(physicalAngle, -90.0, 90.0);
    const double pulseUs = c.minPulseUs + (physicalAngle + 90.0) * (c.maxPulseUs - c.minPulseUs) / 180.0;
    const auto pwmValue = static_cast<std::uint16_t>(std::lround(pulseUs * 4095.0 / 20000.0));
    pwm_.setChannelValue(channel, pwmValue);
}

void ServoController::setCenter(int channel) {
    setAngle(channel, 0.0);
}
void ServoController::validateChannel(int channel) {
        if (channel < 0 || channel > 2) throw std::out_of_range("Servo channel must be P0, P1 or P2");
    }



