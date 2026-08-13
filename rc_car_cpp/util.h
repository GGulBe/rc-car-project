#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <stdexcept>

std::string makePipeline(int width, int height, int fps);
std::string makePhotoFilename();
void drawStatus(cv::Mat& frame, double speed, double cmd, double steering, double pan, double tilt, double fps);
std::runtime_error systemError(const std::string& message);