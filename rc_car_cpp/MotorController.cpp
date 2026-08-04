#include "MotorController.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

MotorController::MotorController(PwmController& pwm)
    : pwm_(pwm), motor1Direction_(23, false), motor2Direction_(24, false) {
    pwm_.configureMotorTimer();
    stop();
}

MotorController::~MotorController() {
    try {
        stop();
    } catch (...) {
    }
}

void MotorController::drive(double speedPercent) {
    setSpeed(speedPercent);
}

void MotorController::stop() {
    setSpeed(0.0);
}

double MotorController::commandPercent() const noexcept {
    return commandPercent_;
}

void MotorController::setSpeed(double speedPercent) {
    speedPercent = std::clamp(speedPercent, -100.0, 100.0);

    if (std::abs(speedPercent) < 0.001) speedPercent = 0.0;

    commandPercent_ = speedPercent;

    if (speedPercent == 0.0) {
        pwm_.setDutyPercent(PWM_LEFT_CHANNEL, 0.0);
        pwm_.setDutyPercent(PWM_RIGHT_CHANNEL, 0.0);
        return;
    }

    const bool forward = speedPercent > 0.0;

    pwm_.setDutyPercent(PWM_LEFT_CHANNEL, 0.0);
    pwm_.setDutyPercent(PWM_RIGHT_CHANNEL, 0.0);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    motor2Direction_.setLevel(forward);
    motor1Direction_.setLevel(!forward);

    const double dutyPercent = std::abs(speedPercent);
    pwm_.setDutyPercent(PWM_LEFT_CHANNEL, dutyPercent);
    pwm_.setDutyPercent(PWM_RIGHT_CHANNEL, dutyPercent);
}
