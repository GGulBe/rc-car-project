#pragma once

#include "PwmController.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>


struct ServoCalibration {
    double minAngle;
    double maxAngle;
    double centerOffset = 0.0;
    double minPulseUs = 500.0;
    double maxPulseUs = 2500.0;
    bool reversed = false;
};

class ServoController {
public:
    explicit ServoController(PwmController& pwm);

    void setCalibration(int channel, const ServoCalibration& calibration);

    void setAngle(int channel, double requestedAngle);

    void setCenter(int channel);

private:
    void validateChannel(int channel);
    PwmController& pwm_;
    std::array<ServoCalibration, 3> calibration_{};
};

