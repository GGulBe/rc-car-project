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
#include <vector>

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
#include "Detector.h"
#include "PhoneGpsReceiver.h"

// 다중 객체 좌표 및 박스를 공유받기 위한 전역 변수 선언
extern std::mutex g_ai_mtx;
extern bool g_person_detected;
extern std::vector<cv::Point2f> g_person_map_positions;
extern std::vector<cv::Rect> g_person_boxes;
extern cv::Mat g_latest_frame;
extern std::atomic<bool> g_ai_running;

const double P0_CENTER = -80.0;
const double P1_CENTER = 0.0;
const double P2_CENTER = -75.0;
const double P0_MIN = -140.0;
const double P0_MAX = 20.0;
const double P1_MIN = -35.0;
const double P1_MAX = 65.0;
const double P2_MIN = -100.0;
const double P2_MAX = -50.0;
const double CAMERA_STEP = 5.0;
const double STEERING_STEP = 1.0;

int main()
{
    try
    {
        I2cDevice i2c(0x14);
        I2cDevice bno055I2c(0x28);
        PwmController pwm(i2c);
        ServoController servos(pwm);
        MotorController motors(pwm);
        Bno055 imu(bno055I2c);
        //UartDevice gps;
        TerminalInput keyboard;
        PhoneGpsReceiver gps;

        servos.setCalibration(2, {P2_MIN, P2_MAX});
        servos.setAngle(2, P2_CENTER);
        motors.stop();

        const int width = 320;
        const int height = 240;
        const int targetFps = 30;

        cv::VideoCapture camera(makePipeline(width, height, targetFps), cv::CAP_GSTREAMER);
        if (!camera.isOpened())
        {
            std::cerr << "GStreamer camera open failed. Trying V4L2 index 0.\n";
            camera.open(0, cv::CAP_V4L2);
        }
        if (!camera.isOpened())
            throw systemError("Failed to open Raspberry Pi camera");

        MapManager mapManager;
        if (!mapManager.loadMap("map.jpg"))
        {
            throw std::runtime_error("map.jpg image file not found!");
        }

        cv::Mat map_image = cv::imread("map.jpg");
        if (map_image.empty())
            throw std::runtime_error("map.jpg image file not found!");

        std::vector<cv::Point2f> map_points = getCalibrationPoints(map_image, "Calibration: Click 4 Points on Map");

        cv::Mat cam_frame;
        int retry_count = 0;
        while (retry_count < 10)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            camera.read(cam_frame);
            if (!cam_frame.empty())
                break;
            retry_count++;
        }
        if (cam_frame.empty())
            throw std::runtime_error("Failed to read camera frame!");

        std::vector<cv::Point2f> video_points = getCalibrationPoints(cam_frame, "Calibration: Click 4 Points on Camera (320x240)");

        mapManager.setHomography(video_points, map_points);
        std::cout << "320x240 Camera and map.jpg homography calibration completed!" << std::endl;

        std::atomic<bool> running{true};
        g_ai_running = true;
        std::thread gpsThread(gpsWorker, std::ref(gps), std::ref(running));

        std::thread ai_thread([](MapManager &mgr)
                              {
            try {
                aiAndTransformThread("results4_fixed.onnx", mgr);
            } catch (const std::exception& e) {
                std::cerr << "AI thread exception occurred: " << e.what() << std::endl;
            } }, std::ref(mapManager));

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

        while (running.load() && g_ai_running.load())
        {
            UartDevice::gpsdata gpsdata;
            {
                std::lock_guard<std::mutex> lock(gpsMutex);
                gpsdata = latestGps;
            }

            if (!camera.read(frame) || frame.empty())
            {
                std::cerr << "[Warning] Failed to read camera frame, retrying..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            {
                std::lock_guard<std::mutex> lock(g_ai_mtx);
                g_latest_frame = frame.clone();
            }

            bool current_detected;
            std::vector<cv::Point2f> current_map_positions;
            std::vector<cv::Rect> current_boxes;
            {
                std::lock_guard<std::mutex> lock(g_ai_mtx);
                current_detected = g_person_detected;
                current_map_positions = g_person_map_positions;
                current_boxes = g_person_boxes;
            }

            if (current_detected)
            {
                for (size_t i = 0; i < current_boxes.size(); ++i)
                {
                    const auto &box = current_boxes[i];
                    cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
                    std::string label = "PERSON " + std::to_string(i + 1);
                    cv::putText(frame, label, cv::Point(box.x, std::max(box.y - 5, 15)),
                                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
                }
            }

            ++frameCounter;
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - fpsStart).count();
            if (elapsed >= 1.0)
            {
                measuredFps = frameCounter / elapsed;
                frameCounter = 0;
                fpsStart = now;
            }

            Bno055::Tilt tilt = imu.readMotion();

            cv::Mat display = makeDisplay(frame, measuredFps, tilt, gpsdata, speedSetting, driveCommand, steeringAngle);
            cv::imshow("Robot Camera Control", display);

            double current_lat = gpsdata.gpsfix ? gpsdata.lat : 37.58635;
            double current_lon = gpsdata.gpsfix ? gpsdata.lon : 127.09746;

            cv::Mat display_map = mapManager.drawMarkers(current_lat, current_lon, current_detected, current_map_positions);
            if (!display_map.empty())
            {
                cv::imshow("RC Car Real-time Monitoring", display_map);
            }

            const int windowKey = cv::waitKey(1);
            int key = keyboard.readKey(0);
            if (key < 0 && windowKey >= 0)
                key = windowKey & 0xFF;
            if (key < 0)
                continue;

            if (key == 'w' || key == 'W')
            {
                driveCommand = speedSetting;
                motors.drive(driveCommand);
            }
            else if (key == 's' || key == 'S')
            {
                driveCommand = -speedSetting;
                motors.drive(driveCommand);
            }
            else if (key == ' ')
            {
                driveCommand = 0.0;
                motors.stop();
            }
            else if (key == 'a' || key == 'A')
            {
                steeringAngle = std::max(P2_MIN, steeringAngle - STEERING_STEP);
                servos.setAngle(2, steeringAngle);
            }
            else if (key == 'd' || key == 'D')
            {
                steeringAngle = std::min(P2_MAX, steeringAngle + STEERING_STEP);
                servos.setAngle(2, steeringAngle);
            }
            else if (key == 'x' || key == 'X')
            {
                steeringAngle = P2_CENTER;
                servos.setAngle(2, steeringAngle);
            }
            else if (key == '+' || key == '=')
            {
                speedSetting = std::min(100.0, speedSetting + 5.0);
                if (driveCommand != 0.0)
                {
                    driveCommand = driveCommand > 0.0 ? speedSetting : -speedSetting;
                    motors.drive(driveCommand);
                }
            }
            else if (key == '-' || key == '_')
            {
                speedSetting = std::max(10.0, speedSetting - 5.0);
                if (driveCommand != 0.0)
                {
                    driveCommand = driveCommand > 0.0 ? speedSetting : -speedSetting;
                    motors.drive(driveCommand);
                }
            }
            else if (key == 'r' || key == 'R')
            {
                steeringAngle = P2_CENTER;
                servos.setAngle(2, steeringAngle);
            }
            else if (key == 'q' || key == 'Q' || key == 27)
            {
                break;
            }
        }

        running = false;
        g_ai_running = false;

        motors.stop();
        servos.setAngle(2, P2_CENTER);
        camera.release();
        cv::destroyAllWindows();

        if (gpsThread.joinable())
        {
            gpsThread.join();
        }

        if (ai_thread.joinable())
        {
            ai_thread.join();
        }

        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
