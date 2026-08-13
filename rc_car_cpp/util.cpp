#include "util.h"
#include "Bno055.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

std::runtime_error systemError(const std::string& message) {
    return std::runtime_error(message + ": " + std::strerror(errno));
}

std::string makePipeline(int width, int height, int fps, double speedSetting, double driveCommand, double steeringAngle, double cameraPan, double cameraTilt, double fps,Bno055::Tilt tilt, UartDevice::gpsdata gpsdata) {
    std::ostringstream pipeline;
    pipeline << "libcamerasrc ! video/x-raw,width=" << width << ",height=" << height << ",format=NV12,framerate=" << fps
        << "/1 ! videoconvert ! video/x-raw,format=BGR ! queue max-size-buffers=1 leaky=downstream ! appsink drop=true max-buffers=1 sync=false" << std::endl;
    std::ostringstream line1;
    line1 << std::fixed << std::setprecision(1) << "FPS " << fps << "  Speed " << speedSetting << "%  Drive " << driveCommand << "%" << std::endl;
    std::ostringstream line2;
    line2 << "P2 steer " << steeringAngle << "  P0 pan " << cameraPan << "  P1 tilt " << cameraTilt << std::endl;
    std::ostringstream line3;
    line3 << "gpsfix : " << gpsdata.gpsfix << " 위도 : " << gpsdata.lat <<  " 경도 : " << gpsdata.lon << " UTC : " << gpsdata.utc << " date : " << gpsdata.date << std::endl;
    std::ostringstream line4;
    line4 << "heading : " << tilt.headingDeg << " roll : " << tilt.rollDeg << " pitch : " << tilt.pitchDeg << std::endl;
    return (pipeline+line1+line2+line3+line4).str();
}

std::string makePhotoFilename() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_r(&time, &local);
    std::ostringstream name;
    name << "robot_photo_" << std::put_time(&local, "%Y%m%d_%H%M%S") << ".jpg";
    return name.str();
}

