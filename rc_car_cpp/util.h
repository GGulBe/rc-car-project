#pragma once
#include "Bno055.h"
#include "UartDevice.h"

#include <string>
#include <opencv2/opencv.hpp>

std::runtime_error systemError(const std::string& message);
std::string makePipeline(int width, int height, int fps, double speedSetting, double driveCommand, double steeringAngle, double cameraPan, double cameraTilt, double fps,Bno055::Tilt tilt, UartDevice::gpsdata gpsdata);
std::string makePhotoFilename();

