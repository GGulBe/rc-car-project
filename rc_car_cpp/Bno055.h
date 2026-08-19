#pragma once

#include "I2CDevice.h"

#include <cstddef>
#include <cstdint>

class Bno055 {
public:
    struct Tilt
    {
        double headingDeg; // 회전 (절대 방위각 0~360도)
        double rollDeg;    // 좌우 기울기
        double pitchDeg;   // 앞뒤 기울기
    };

    explicit Bno055(I2cDevice& i2c);
    
    Tilt readMotion() const;

private: 
    static std::int16_t decodeInt16(const std::uint8_t* data);
    void initialize();
    I2cDevice& i2c_;
    
    static const std::uint8_t REG_EULER     = 0x1A;
    static const std::uint8_t REG_CHIP_ID   = 0x00;
    static const std::uint8_t REG_UNIT_SEL  = 0x3B;
    static const std::uint8_t REG_OPR_MODE  = 0x3D;
    static const std::uint8_t REG_PWR_MODE  = 0x3E;
    static const std::uint8_t REG_PAGE_ID   = 0x07;
    
    static const std::uint8_t CHIP_ID_VALUE = 0xA0;
    static const std::uint8_t MODE_CONFIG   = 0x00;
    static const std::uint8_t MODE_IMU      = 0x0C; // 9축 NDOF 모드 활성화[cite: 1]
};