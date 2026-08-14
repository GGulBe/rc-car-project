#include <opencv2/opencv.hpp>
#include <chrono>
#include <string>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>

#include "MotorController.h"
#include "PwmController.h"
#include "ServoController.h"
#include "TerminalInput.h"
#include "I2CDevice.h"
#include "util.h"
#include "Bno055.h"
#include "UartDevice.h"
#include "GPSWorker.h"
#include "MapManager.h"
#include "CalibrationWorker.h"

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
        UartDevice gps;
        TerminalInput keyboard;

        imu.initialize();

        servos.setCalibration(0, { P0_MIN, P0_MAX});
        servos.setCalibration(1, { P1_MIN, P1_MAX });
        servos.setCalibration(2, { P2_MIN, P2_MAX});

        servos.setAngle(0, P0_CENTER);
        servos.setAngle(1, P1_CENTER);
        servos.setAngle(2, P2_CENTER);
        motors.stop();

        std::atomic<bool> running{true};
        std::thread gpsThread(gpsWorker, std::ref(gps), std::ref(running));

        constexpr int width = 320;
        constexpr int height = 240;
        constexpr int targetFps = 30;

        cv::VideoCapture camera(makePipeline(width, height, targetFps), cv::CAP_GSTREAMER);
        if (!camera.isOpened()) {
            std::cerr << "GStreamer camera open failed. Trying V4L2 index 0.\n";
            camera.open(0, cv::CAP_V4L2);
        }
        if (!camera.isOpened()) throw systemError("Failed to open Raspberry Pi camera");

        MapManager mapManager("map_3.jpg");

        cv::Mat map_image = cv::imread("map_3.jpg");
        if (map_image.empty()) throw std::runtime_error("map_3.jpg 이미지 파일을 찾을 수 없습니다!");
        std::vector<cv::Point2f> map_points = getCalibrationPoints(map_image, "Calibration: Click 4 Points on Map");

        cv::Mat cam_frame;
        int retry_count = 0;
        while (retry_count < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            camera.read(cam_frame);
            if (!cam_frame.empty()) break;
            retry_count++;
        }
        if (cam_frame.empty()) throw std::runtime_error("카메라 프레임을 읽어오지 못했습니다!");
        std::vector<cv::Point2f> video_points = getCalibrationPoints(cam_frame, "Calibration: Click 4 Points on Camera (320x240)");

        mapManager.setHomography(video_points, map_points);
        std::cout << "✨ 320x240 카메라 및 map_3.jpg 호모그래피 캘리브레이션 완료!" << std::endl;

        std::thread ai_thread(aiAndTransformThread, "person_detector_v2_best_int8.onnx", std::ref(mapManager));
        ai_thread.detach();

        double speedSetting = 30.0;
        double driveCommand = 0.0;
        double steeringAngle = P2_CENTER;

        cv::Mat frame;
        int frameCounter = 0;
        double measuredFps = 0.0;
        auto fpsStart = std::chrono::steady_clock::now();

        cv::namedWindow("Robot Camera Control", cv::WINDOW_AUTOSIZE);
        cv::namedWindow("RC Car Real-time Monitoring", cv::WINDOW_AUTOSIZE);
        std::cout << "Keyboard input is read from this terminal. Press W/A/S/D without Enter.\n";

        while (running.load()) {
            UartDevice::gpsdata gpsdata;
            {
                std::lock_guard<std::mutex> lock(gpsMutex);
                gpsdata = latestGps;
            }

            if (!camera.read(frame) || frame.empty()) throw systemError("Failed to read camera frame");
            
            {
                std::lock_guard<std::mutex> lock(g_ai_mtx);
                g_latest_frame = frame.clone();
            }

            ++frameCounter;
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - fpsStart).count();
            if (elapsed >= 1.0) {
                measuredFps = frameCounter / elapsed;
                frameCounter = 0;
                fpsStart = now;
            }

            Bno055::Tilt tilt = imu.readMotion();

            cv::Mat display = makeDisplay(frame, measuredFps, tilt, gpsdata, speedSetting, driveCommand, steeringAngle);
            cv::imshow("Robot Camera Control", display);

            bool current_detected;
            cv::Point2f current_map_pos;
            {
                std::lock_guard<std::mutex> lock(g_ai_mtx);
                current_detected = g_person_detected;
                current_map_pos = g_person_map_pos;
            }

            double current_lat = gpsdata.gpsfix ? gpsdata.lat : 37.58635; 
            double current_lon = gpsdata.gpsfix ? gpsdata.lon : 127.09746; 

            cv::Mat display_map = mapManager.drawMarkers(current_lat, current_lon, current_detected, current_map_pos);
            cv::imshow("RC Car Real-time Monitoring", display_map);

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
            else if (key == 'r' || key == 'R') {
                steeringAngle = P2_CENTER;
                servos.setAngle(2, steeringAngle);
            }
            else if (key == 'q' || key == 'Q' || key == 27) {
                break;
            }
        }

        running = false;
        g_ai_running = false;
        motors.stop();
        servos.setAngle(2, P2_CENTER);
        camera.release();
        cv::destroyAllWindows();

        if (gpsThread.joinable()) {
            gpsThread.join();
        }

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