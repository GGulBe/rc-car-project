// imu 헤더파일
#pragma once

#include "I2CDevice.h"

#include <cstddef>
#include <cstdint>

class Bno055 {
public:
    struct Tilt
    {
        double headingDeg; // 회전
        double rollDeg;   // 좌우 기울기
        double pitchDeg;  // 앞뒤 기울기
    };
    

    explicit Bno055(I2cDevice& i2c);

    Tilt readMotion() const;

private: 
    static std::int16_t decodeInt16(const std::uint8_t* data);
    void initialize();
    I2cDevice& i2c_;
    
    static const std::uint8_t REG_EULER = 0x1A;         // EULER 사용 0x1A  Heading LSB 0x1B  Heading MSB 0x1C  Roll LSB 0x1D  Roll MSB 0x1E  Pitch LSB 0x1F  Pitch MSB
    static const std::uint8_t REG_CHIP_ID = 0x00;       // 칩 레지스터 주소
    static const std::uint8_t REG_UNIT_SEL = 0x3B;      // 데이터 출력 형식 지정 주소
    static const std::uint8_t REG_OPR_MODE = 0x3D;      // 동작 모드 설정 주소
    static const std::uint8_t REG_PWR_MODE = 0x3E;      // 전력 모드 설정 주소
    static const std::uint8_t REG_PAGE_ID = 0x07;
    
    static const std::uint8_t CHIP_ID_VALUE = 0xA0;     // 우리가 쓰는 BNO055 칩 고유 id 값
    static const std::uint8_t MODE_CONFIG = 0x00;       // 설정 변경 되는 config 값
    static const std::uint8_t MODE_IMU = 0x08;          // imu 센서 융합 모드 값
};
