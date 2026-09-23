#include "UartDevice.h"
#include "util.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

UartDevice::UartDevice(const std::string& devicePath)
{
    fd_ = ::open(devicePath.c_str(), O_RDWR | O_CLOEXEC | O_NOCTTY);

    if(fd_ < 0){
        ::close(fd_);
        fd_ = -1;
        throw systemError("Failed to select I2C device");
    }
    
    configure();
}

UartDevice::~UartDevice()
{
    if (fd_ >= 0) ::close(fd_);
}

// UART 통신 설정
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

std::string UartDevice::readRmc()
{
    while (true) {
        std::string line = readLine();

        if (line.rfind("$GPRMC", 0) == 0 || line.rfind("$GNRMC", 0) == 0) {
            return line;
        }
    }
}

std::string UartDevice::readLine() {
    // 버퍼링을 도입하여 1바이트씩 읽는 대신 한 번에 읽어와 시스템 콜 부하 최소화
    static std::string buffer;
    char read_buf[128];

    while (true) {
        size_t pos = buffer.find('\n');
        if (pos != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            buffer.erase(0, pos + 1);
            return line;
        }

        const ssize_t bytesRead = ::read(fd_, read_buf, sizeof(read_buf));

        if (bytesRead < 0) {
            throw systemError("Failed to read UART");
        }

        if (bytesRead == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        buffer.append(read_buf, bytesRead);
    }
}

UartDevice::gpsdata UartDevice::parseRmc()
{
    std::string line = readRmc();
    gpsdata result{};

    std::stringstream ss(line);
    std::string token;
    std::vector<std::string> fields;

    while (std::getline(ss, token, ',')) {
        fields.push_back(token);
    }

    if (fields.size() < 10) {
        return result;
    }

    if (fields[0] != "$GPRMC" && fields[0] != "$GNRMC") {
        return result;
    }

    result.utc = fields[1];

    // A = GPS 유효, V = GPS 무효
    result.gpsfix = (fields[2] == "A");

    if (!fields[3].empty()) {
        double rawLat = std::stod(fields[3]);

        double degree = static_cast<int>(rawLat / 100);
        double minute = rawLat - degree * 100;

        result.lat = degree + minute / 60.0;

        if (fields[4] == "S") {
            result.lat *= -1.0;
        }
    }

    if (!fields[5].empty()) {
        double rawLon = std::stod(fields[5]);

        double degree = static_cast<int>(rawLon / 100);
        double minute = rawLon - degree * 100;

        result.lon = degree + minute / 60.0;

        if (fields[6] == "W") {
            result.lon *= -1.0;
        }
    }

    result.date = fields[9];

    return result;
}
