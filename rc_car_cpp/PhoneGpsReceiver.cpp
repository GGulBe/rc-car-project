#include "PhoneGpsReceiver.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>


namespace
{

double nmeaToDecimal(const std::string& value, const std::string& direction)
{
    if (value.empty() || direction.empty())
    {
        throw std::runtime_error("Invalid NMEA coordinate");
    }

    const double raw = std::stod(value);

    // 3735.1810
    // -> 37도 + 35.1810분
    const double degrees = std::floor(raw / 100.0);
    const double minutes = raw - (degrees * 100.0);

    double decimal = degrees + (minutes / 60.0);

    if (direction == "S" || direction == "W")
    {
        decimal = -decimal;
    }
    else if (direction != "N" && direction != "E")
    {
        throw std::runtime_error("Invalid NMEA direction");
    }

    return decimal;
}

bool isRmcSentence(const std::string& line)
{
    // $GPRMC
    // $GNRMC
    // $GLRMC 등 talker ID가 달라도 RMC면 허용

    return line.size() >= 6
        && line[0] == '$'
        && line.compare(3, 3, "RMC") == 0;
}

}


PhoneGpsReceiver::PhoneGpsReceiver(int port)
{
    // TCP 소켓 생성
    serverFd_ = ::socket(AF_INET, SOCK_STREAM, 0);

    if (serverFd_ < 0)
    {
        throw std::runtime_error(
            std::string("Failed to create TCP socket: ")
            + std::strerror(errno)
        );
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
        const std::string error =
            std::strerror(errno);

        ::close(serverFd_);
        serverFd_ = -1;

        throw std::runtime_error(
            "Failed to set socket option: " + error
        );
    }


    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;

    // Raspberry Pi의 모든 네트워크 인터페이스 허용
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // TCP 포트
    serverAddress.sin_port =
        htons(static_cast<std::uint16_t>(port));


    // 소켓과 포트 연결
    if (::bind(
            serverFd_,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) < 0)
    {
        const std::string error =
            std::strerror(errno);

        ::close(serverFd_);
        serverFd_ = -1;

        throw std::runtime_error(
            "Failed to bind TCP socket: " + error
        );
    }


    // TCP Client 연결 대기 상태
    if (::listen(serverFd_, 1) < 0)
    {
        const std::string error =
            std::strerror(errno);

        ::close(serverFd_);
        serverFd_ = -1;

        throw std::runtime_error(
            "Failed to listen TCP socket: " + error
        );
    }

    std::cout
        << "[Phone GPS] TCP server listening on port "
        << port
        << std::endl;
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
    std::cout
        << "[Phone GPS] Waiting for phone..."
        << std::endl;

    sockaddr_in clientAddress{};
    socklen_t clientLength = sizeof(clientAddress);

    while (true)
    {
        clientFd_ = ::accept(
            serverFd_,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientLength
        );

        if (clientFd_ >= 0)
        {
            break;
        }

        if (errno == EINTR)
        {
            continue;
        }

        throw std::runtime_error(
            std::string("Failed to accept phone connection: ")
            + std::strerror(errno)
        );
    }


    char ip[INET_ADDRSTRLEN]{};

    ::inet_ntop(
        AF_INET,
        &clientAddress.sin_addr,
        ip,
        sizeof(ip)
    );

    std::cout
        << "[Phone GPS] Connected: "
        << ip
        << std::endl;
}


std::string PhoneGpsReceiver::readRmc()
{
    while (true)
    {
        // 연결되어 있지 않으면 여기서 휴대폰 연결 대기
        if (clientFd_ < 0)
        {
            waitForClient();
        }


        // 이미 받아둔 데이터에서 한 줄 검사
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


            // NMEA는 일반적으로 \r\n
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }


            // 앞에 이상한 데이터가 붙어 있을 경우 $부터 사용
            const std::size_t dollar =
                line.find('$');

            if (dollar != std::string::npos)
            {
                line.erase(0, dollar);
            }


            // RMC 문장만 반환
            if (isRmcSentence(line))
            {
                return line;
            }

            // GGA/GSA/GSV 등의 다른 NMEA 문장은 무시
            continue;
        }


        char tempBuffer[512];

        const ssize_t received =
            ::recv(
                clientFd_,
                tempBuffer,
                sizeof(tempBuffer),
                0
            );


        if (received > 0)
        {
            buffer_.append(
                tempBuffer,
                static_cast<std::size_t>(received)
            );

            continue;
        }


        // 상대방이 정상적으로 연결 종료
        if (received == 0)
        {
            std::cout
                << "[Phone GPS] Phone disconnected."
                << std::endl;

            ::close(clientFd_);

            clientFd_ = -1;
            buffer_.clear();

            // while 처음으로 돌아가서 다시 accept()
            continue;
        }


        // signal에 의해 recv가 잠깐 중단됨
        if (errno == EINTR)
        {
            continue;
        }


        std::cerr
            << "[Phone GPS] Receive error: "
            << std::strerror(errno)
            << std::endl;


        ::close(clientFd_);

        clientFd_ = -1;
        buffer_.clear();

        // 다시 휴대폰 연결 기다림
    }
}


UartDevice::gpsdata PhoneGpsReceiver::parseRmc()
{
    UartDevice::gpsdata data{};

    const std::string line = readRmc();


    // 예:
    //
    // $GNRMC,063105.00,A,3735.1810,N,12705.8476,E,
    //        0.0,0.0,180826,,,A*XX
    //
    // index
    // 0 = $GNRMC
    // 1 = UTC
    // 2 = Status (A/V)
    // 3 = Latitude
    // 4 = N/S
    // 5 = Longitude
    // 6 = E/W
    // 7 = Speed
    // 8 = Course
    // 9 = Date


    std::stringstream stream(line);

    std::vector<std::string> fields;
    std::string field;


    while (std::getline(stream, field, ','))
    {
        fields.push_back(field);
    }


    if (fields.size() < 10)
    {
        throw std::runtime_error(
            "Invalid RMC sentence: insufficient fields"
        );
    }


    if (!isRmcSentence(fields[0]))
    {
        throw std::runtime_error(
            "Invalid RMC sentence type"
        );
    }


    // UTC
    data.utc = fields[1];

    // A = Active / GPS fix 정상
    // V = Void   / GPS fix 없음
    data.gpsfix = (fields[2] == "A");

    // 날짜
    data.date = fields[9];


    // GPS fix가 없으면 좌표 변환하지 않음
    if (!data.gpsfix)
    {
        data.lat = 0.0;
        data.lon = 0.0;

        return data;
    }


    // NMEA ddmm.mmmm 형식 → decimal degree
    data.lat =
        nmeaToDecimal(
            fields[3],
            fields[4]
        );

    data.lon =
        nmeaToDecimal(
            fields[5],
            fields[6]
        );


    // 범위 검증
    if (data.lat < -90.0 || data.lat > 90.0)
    {
        throw std::runtime_error(
            "Invalid GPS latitude range"
        );
    }

    if (data.lon < -180.0 || data.lon > 180.0)
    {
        throw std::runtime_error(
            "Invalid GPS longitude range"
        );
    }


    return data;
}