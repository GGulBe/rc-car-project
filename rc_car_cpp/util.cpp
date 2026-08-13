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
    return pipeline.str();
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

cv::Mat makeDisplay(const cv::Mat& frame, double measuredFps, const Bno055::Tilt& tilt, const UartDevice::gpsdata& gpsdata, double speedSetting, double driveCommand, double steeringAngle)
{
    constexpr int INFO_HEIGHT = 140;

    cv::Mat display(frame.rows + INFO_HEIGHT, frame.cols, frame.type(), cv::Scalar(255, 255, 255));

    frame.copyTo(display(cv::Rect(0, INFO_HEIGHT, frame.cols, frame.rows)));

    std::ostringstream gpsline;
    gpsline << "gpsfix : " << gpsdata.gpsfix << " UTC : " << gpsdata.utc << " date : " << gpsdata.date;
    cv::putText(display, gpsline.str(), cv::Point(10, 20), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(0, 0, 0), 1);

    std::ostringstream gpsPosition;
    gpsPosition << std::fixed << std::setprecision(6) << "lat : " << gpsdata.lat << " lon : " << gpsdata.lon;
    cv::putText(display, gpsPosition.str(), cv::Point(10, 40), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(0, 0, 0), 1);

    std::ostringstream imuline;
    imuline << std::fixed << std::setprecision(1) << "heading : " << tilt.headingDeg << " pitch : " << tilt.pitchDeg << " roll : " << tilt.rollDeg;
    cv::putText(display, imuline.str(), cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(0, 0, 0), 1);

    std::ostringstream fpsText;
    fpsText << std::fixed << std::setprecision(1) << "FPS : " << measuredFps;
    cv::putText(display, fpsText.str(), cv::Point(10, 80), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(0, 0, 0), 1);

    std::ostringstream motorLine;
    motorLine << std::fixed << std::setprecision(1) << "SpeedSetting : " << speedSetting << " Drive : " << driveCommand;
    cv::putText(display, motorLine.str(), cv::Point(10, 100), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(0, 0, 0), 1);

    std::ostringstream servoLine;
    servoLine << std::fixed << std::setprecision(1) << "Steering : " << steeringAngle << " Pan : " << cameraPan << " Tilt : " << cameraTilt;
    cv::putText(display, servoLine.str(), cv::Point(10, 120), cv::FONT_HERSHEY_SIMPLEX, 0.35, cv::Scalar(0, 0, 0), 1);

    return display;
}