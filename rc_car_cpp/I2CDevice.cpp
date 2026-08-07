#include "I2CDevice.h"
#include "util.h"

#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdexcept>
#include <system_error>
#include <sys/ioctl.h>
#include <unistd.h>

I2cDevice::I2cDevice(std::uint8_t address_, const std::string& devicePath) : address_(address) {
    fd_ = ::open(devicePath.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0) throw systemError("Failed to open " + devicePath);

    if (::ioctl(fd_, I2C_SLAVE, address_) < 0) {
        ::close(fd_);
        fd_ = -1;
        throw systemError("Failed to select I2C device");
    }
}


void I2cDevice::writeRegister16(std::uint8_t reg, std::uint16_t value) {
        const std::uint8_t data[3] = {
            reg,
            static_cast<std::uint8_t>((value >> 8U) & 0xFFU),
            static_cast<std::uint8_t>(value & 0xFFU)
        };
        if (::write(fd_, data, sizeof(data)) != static_cast<ssize_t>(sizeof(data))) throw systemError("I2C write failed");
}

I2cDevice::~I2cDevice() {
    if (fd_ >= 0) ::close(fd_);
}

void I2cDevice::writeRegister8(std::uint8_t reg, std::uint8_t value) const {
    const std::uint8_t data[2] = {reg, value};
    if (::write(fd_, data, sizeof(data)) != static_cast<ssize_t>(sizeof(data))) throw systemError("I2C register write failed");
}

std::uint8_t I2cDevice::readRegister8(std::uint8_t reg) const {
    std::uint8_t value = 0;
    readRegisters(reg, &value, 1);
    return value;
}

void I2cDevice::readRegisters(std::uint8_t startReg, std::uint8_t* buffer, std::size_t length) const {
    if (buffer == nullptr || length == 0 || length > std::numeric_limits<__u16>::max()) throw std::invalid_argument("Invalid I2C read buffer");

    i2c_msg messages[2]{};
    messages[0].addr = address_;
    messages[0].flags = 0;
    messages[0].len = 1;
    messages[0].buf = &startReg;

    messages[1].addr = address_;
    messages[1].flags = I2C_M_RD;
    messages[1].len = static_cast<__u16>(length);
    messages[1].buf = buffer;

    i2c_rdwr_ioctl_data transaction{};
    transaction.msgs = messages;
    transaction.nmsgs = 2;

    if (::ioctl(fd_, I2C_RDWR, &transaction) < 0) throw systemError("I2C combined read failed");
}
