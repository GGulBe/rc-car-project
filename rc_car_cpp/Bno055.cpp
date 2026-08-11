// imu 로직 코드
#include "Bno055.h"
#include "util.h"

#include <array>
#include <chrono>
#include <thread>

Bno055::Bno055(I2cDevice& i2c): i2c_(i2c){  }
    

void Bno055::initialize() { // 시작할 때 한번 진행하는 코드 데이터를 넣어줌
    std::this_thread::sleep_for(std::chrono::milliseconds(700)); //BNO055가 전원 공급 직후 바로 사용할 수 있는 상태가 아니기 때문

    if (i2c_.readRegister8(REG_CHIP_ID) != CHIP_ID_VALUE) throw systemError("Invalid BNO055 CHIP_ID");// 칩 id 확인

    i2c_.writeRegister8(REG_PAGE_ID, 0x00); // Register Page 0 선택
    i2c_.writeRegister8(REG_OPR_MODE, MODE_CONFIG); // 동작 모드를 설정 변경 모드로 변경
    std::this_thread::sleep_for(std::chrono::milliseconds(25));

    i2c_.writeRegister8(REG_PWR_MODE, 0x00); // 전력 모드를 0(일반 모드)으로 설정
    i2c_.writeRegister8(REG_UNIT_SEL, 0x00); // 출력 단위를 m/s², °/s, degree, °C 등 기본 단위로 설정
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    i2c_.writeRegister8(REG_OPR_MODE, MODE_IMU); // MODE_IMU = 0x08 -> IMU모드 실행
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

Bno055::Tilt Bno055::readMotion() const {
    std::array<std::uint8_t, 6> data{};

    // Heading LSB/MSB + Roll LSB/MSB + Pitch LSB/MSB 
    i2c_.readRegisters(REG_EULER, data.data(), data.size());
    const double heading = static_cast<double>(decodeInt16(&data[0])) / 16.0;
    const double roll = static_cast<double>(decodeInt16(&data[2])) / 16.0;
    const double pitch = static_cast<double>(decodeInt16(&data[4])) / 16.0;

    return {heading, roll, pitch};
}

std::int16_t Bno055::decodeInt16(const std::uint8_t* data) {
    const std::uint16_t value = static_cast<std::uint16_t>(data[0]) | (static_cast<std::uint16_t>(data[1]) << 8U);

    return static_cast<std::int16_t>(value);
}
