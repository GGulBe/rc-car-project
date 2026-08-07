#pragma once
#include "Bno055.h"

#include <string>
#include <opencv2/opencv.hpp>

std::runtime_error systemError(const std::string& message);
std::string makePipeline(int width, int height, int fps);
std::string makePhotoFilename();
void drawStatus(cv::Mat& frame, double speedSetting, double driveCommand, double steeringAngle, double cameraPan, double cameraTilt, double fps, Bno055::Tilt tilt);
