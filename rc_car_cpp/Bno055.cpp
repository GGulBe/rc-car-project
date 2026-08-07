// imu 로직 코드
#include "Bno055.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>

Bno055::Bno055(I2cDevice& i2c, Axis forwardAxis, double forwardSign, Axis yawAxis, double yawSign)
    : i2c_(i2c), forwardAxis_(forwardAxis), yawAxis_(yawAxis), forwardSign_(forwardSign), yawSign_(yawSign) {
    if ((forwardSign_ != 1.0 && forwardSign_ != -1.0) || (yawSign_ != 1.0 && yawSign_ != -1.0)) throw std::invalid_argument("Axis sign must be 1.0 or -1.0");
} // sign(부호)는 무조건 1.0(정방향) , (-1.0)역방향으로 계산해야 하므로 예외처리 하는 과정이 담겨 있는듯

void Bno055::initialize() { // 시작할 때 한번 진행하는 코드 데이터를 넣어줌
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    if (i2c_.readRegister8(REG_CHIP_ID) != CHIP_ID_VALUE) throw std::runtime_error("Invalid BNO055 CHIP_ID");// 칩 id 확인

    i2c_.writeRegister16(REG_PAGE_ID, 0x00); // 값들을 data[2]에 저장
    i2c_.writeRegister16(REG_OPR_MODE, MODE_CONFIG); // 동작 모드를 설정 변경 모드로 변경
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    i2c_.writeRegister16(REG_PWR_MODE, 0x00); // 전력 모드를 0(일반 모드)으로 설정
    i2c_.writeRegister16(REG_UNIT_SEL, 0x00); // 미터 기반 단위를 사용하기 위해 단위 설정
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    i2c_.writeRegister16(REG_OPR_MODE, MODE_IMU); // MODE_IMU = 0x08 -> IMU모드 실행
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    speedMmps_ = 0.0;
    timeInitialized_ = false;
}

Bno055::Motion Bno055::readMotion() const {
    std::array<std::uint8_t, 6> linearAcceleration{};
    std::array<std::uint8_t, 6> gyroscope{};

    i2c_.readRegisters(REG_LIA_DATA_X, linearAcceleration.data(), linearAcceleration.size());
    i2c_.readRegisters(REG_GYR_DATA_X, gyroscope.data(), gyroscope.size());

    const std::size_t forwardOffset = axisOffset(forwardAxis_);
    const std::size_t yawOffset = axisOffset(yawAxis_);

    const double forwardAcceleration = static_cast<double>(decodeInt16(&linearAcceleration[forwardOffset])) / 100.0;
    const double yawRate = static_cast<double>(decodeInt16(&gyroscope[yawOffset])) / 16.0;

    return {forwardAcceleration * forwardSign_, yawRate * yawSign_, speedMmps_};
}

Bno055::Motion Bno055::update(double motorPwmPercent) { // rc카 현재 속도 확인인데 update라는 이름은 눈에 확 띄지 않을 것 같음
    Motion motion = readMotion();
    const auto currentTime = std::chrono::steady_clock::now();

    if (!timeInitialized_) {
        previousTime_ = currentTime;
        timeInitialized_ = true;
        motion.speedMmps = speedMmps_;
        return motion;
    }

    const double deltaTimeSeconds = std::chrono::duration<double>(currentTime - previousTime_).count();
    previousTime_ = currentTime;

    if (deltaTimeSeconds <= 0.0 || deltaTimeSeconds > 0.2) {
        motion.speedMmps = speedMmps_;
        return motion;
    }

    if (std::abs(motorPwmPercent) < MOTOR_PWM_DEADBAND_PERCENT) {
        speedMmps_ = 0.0;
    } else {
        double accelerationMps2 = motion.forwardAccelerationMps2;

        if (std::abs(accelerationMps2) < ACCELERATION_DEADBAND_MPS2) accelerationMps2 = 0.0;

        const double accelerationMmps2 = accelerationMps2 * 1000.0;
        speedMmps_ += accelerationMmps2 * deltaTimeSeconds;
        speedMmps_ = std::clamp(speedMmps_, -MAX_SPEED_MMPS, MAX_SPEED_MMPS);
    }

    motion.speedMmps = speedMmps_;
    return motion;
}

void Bno055::resetSpeed() noexcept { // 현재 쓰이는 곳이 없는듯?
    speedMmps_ = 0.0;
    timeInitialized_ = false;
}

std::int16_t Bno055::decodeInt16(const std::uint8_t* data) { // 2byte 데이터를 16bit로 변환해서 다시 뿌려주는 메소드?
    const std::uint16_t value = static_cast<std::uint16_t>(data[0]) | (static_cast<std::uint16_t>(data[1]) << 8U);
    return static_cast<std::int16_t>(value);
} // imu 데이터 시트에 data[1]=0x29이 data[0]=0x28보다 상위 비트임 

std::size_t Bno055::axisOffset(Axis axis) {
    return static_cast<std::size_t>(axis) * 2U; // enum 값을 정수로 취급해줌 2U = 2바이트 크기의 레지스터 간격을 곱해주기 위함
}
