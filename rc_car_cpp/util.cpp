#include "util.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <cstring>

std::string makePipeline(int width, int height, int fps) {
    return "libcamerasrc ! video/x-raw, width=" + std::to_string(width) +
           ", height=" + std::to_string(height) +
           ", framerate=" + std::to_string(fps) + "/1 ! videoconvert ! video/x-raw, format=BGRx ! videoconvert ! video/x-raw, format=BGR ! appsink";
}

std::string makePhotoFilename() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "photo_" << std::put_time(std::localtime(&now_c), "%Y%m%d_%H%M%S") << ".jpg";
    return ss.str();
}

void drawStatus(cv::Mat& frame, double speed, double cmd, double steering, double pan, double tilt, double fps) {
    double fontScale = 0.5;
    int thickness = 1;
    cv::Scalar color(0, 255, 0); // 초록색 텍스트

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "Speed: " << speed << " | Cmd: " << cmd << " | Steer: " << steering
       << " | Pan: " << pan << " | Tilt: " << tilt << " | FPS: " << fps;

    cv::putText(frame, ss.str(), cv::Point(10, frame.rows - 10), cv::FONT_HERSHEY_SIMPLEX, fontScale, color, thickness);
}

std::runtime_error systemError(const std::string& message) {
    return std::runtime_error(message + " (errno: " + std::strerror(errno) + ")");
}