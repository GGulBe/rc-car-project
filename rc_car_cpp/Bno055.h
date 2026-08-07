// imu 헤더파일
#pragma once

#include "I2CDevice.h"

#include <chrono>
#include <cstddef>
#include <cstdint>

class Bno055 {
public:
    struct Tilt
    {
        double rollDeg;   // 좌우 기울기
        double pitchDeg;  // 앞뒤 기울기
    };
    

    explicit Bno055(I2cDevice& i2c);

    void initialize();
    Tilt readMotion() const;

private: 
    static std::int16_t decodeInt16(const std::uint8_t* data);

    I2cDevice& i2c_;
    
    static constexpr std::uint8_t REG_EUL_ROLL_LSB = 0x1C;
    static constexpr std::uint8_t REG_CHIP_ID = 0x00;       // 칩 레지스터 주소
    static constexpr std::uint8_t REG_PAGE_ID = 0x07;       // 레지스터 페이지 전환 주소
    static constexpr std::uint8_t REG_GYR_DATA_X = 0x14;    // 자이로 X축 데이터 시작 주소
    static constexpr std::uint8_t REG_LIA_DATA_X = 0x28;    // 선형 가속도 X축 데이터 시작 주소
    static constexpr std::uint8_t REG_UNIT_SEL = 0x3B;      // 데이터 출력 형식 지정 주소
    static constexpr std::uint8_t REG_OPR_MODE = 0x3D;      // 동작 모드 설정 주소
    static constexpr std::uint8_t REG_PWR_MODE = 0x3E;      // 전력 모드 설정 주소

    static constexpr std::uint8_t CHIP_ID_VALUE = 0xA0;     // 우리가 쓰는 BNO055 칩 고유 id 값
    static constexpr std::uint8_t MODE_CONFIG = 0x00;       // 설정 변경 되는 config 값
    static constexpr std::uint8_t MODE_IMU = 0x08;          // imu 센서 융합 모드 값
};
