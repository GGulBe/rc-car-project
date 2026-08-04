// imu 헤더파일
#pragma once

#include "I2CDevice.h"

#include <chrono>
#include <cstddef>
#include <cstdint>

class Bno055 {
public:
    enum class Axis : std::uint8_t { X = 0, Y = 1, Z = 2 }; // enum : x호출하면 그걸 0으로 알아서 확인 해 주는듯
    
    struct Motion { // 선배님이 쓰시는 방식중에 구조체 선언하며 typedef하여 이름 한번 더 선언하시는 방식 자주 쓰시는데 우리도 고쳐야하나?
        double forwardAccelerationMps2; // Mps2 = m / s^2
        double yawRateDps; // Dps = d / s *d == Degrees(각도)
        double speedMmps; // Mmps = mm / s
    };

    explicit Bno055(I2cDevice& i2c, Axis forwardAxis = Axis::Y, double forwardSign = 1.0, Axis yawAxis = Axis::Z, double yawSign = 1.0);

    void initialize();
    Motion readMotion() const;
    Motion update(double motorPwmPercent);
    void resetSpeed() noexcept; // except(예외)가 안난다는걸 보장해주기 때문에 최적화에 좋음

private: 
    static std::int16_t decodeInt16(const std::uint8_t* data);
    static std::size_t axisOffset(Axis axis);

    I2cDevice& i2c_;
    Axis forwardAxis_;
    Axis yawAxis_;
    double forwardSign_;
    double yawSign_;

    double speedMmps_ = 0.0;
    bool timeInitialized_ = false;
    std::chrono::steady_clock::time_point previousTime_{};

    static constexpr double ACCELERATION_DEADBAND_MPS2 = 0.08;
    static constexpr double MOTOR_PWM_DEADBAND_PERCENT = 0.5;
    static constexpr double MAX_SPEED_MMPS = 5000.0;
    
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
