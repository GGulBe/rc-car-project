#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <chrono>
#include <string>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <mutex>
#include <vector>
 
#include "MotorController.h"
#include "PwmController.h"
#include "ServoController.h"
#include "TerminalInput.h"
#include "I2CDevice.h"
#include "util.h"
#include "Bno055.h"
#include "UartDevice.h"
<<<<<<< Updated upstream
#include "MapManager.h"
#include "Detector.h"
=======
#include "GPSWorker.h"
#include "MapManager.h"
#include "CalibrationWorker.h"
>>>>>>> Stashed changes

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

std::mutex g_mtx;
cv::Mat g_latest_frame;
bool g_person_detected = false;
cv::Point2f g_person_map_pos(-1, -1);
UartDevice::gpsdata g_latest_gps{};
bool g_running = true;

struct ClickContext {
    std::vector<cv::Point2f> points;
    std::string window_name;
    cv::Mat image;
};

void mouseCallback(int event, int x, int y, int flags, void* userdata) {
    if (event == cv::EVENT_LBUTTONDOWN) {
        ClickContext* ctx = reinterpret_cast<ClickContext*>(userdata);
        if (ctx->points.size() < 4) {
            ctx->points.push_back(cv::Point2f(static_cast<float>(x), static_cast<float>(y)));
            std::cout << "[" << ctx->window_name << "] 선택된 픽셀 좌표 (" << ctx->points.size() << "/4): " 
                      << x << ", " << y << std::endl;
            
            cv::circle(ctx->image, cv::Point(x, y), 5, cv::Scalar(0, 255, 0), -1);
            cv::putText(ctx->image, std::to_string(ctx->points.size()), cv::Point(x + 8, y - 8),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
            cv::imshow(ctx->window_name, ctx->image);
        }
    }
}

std::vector<cv::Point2f> getCalibrationPoints(cv::Mat& img, const std::string& win_name) {
    ClickContext context;
    context.window_name = win_name;
    context.image = img.clone();

    cv::namedWindow(win_name, cv::WINDOW_AUTOSIZE);
    cv::imshow(win_name, context.image);
    cv::setMouseCallback(win_name, mouseCallback, &context);

    std::cout << "\n=== " << win_name << " 창에서 대응점 4개를 순서대로 클릭하세요! ===" << std::endl;
    std::cout << "(예시 순서: 1.왼쪽 위 -> 2.오른쪽 위 -> 3.오른쪽 아래 -> 4.왼쪽 아래)" << std::endl;

    while (true) {
        int key = cv::waitKey(10);
        if (context.points.size() >= 4) {
            std::cout << win_name << " 4개 점 수집 완료!\n" << std::endl;
            break;
        }
        if (key == 'q' || key == 27) {
            std::cerr << "캘리브레이션이 취소되었습니다." << std::endl;
            break;
        }
    }

    cv::destroyWindow(win_name);
    return context.points;
}

void aiAndTransformThread(const std::string& model_path, MapManager& mapManager) {
    Detector detector(model_path, 0.3f);
    cv::Mat target_frame;

    while (g_running) {
        {
            std::lock_guard<std::mutex> lock(g_mtx);
            if (g_latest_frame.empty()) continue;
            target_frame = g_latest_frame.clone();
        }

        cv::Point2f bottom_center(0, 0);
        bool detected = detector.detectPerson(target_frame, bottom_center);

        cv::Point2f transformed_pos(-1, -1);
        if (detected) {
            transformed_pos = mapManager.transformToMap(bottom_center);
        }

        {
            std::lock_guard<std::mutex> lock(g_mtx);
            g_person_detected = detected;
            g_person_map_pos = transformed_pos;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void gpsReadThread() {
    try {
        UartDevice gps;
        while (g_running) {
            std::string rmsline = gps.readRmc();
            UartDevice::gpsdata data = gps.parseRmc(rmsline);
            
            {
                std::lock_guard<std::mutex> lock(g_mtx);
                g_latest_gps = data;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "GPS Thread Error: " << e.what() << std::endl;
    }
}

int main() {
    try {
        I2cDevice i2c(0x14);
        I2cDevice bno055I2c(0x28);
        PwmController pwm(i2c);
        ServoController servos(pwm);
        MotorController motors(pwm);

        Bno055 imu(bno055I2c);
<<<<<<< Updated upstream
=======
        UartDevice gps;
        TerminalInput keyboard;

>>>>>>> Stashed changes
        imu.initialize();

        servos.setCalibration(0, { P0_MIN, P0_MAX});
        servos.setCalibration(1, { P1_MIN, P1_MAX });
        servos.setCalibration(2, { P2_MIN, P2_MAX});

        servos.setAngle(0, P0_CENTER);
        servos.setAngle(1, P1_CENTER);
        servos.setAngle(2, P2_CENTER);
        motors.stop();

<<<<<<< Updated upstream
=======
        std::atomic<bool> running{true};
        std::thread gpsThread(gpsWorker, std::ref(gps), std::ref(running));

>>>>>>> Stashed changes
        constexpr int width = 320;
        constexpr int height = 240;
        constexpr int targetFps = 30;

<<<<<<< Updated upstream
        // V4L2 방식으로 카메라 열기
        cv::VideoCapture camera(0, cv::CAP_V4L2);
        camera.set(cv::CAP_PROP_FRAME_WIDTH, width);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        camera.set(cv::CAP_PROP_FPS, targetFps);

        if (!camera.isOpened()) throw systemError("Failed to open Raspberry Pi camera via V4L2");

        MapManager mapManager("map_3.jpg");

        cv::Mat map_image = cv::imread("map_3.jpg");
        if (map_image.empty()) throw std::runtime_error("map_3.jpg 이미지 파일을 찾을 수 없습니다!");
        std::vector<cv::Point2f> map_points = getCalibrationPoints(map_image, "Calibration: Click 4 Points on Map");

        cv::Mat cam_frame;
        int retry_count = 0;
        
        while (retry_count < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            camera.read(cam_frame);
            if (!cam_frame.empty()) {
                break;
            }
            retry_count++;
        }

        if (cam_frame.empty()) throw std::runtime_error("카메라 프레임을 읽어오지 못했습니다!");
        std::vector<cv::Point2f> video_points = getCalibrationPoints(cam_frame, "Calibration: Click 4 Points on Camera (320x240)");

        mapManager.setHomography(video_points, map_points);
        std::cout << "✨ 320x240 카메라 및 map_3.jpg 캘리브레이션 완료!" << std::endl;

        std::thread ai_thread(aiAndTransformThread, "person_detector_v2_best_int8.onnx", std::ref(mapManager));
        ai_thread.detach();

        std::thread gps_thread(gpsReadThread);
        gps_thread.detach();

        double speedSetting = 30.0;
        double driveCommand = 0.0;
        double steeringAngle = P2_CENTER;
        double cameraPan = P0_CENTER;
        double cameraTilt = P1_CENTER;
=======
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
>>>>>>> Stashed changes

        cv::Mat frame;
        int frameCounter = 0;
        double measuredFps = 0.0;
        auto fpsStart = std::chrono::steady_clock::now();

<<<<<<< Updated upstream
        TerminalInput keyboard;
        cv::namedWindow("Robot Camera Control", cv::WINDOW_AUTOSIZE);
        cv::namedWindow("RC Car Real-time Monitoring", cv::WINDOW_AUTOSIZE);
        
        std::cout << "Keyboard input is read from this terminal. Press W/A/S/D without Enter.\n";

        while (g_running) {
            if (!camera.read(frame) || frame.empty()) throw systemError("Failed to read camera frame");
            
=======
        cv::namedWindow("Robot Camera Control", cv::WINDOW_AUTOSIZE);
        cv::namedWindow("RC Car Real-time Monitoring", cv::WINDOW_AUTOSIZE);
        std::cout << "Keyboard input is read from this terminal. Press W/A/S/D without Enter.\n";

        while (running.load()) {
            UartDevice::gpsdata gpsdata;
>>>>>>> Stashed changes
            {
                std::lock_guard<std::mutex> lock(g_mtx);
                g_latest_frame = frame.clone();
            }

<<<<<<< Updated upstream
            frameCounter++;

            const Bno055::Tilt tilt = imu.readMotion();
            std::cout << "heading : " << tilt.headingDeg << " roll : " << tilt.rollDeg << " pitch : " << tilt.pitchDeg << std::endl; 
            
=======
            if (!camera.read(frame) || frame.empty()) throw systemError("Failed to read camera frame");
            
            {
                std::lock_guard<std::mutex> lock(g_ai_mtx);
                g_latest_frame = frame.clone();
            }

            ++frameCounter;
>>>>>>> Stashed changes
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - fpsStart).count();
            if (elapsed >= 1.0) {
                measuredFps = frameCounter / elapsed;
                frameCounter = 0;
                fpsStart = now;
            }
            
            cv::Mat display = frame.clone();
            drawStatus(display, speedSetting, driveCommand, steeringAngle, cameraPan, cameraTilt, measuredFps);

            bool current_detected;
            cv::Point2f current_map_pos;
            UartDevice::gpsdata current_gps;
            {
                std::lock_guard<std::mutex> lock(g_mtx);
                current_detected = g_person_detected;
                current_map_pos = g_person_map_pos;
                current_gps = g_latest_gps;
            }

            double current_lat = current_gps.gpsfix ? current_gps.lat : 37.58635; 
            double current_lon = current_gps.gpsfix ? current_gps.lon : 127.09746; 

            // 카메라 뷰어 화면에 실시간 GPS 좌표 출력
            std::ostringstream gpsText;
            gpsText << std::fixed << std::setprecision(5)
                    << "GPS: " << current_lat << ", " << current_lon 
                    << (current_gps.gpsfix ? " (FIX)" : " (NO FIX)");

            cv::putText(
                display,
                gpsText.str(),
                cv::Point(10, 20),          // 출력 위치 (좌측 상단)
                cv::FONT_HERSHEY_SIMPLEX,
                0.4,                        // 폰트 크기
                cv::Scalar(0, 255, 0),      // 초록색 글씨
                1                           // 두께
            );

<<<<<<< Updated upstream
            cv::imshow("Robot Camera Control", display);

=======
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

>>>>>>> Stashed changes
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
<<<<<<< Updated upstream
            else if (key == 'p' || key == 'P') {
                const std::string filename = makePhotoFilename();
                if (cv::imwrite(filename, frame)) std::cout << "Saved: " << filename << '\n';
                else std::cerr << "Failed to save image\n";
            }
=======
>>>>>>> Stashed changes
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
                g_running = false;
                break;
            }
        }

<<<<<<< Updated upstream
        g_running = false;
=======
        running = false;
        g_ai_running = false;
>>>>>>> Stashed changes
        motors.stop();
        servos.setAngle(2, P2_CENTER);
        camera.release();
        cv::destroyAllWindows();
<<<<<<< Updated upstream
=======

        if (gpsThread.joinable()) {
            gpsThread.join();
        }

>>>>>>> Stashed changes
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