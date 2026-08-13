#pragma once
#include "Bno055.h"
#include "UartDevice.h"

#include <string>
#include <opencv2/opencv.hpp>

std::runtime_error systemError(const std::string& message);
std::string makePipeline(int width, int height, int fps);
std::string makePhotoFilename();
cv::Mat makeDisplay(const cv::Mat& frame, double measuredFps, const Bno055::Tilt& tilt, const UartDevice::gpsdata& gpsdata, double speedSetting, double driveCommand, double steeringAngle);

