#pragma once

#include "GpioOutput.h"
#include "PwmController.h"

class MotorController {
public:
    explicit MotorController(PwmController& pwm);
    ~MotorController();

    MotorController(const MotorController&) = delete;
    MotorController& operator=(const MotorController&) = delete;

    void drive(double speedPercent);
    void stop();
    double commandPercent() const noexcept;

private:
    void setSpeed(double speedPercent);

    PwmController& pwm_;
    GpioOutput motor1Direction_;
    GpioOutput motor2Direction_;

    static const int PWM_LEFT_CHANNEL = 13;
    static const int PWM_RIGHT_CHANNEL = 12;

    double commandPercent_ = 0.0;
};
