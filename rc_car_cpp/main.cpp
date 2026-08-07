#include <cerrno>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
 
#include "MotorController.h"
#include "PwmController.h"
#include "ServoController.h"
#include "TerminalInput.h"
#include "I2CDevice.h"
#include "util.h"
#include "Bno055.h"


constexpr double P0_CENTER = -80.0;
constexpr double P1_CENTER = 0.0;
constexpr double P2_CENTER = -75.0;
constexpr double P0_MIN = -140.0;
constexpr double P0_MAX = 20.0;
constexpr double P1_MIN = -35.0;
constexpr double P1_MAX = 65.0;
constexpr double P2_MIN = -100.0;
constexpr double P2_MAX = -50.0;
constexpr double CAMERA_STEP = 5.0;
constexpr double STEERING_STEP = 1.0;

int main() {
    try {
        I2cDevice i2c(0x14);
        I2cDevice bno055I2c(0x28);
        PwmController pwm(i2c);
        ServoController servos(pwm);
        MotorController motors(pwm);
        
        Bno055 imu(bno055I2c);
        imu.initialize();

        servos.setCalibration(0, { P0_MIN, P0_MAX});
        servos.setCalibration(1, { P1_MIN, P1_MAX });
        servos.setCalibration(2, { P2_MIN, P2_MAX});

        servos.setAngle(0, P0_CENTER);
        servos.setAngle(1, P1_CENTER);
        servos.setAngle(2, P2_CENTER);
        motors.stop();

        constexpr int width = 640;
        constexpr int height = 480;
        constexpr int targetFps = 30;

        cv::VideoCapture camera(makePipeline(width, height, targetFps));
        if (!camera.isOpened()) {
            std::cerr << "GStreamer camera open failed. Trying V4L2 index 0.\n";
            camera.open(0, cv::CAP_V4L2);
        }
        if (!camera.isOpened()) throw systemError("Failed to open Raspberry Pi camera");

        double speedSetting = 30.0;
        double driveCommand = 0.0;
        double steeringAngle = P2_CENTER;
        double cameraPan = P0_CENTER;
        double cameraTilt = P1_CENTER;

        cv::Mat frame;
        int frameCounter = 0;
        double measuredFps = 0.0;
        auto fpsStart = std::chrono::steady_clock::now();

        TerminalInput keyboard;
        cv::namedWindow("Robot Camera Control", cv::WINDOW_AUTOSIZE);
        std::cout << "Keyboard input is read from this terminal. Press W/A/S/D without Enter.\n";

        while (true) {
            if (!camera.read(frame) || frame.empty()) throw systemError("Failed to read camera frame");
            
            const Bno055::Tilt tilt = imu.readMotion();

            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - fpsStart).count();
            if (elapsed >= 1.0) {
                measuredFps = frameCounter / elapsed;
                frameCounter = 0;
                fpsStart = now;
            }
            
            cv::Mat display = frame.clone();
            drawStatus(display, speedSetting, driveCommand, steeringAngle, cameraPan, cameraTilt, measuredFps,tilt);
            cv::imshow("Robot Camera Control", display);

            const int windowKey = cv::waitKey(1);
            int key = keyboard.readKey(0);
            if (key < 0 && windowKey >= 0) key = windowKey & 0xFF;
            if (key < 0) continue;

            if (key == 'w' || key == 'W') {
                driveCommand = speedSetting;
                motors.drive(driveCommand);
            }
            else if (key == 's' || key == 'S') {
                driveCommand = -speedSetting;
                motors.drive(driveCommand);
            }
            else if (key == ' ') {
                driveCommand = 0.0;
                motors.stop();
            }
            else if (key == 'a' || key == 'A') {
                steeringAngle = std::max(P2_MIN, steeringAngle - STEERING_STEP);
                servos.setAngle(2, steeringAngle);
            }
            else if (key == 'd' || key == 'D') {
                steeringAngle = std::min(P2_MAX, steeringAngle + STEERING_STEP);
                servos.setAngle(2, steeringAngle);
            }
            else if (key == 'x' || key == 'X') {
                steeringAngle = P2_CENTER;
                servos.setAngle(2, steeringAngle);
            }
            else if (key == 'j' || key == 'J') {
                cameraPan = std::max(P0_MIN, cameraPan - CAMERA_STEP);
                servos.setAngle(0, cameraPan);
            }
            else if (key == 'l' || key == 'L') {
                cameraPan = std::min(P0_MAX, cameraPan + CAMERA_STEP);
                servos.setAngle(0, cameraPan);
            }
            else if (key == 'i' || key == 'I') {
                cameraTilt = std::min(P1_MAX, cameraTilt + CAMERA_STEP);
                servos.setAngle(1, cameraTilt);
            }
            else if (key == 'k' || key == 'K') {
                cameraTilt = std::max(P1_MIN, cameraTilt - CAMERA_STEP);
                servos.setAngle(1, cameraTilt);
            }
            else if (key == 'c' || key == 'C') {
                cameraPan = P0_CENTER;
                cameraTilt = P1_CENTER;
                servos.setAngle(0, cameraPan);
                servos.setAngle(1, cameraTilt);
            }
            else if (key == '+' || key == '=') {
                speedSetting = std::min(100.0, speedSetting + 5.0);
                if (driveCommand != 0.0) {
                    driveCommand = driveCommand > 0.0 ? speedSetting : -speedSetting;
                    motors.drive(driveCommand);
                }
            }
            else if (key == '-' || key == '_') {
                speedSetting = std::max(10.0, speedSetting - 5.0);
                if (driveCommand != 0.0) {
                    driveCommand = driveCommand > 0.0 ? speedSetting : -speedSetting;
                    motors.drive(driveCommand);
                }
            }
            else if (key == 'p' || key == 'P') {
                const std::string filename = makePhotoFilename();
                if (cv::imwrite(filename, frame)) std::cout << "Saved: " << filename << '\n';
                else std::cerr << "Failed to save image\n";
            }
            else if (key == 'r' || key == 'R') {
                driveCommand = 0.0;
                steeringAngle = P2_CENTER;
                cameraPan = P0_CENTER;
                cameraTilt = P1_CENTER;
                motors.stop();
                servos.setAngle(0, cameraPan);
                servos.setAngle(1, cameraTilt);
                servos.setAngle(2, steeringAngle);
            }
            else if (key == 'q' || key == 'Q' || key == 27) {
                break;
            }
        }

        motors.stop();
        servos.setAngle(2, P2_CENTER);
        camera.release();
        cv::destroyAllWindows();
        return 0;
    }
    catch (const cv::Exception& error) {
        std::cerr << "OpenCV error: " << error.what() << '\n';
        return 1;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
