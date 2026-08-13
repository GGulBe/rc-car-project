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

std::string makePipeline(int width, int height, int fps) {
    std::ostringstream pipeline;
    pipeline << "libcamerasrc ! video/x-raw,width=" << width << ",height=" << height << ",format=NV12,framerate=" << fps
        << "/1 ! videoconvert ! video/x-raw,format=BGR ! queue max-size-buffers=1 leaky=downstream ! appsink drop=true max-buffers=1 sync=false";
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

void drawStatus(cv::Mat& frame, double speedSetting, double driveCommand, double steeringAngle, double cameraPan, double cameraTilt, double fps, std::ostringstream imuline, std::ostringstream gpsline) {
    std::ostringstream line1;
    line1 << std::fixed << std::setprecision(1) << "FPS " << fps << "  Speed " << speedSetting << "%  Drive " << driveCommand << "%";
    std::ostringstream line2;
    line2 << "P2 steer " << steeringAngle << "  P0 pan " << cameraPan << "  P1 tilt " << cameraTilt;
    cv::putText(frame, line1.str(), cv::Point(15, 30), cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
    cv::putText(frame, line2.str(), cv::Point(15, 60), cv::FONT_HERSHEY_SIMPLEX, 0.60, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);;
    cv::putText(frame, imuline.str(), cv::Point(15, 90), cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
    cv::putText(frame, gpsline.str(), cv::Point(15, 120),cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
   
    
}
