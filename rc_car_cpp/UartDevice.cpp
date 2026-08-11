#include "UartDevice.h"
#include "util.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstddef>
#include <cstdint>
UartDevice::UartDevice(const std::string& devicePath)
{
    fd_ = ::open(devicePath.c_str(), O_RDWR | O_CLOEXEC | O_NOCTTY);

    if(fd_ < 0){
        ::close(fd_);
        fd_ = -1;
        throw systemError("Failed to select I2C device");
    }
    
    
}

UartDevice::~UartDevice()
{
    if (fd_ >= 0) ::close(fd_);
}
//UART 통신 설정
void UartDevice::configure()
{
    termios tty{};

    if (::tcgetattr(fd_, &tty) != 0) {
        throw systemError("Failed to get UART settings");
    }

    ::cfmakeraw(&tty);

    ::cfsetispeed(&tty, B9600);
    ::cfsetospeed(&tty, B9600);

    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CLOCAL | CREAD;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    if (::tcsetattr(fd_, TCSANOW, &tty) != 0) {
        throw systemError("Failed to get UART settings");
    }
}

std::string UartDevice::readLine() {
    std::string line;
    char ch;

    while (true) {
        const ssize_t bytesRead = ::read(fd_, &ch, 1);

        if (bytesRead < 0) {
            throw systemError("Failed to read UART");
        }

        if (bytesRead == 0) {
            continue;
        }

        if (ch == '\n') {
            break;
        }

        if (ch != '\r') {
            line += ch;
        }
    }

    return line;
}

