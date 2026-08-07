#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

class I2cDevice {
public:
    explicit I2cDevice(std::uint8_t address, const std::string& devicePath = "/dev/i2c-1");
    ~I2cDevice();

    I2cDevice(const I2cDevice&) = delete;
    I2cDevice& operator=(const I2cDevice&) = delete;
    void writeRegister16(std::uint8_t reg, std::uint16_t value);
    std::uint8_t readRegister8(std::uint8_t reg) const;
    void readRegisters(std::uint8_t startReg, std::uint8_t* buffer, std::size_t length) const;
    
private:
    int fd_ = -1;
    std::uint8_t address_;
};
