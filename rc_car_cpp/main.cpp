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
#include <cmath>

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

// 전역 변수 및 뮤텍스 선언 (AI 스레드와 공유)
extern std::mutex g_ai_mtx;
extern bool g_person_detected;
extern std::vector<cv::Point2f> g_person_rel_meters;
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

// 스레드 안전 관리를 위한 RAII 래퍼
struct SafeThread {
    std::thread t;
    template<typename... Args>
    SafeThread(Args&&... args) : t(std::forward<Args>(args)...) {}
    ~SafeThread() {
        if (t.joinable()) {
            t.join();
        }
    }
    SafeThread(const SafeThread&) = delete;
    SafeThread& operator=(const SafeThread&) = delete;
};

int main() {
    try {
        I2cDevice i2c(0x14);
        I2cDevice bno055I2c(0x28);
        PwmController pwm(i2c);
        ServoController servos(pwm);
        MotorController motors(pwm);
        Bno055 imu(bno055I2c);
        TerminalInput keyboard;
        PhoneGpsReceiver gps("10.40.202.172", 5000);

        servos.setCalibration(2, { P2_MIN, P2_MAX });
        servos.setAngle(2, P2_CENTER);
        motors.stop();

        // 💡 448x336 해상도 설정 반영
        const int width = 448;
        const int height = 336;
        const int targetFps = 30;

        cv::VideoCapture camera(makePipeline(width, height, targetFps), cv::CAP_GSTREAMER);
        if (!camera.isOpened()) {
            std::cerr << "GStreamer camera open failed. Trying V4L2 index 0.\n";
            camera.open(0, cv::CAP_V4L2);
        }
        if (!camera.isOpened()) throw systemError("Failed to open Raspberry Pi camera");

        // 1. 위성 지도 이미지 로드 및 MapManager 등록
        cv::Mat satelliteMap = cv::imread("map.jpg");
        if (satelliteMap.empty()) throw std::runtime_error("map.jpg image file not found!");

        MapManager mapManager;
        if (!mapManager.loadMap("map.jpg")) {
            throw std::runtime_error("MapManager failed to load map.jpg!");
        }
        
        // 2. 위성 지도의 실제 지리적 경계 설정
        MapManager::GeoPoint map_nw = { 37.587197, 127.097464 };
        MapManager::GeoPoint map_se = { 37.586367, 127.098331 };
        mapManager.setMapGeoBounds(map_nw, map_se, satelliteMap.size());

        // 3. 카메라 초기 프레임 획득 및 자동 노출(AE/AGC) 안정화
        cv::Mat cam_frame;
        std::cout << ">> 카메라 자동 노출(AE) 안정화 중 (약 1.5초)..." << std::endl;
        for (int i = 0; i < 40; ++i) {
            camera.read(cam_frame);
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
        if (cam_frame.empty()) throw std::runtime_error("Failed to read camera frame!");
        std::cout << ">> Actual Camera Resolution: " << cam_frame.cols << "x" << cam_frame.rows << std::endl;

        // 4. 전방 바닥 기준점 캘리브레이션
        std::vector<cv::Point2f> groundMeters = {
            {1.0f, -0.5f}, // 1) 전방 1m 좌측
            {1.0f,  0.5f}, // 2) 전방 1m 우측
            {3.0f, -0.5f}, // 3) 전방 3m 좌측
            {3.0f,  0.5f}  // 4) 전방 3m 우측
        };

        // 💡 448x336 해상도에 맞춰 1.4배 자동 스케일링된 하드코딩 좌표
        std::vector<cv::Point2f> camPoints = {
            {133.0f, 259.0f}, // 1번: 전방 1m 좌측
            {315.0f, 259.0f}, // 2번: 전방 1m 우측
            {189.0f, 147.0f}, // 3번: 전방 3m 좌측
            {259.0f, 147.0f}  // 4번: 전방 3m 우측
        };

        mapManager.calibrateCameraToMeters(camPoints, groundMeters);
        std::cout << ">> Camera-to-Meters ground calibration completed (448x336 Scaled)!" << std::endl;

        // 5. GUI 윈도우 생성 및 초기 화면 갱신
        cv::namedWindow("Robot Camera Control", cv::WINDOW_AUTOSIZE);
        cv::namedWindow("RC Car Real-time Monitoring", cv::WINDOW_AUTOSIZE);
        cv::imshow("Robot Camera Control", cam_frame);
        cv::imshow("RC Car Real-time Monitoring", satelliteMap);
        cv::waitKey(10);

        std::cout << "Keyboard input is read from this terminal. Press W/A/S/D without Enter.\n";

        std::atomic<bool> running{true};
        g_ai_running = true;

        // 6. 백그라운드 스레드 시작
        SafeThread gpsThread(gpsWorker, std::ref(gps), std::ref(running));

        SafeThread ai_thread([](MapManager& mgr) {
            try {
                aiAndTransformThread("S1_HN2_U8_E7_320_Q3Q4_INT8.onnx", mgr);
            } catch (const std::exception& e) {
                std::cerr << "AI thread exception occurred: " << e.what() << std::endl;
            }
        }, std::ref(mapManager));

        double speedSetting = 30.0;
        double driveCommand = 0.0;
        double steeringAngle = P2_CENTER;

        double currentSpeed = 0.0;
        double previousLat = 0.0;
        double previousLon = 0.0;
        bool hasPreviousGps = false;
        auto previousGpsTime = std::chrono::steady_clock::now();

        cv::Mat frame;
        int frameCounter = 0;
        double measuredFps = 0.0;
        auto fpsStart = std::chrono::steady_clock::now();

        while (running.load() && g_ai_running.load()) {
            UartDevice::gpsdata gpsdata;
            {
                std::lock_guard<std::mutex> lock(gpsMutex);
                gpsdata = latestGps;
            }

            // GPS 속도 계산
            if (gpsdata.gpsfix) {
                auto currentGpsTime = std::chrono::steady_clock::now();

                if (hasPreviousGps) {
                    if (gpsdata.lat != previousLat || gpsdata.lon != previousLon) {
                        double dt = std::chrono::duration<double>(currentGpsTime - previousGpsTime).count();
                        if (dt > 0.0) {
                            double lat1 = previousLat * CV_PI / 180.0;
                            double lat2 = gpsdata.lat * CV_PI / 180.0;
                            double dLat = (gpsdata.lat - previousLat) * CV_PI / 180.0;
                            double dLon = (gpsdata.lon - previousLon) * CV_PI / 180.0;
                            const double earthRadius = 6371000.0;
                            double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) + std::cos(lat1) * std::cos(lat2) * std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
                            double distance = 2.0 * earthRadius * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

                            currentSpeed = distance / dt;
                            previousLat = gpsdata.lat;
                            previousLon = gpsdata.lon;
                            previousGpsTime = currentGpsTime;
                        }
                    } else {
                        double dt_idle = std::chrono::duration<double>(currentGpsTime - previousGpsTime).count();
                        if (dt_idle > 1.5) {
                            currentSpeed = 0.0;
                        }
                    }
                } else {
                    previousLat = gpsdata.lat;
                    previousLon = gpsdata.lon;
                    previousGpsTime = currentGpsTime;
                    hasPreviousGps = true;
                }
            }

            if (!camera.read(frame) || frame.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            
            {
                std::lock_guard<std::mutex> lock(g_ai_mtx);
                g_latest_frame = frame.clone();
            }

            bool current_detected;
            std::vector<cv::Point2f> current_rel_meters;
            std::vector<cv::Rect> current_boxes;
            {
                std::lock_guard<std::mutex> lock(g_ai_mtx);
                current_detected = g_person_detected;
                current_rel_meters = g_person_rel_meters;
                current_boxes = g_person_boxes;
            }

            // 카메라 화면에 검출 바운딩 박스 표시
            if (current_detected) {
                for (size_t i = 0; i < current_boxes.size(); ++i) {
                    const auto& box = current_boxes[i];
                    cv::rectangle(frame, box, cv::Scalar(0, 255, 0), 2);
                    
                    std::string label = "PERSON " + std::to_string(i + 1);
                    cv::putText(frame, label, cv::Point(box.x, std::max(box.y - 5, 15)), 
                                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
                }
            }

            ++frameCounter;
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - fpsStart).count();
            if (elapsed >= 1.0) {
                measuredFps = frameCounter / elapsed;
                frameCounter = 0;
                fpsStart = now;
            }

            // IMU 센서 읽기
            Bno055::Tilt tilt = imu.readMotion();
            double currentHeading = tilt.headingDeg;

            // 카메라 제어 UI 렌더링
            cv::Mat display = makeDisplay(frame, measuredFps, tilt, gpsdata, speedSetting, driveCommand, steeringAngle, currentSpeed);
            if (!display.empty()) {
                cv::imshow("Robot Camera Control", display);
            }

            // 실시간 지도 렌더링
            cv::Mat display_map;
            if (gpsdata.gpsfix) {
                MapManager::GeoPoint carGeo{ gpsdata.lat, gpsdata.lon };
                display_map = mapManager.drawMarkers(carGeo, currentHeading, current_detected, current_rel_meters);
            } else {
                display_map = satelliteMap.clone();
                cv::putText(display_map, "Waiting for GPS Fix...", cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
            }

            if (!display_map.empty()) {
                cv::imshow("RC Car Real-time Monitoring", display_map);
            }

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

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
