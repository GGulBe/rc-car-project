#pragma once

#include "Bno055.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

inline double calibrateForwardAcceleration(Bno055& imu, int sampleCount = 200) {
    if (sampleCount <= 0) return 0.0;

    double sum = 0.0;
    for (int i = 0; i < sampleCount; ++i) {
        sum += imu.readMotion().forwardAccelerationMps2;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return sum / static_cast<double>(sampleCount);
}

class SpeedEstimator {
public:
    explicit SpeedEstimator(double accelerationBias = 0.0, double maximumSpeedMps = 5.0)
        : accelerationBias_(accelerationBias), maximumSpeedMps_(maximumSpeedMps) {}

    double update(double forwardAccelerationMps2, double yawRateDps, double signedMotorPwmPercent, double deltaTimeSeconds) {
        if (deltaTimeSeconds <= 0.0 || deltaTimeSeconds > 0.2) return speedMps_;

        const double measuredAcceleration = forwardAccelerationMps2 - accelerationBias_;
        filteredAcceleration_ += 0.15 * (measuredAcceleration - filteredAcceleration_);

        const bool motorOff = std::abs(signedMotorPwmPercent) < motorPwmDeadbandPercent_;
        const bool accelerationSmall = std::abs(filteredAcceleration_) < accelerationHoldThresholdMps2_;
        const bool lowMotion = accelerationSmall && std::abs(yawRateDps) < gyroStationaryThreshold_;

        if (motorOff) {
            speedMps_ *= std::exp(-coastDecayPerSecond_ * deltaTimeSeconds);
            stationaryTime_ = lowMotion ? stationaryTime_ + deltaTimeSeconds : 0.0;

            if (stationaryTime_ >= stopConfirmationSeconds_) {
                speedMps_ = 0.0;
                accelerationBias_ += 0.01 * (forwardAccelerationMps2 - accelerationBias_);
            }
        } else {
            stationaryTime_ = 0.0;

            if (!accelerationSmall) speedMps_ += filteredAcceleration_ * deltaTimeSeconds;
        }

        speedMps_ = std::clamp(speedMps_, -maximumSpeedMps_, maximumSpeedMps_);
        return speedMps_;
    }

    double speedMps() const noexcept { return speedMps_; }
    double speedMmps() const noexcept { return speedMps_ * 1000.0; }
    double speedKmh() const noexcept { return std::abs(speedMps_) * 3.6; }

    void reset() noexcept {
        speedMps_ = 0.0;
        filteredAcceleration_ = 0.0;
        stationaryTime_ = 0.0;
    }

private:
    double accelerationBias_ = 0.0;
    double maximumSpeedMps_ = 5.0;
    double speedMps_ = 0.0;
    double filteredAcceleration_ = 0.0;
    double stationaryTime_ = 0.0;

    static constexpr double accelerationHoldThresholdMps2_ = 0.08;
    static constexpr double motorPwmDeadbandPercent_ = 0.5;
    static constexpr double gyroStationaryThreshold_ = 2.0;
    static constexpr double coastDecayPerSecond_ = 1.5;
    static constexpr double stopConfirmationSeconds_ = 0.5;
};
