#include "PhoneGpsReceiver.h"

#include <arpa/inet.h>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>


PhoneGpsReceiver::PhoneGpsReceiver(int port)
{
    // TCP 소켓 생성
    serverFd_ = ::socket(AF_INET, SOCK_STREAM, 0);

    if (serverFd_ < 0)
    {
        throw std::runtime_error("Failed to create TCP socket");
    }


    // 프로그램 재실행 시 포트 재사용 가능
    int option = 1;

    if (::setsockopt(
            serverFd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &option,
            sizeof(option)) < 0)
    {
        ::close(serverFd_);
        serverFd_ = -1;

        throw std::runtime_error("Failed to set socket option");
    }


    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;

    // 라즈베리파이의 모든 네트워크 인터페이스에서 접속 허용
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // TCP 포트 설정
    serverAddress.sin_port =
        htons(static_cast<std::uint16_t>(port));


    // 소켓과 포트 연결
    if (::bind(
            serverFd_,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) < 0)
    {
        ::close(serverFd_);
        serverFd_ = -1;

        throw std::runtime_error("Failed to bind TCP socket");
    }


    // 클라이언트 접속 대기
    if (::listen(serverFd_, 1) < 0)
    {
        ::close(serverFd_);
        serverFd_ = -1;

        throw std::runtime_error("Failed to listen TCP socket");
    }
}


PhoneGpsReceiver::~PhoneGpsReceiver()
{
    if (clientFd_ >= 0)
    {
        ::close(clientFd_);
    }

    if (serverFd_ >= 0)
    {
        ::close(serverFd_);
    }
}


void PhoneGpsReceiver::waitForClient()
{
    sockaddr_in clientAddress{};
    socklen_t clientLength = sizeof(clientAddress);

    clientFd_ = ::accept(
        serverFd_,
        reinterpret_cast<sockaddr*>(&clientAddress),
        &clientLength
    );

    if (clientFd_ < 0)
    {
        throw std::runtime_error(
            "Failed to accept phone connection"
        );
    }
}


std::string PhoneGpsReceiver::readRmc()
{
    while (true)
    {
        // 이미 buffer_에 완성된 한 줄이 있는지 검사
        const std::size_t newlinePosition =
            buffer_.find('\n');

        if (newlinePosition != std::string::npos)
        {
            std::string line =
                buffer_.substr(0, newlinePosition);

            buffer_.erase(
                0,
                newlinePosition + 1
            );

            // Android 등에서 \r\n을 보냈을 경우 \r 제거
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            return line;
        }


        char tempBuffer[256];

        const ssize_t received =
            ::recv(
                clientFd_,
                tempBuffer,
                sizeof(tempBuffer),
                0
            );


        if (received < 0)
        {
            throw std::runtime_error(
                "Phone GPS receive failed"
            );
        }


        if (received == 0)
        {
            throw std::runtime_error(
                "Phone disconnected"
            );
        }


        buffer_.append(
            tempBuffer,
            static_cast<std::size_t>(received)
        );
    }
}


UartDevice::gpsdata PhoneGpsReceiver::parseRmc()
{
    UartDevice::gpsdata data{};

    // TCP에서 GPS 한 줄 받아오기
    const std::string line = readRmc();

    std::stringstream stream(line);

    std::string gpsfixText;
    std::string latText;
    std::string lonText;
    std::string utcText;
    std::string dateText;

    if (!std::getline(stream, gpsfixText, ','))
        throw std::runtime_error("Invalid GPS fix");

    if (!std::getline(stream, latText, ','))
        throw std::runtime_error("Invalid latitude");

    if (!std::getline(stream, lonText, ','))
        throw std::runtime_error("Invalid longitude");

    if (!std::getline(stream, utcText, ','))
        throw std::runtime_error("Invalid UTC");

    if (!std::getline(stream, dateText, ','))
        throw std::runtime_error("Invalid date");

    data.gpsfix = (gpsfixText == "1");

    if (data.gpsfix)
    {
        data.lat = std::stod(latText);
        data.lon = std::stod(lonText);
    }

    data.utc = utcText;
    data.date = dateText;

    return data;
}