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

// 전역 변수 및 뮤텍스 선언 (AI 스레드와 공유)
extern std::mutex g_ai_mtx;
extern bool g_person_detected;
extern std::vector<cv::Point2f> g_person_rel_meters; // 카메라 기준 상대 미터 좌표
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

int main() {
    try {
        I2cDevice i2c(0x14);
        I2cDevice bno055I2c(0x28);
        PwmController pwm(i2c);
        ServoController servos(pwm);
        MotorController motors(pwm);
        Bno055 imu(bno055I2c); // 내부적으로 MODE_NDOF(9축 절대 방위각)로 초기화됨[cite: 1]
        TerminalInput keyboard;
        PhoneGpsReceiver gps("10.40.202.172", 5000);

        servos.setCalibration(2, { P2_MIN, P2_MAX });
        servos.setAngle(2, P2_CENTER);
        motors.stop();

        const int width = 320;
        const int height = 240;
        const int targetFps = 30;

        cv::VideoCapture camera(makePipeline(width, height, targetFps), cv::CAP_GSTREAMER);
        if (!camera.isOpened()) {
            std::cerr << "GStreamer camera open failed. Trying V4L2 index 0.\n";
            camera.open(0, cv::CAP_V4L2);
        }
        if (!camera.isOpened()) throw systemError("Failed to open Raspberry Pi camera");

        // 1. 위성 지도 이미지 로드
        cv::Mat satelliteMap = cv::imread("map.jpg");
        if (satelliteMap.empty()) throw std::runtime_error("map.jpg image file not found!");

        MapManager mapManager;
        
        // 2. 위성 지도의 실제 지리적 경계(좌상단 NW, 우하단 SE 위경도) 설정
        // ※ 사용하시는 운동장 위성 사진의 실제 위경도 값으로 반드시 수정해주세요!
        MapManager::GeoPoint map_nw = { 37.587500, 127.098000 };
        MapManager::GeoPoint map_se = { 37.586500, 127.099500 };
        mapManager.setMapGeoBounds(map_nw, map_se, satelliteMap.size());

        // 3. 카메라 초기 프레임 획득
        cv::Mat cam_frame;
        int retry_count = 0;
        while (retry_count < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            camera.read(cam_frame);
            if (!cam_frame.empty()) break;
            retry_count++;
        }
        if (cam_frame.empty()) throw std::runtime_error("Failed to read camera frame!");

        // 4. 전방 바닥 기준점 캘리브레이션 (카메라 화소 -> RC카 기준 미터 변환)
        std::vector<cv::Point2f> camPoints = getCalibrationPoints(cam_frame, "Calibration: Click 4 Points on Ground");
        std::vector<cv::Point2f> groundMeters = {
            {1.0f, -0.5f}, // 1) 전방 1m, 좌측 0.5m
            {1.0f,  0.5f}, // 2) 전방 1m, 우측 0.5m
            {3.0f, -0.5f}, // 3) 전방 3m, 좌측 0.5m
            {3.0f,  0.5f}  // 4) 전방 3m, 우측 0.5m
        };
        mapManager.calibrateCameraToMeters(camPoints, groundMeters);
        std::cout << "Camera-to-Meters ground calibration completed!" << std::endl;

        std::atomic<bool> running{true};
        g_ai_running = true;
        std::thread gpsThread(gpsWorker, std::ref(gps), std::ref(running));

        // AI 추론 및 상대 미터 변환 스레드 시작
        std::thread ai_thread([](MapManager& mgr) {
            try {
                aiAndTransformThread("results4_fixed.onnx", mgr);
            } catch (const std::exception& e) {
                std::cerr << "AI thread exception exception occurred: " << e.what() << std::endl;
            }
        }, std::ref(mapManager));

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

        while (running.load() && g_ai_running.load()) {
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

            bool current_detected;
            std::vector<cv::Point2f> current_rel_meters;
            std::vector<cv::Rect> current_boxes;
            {
                std::lock_guard<std::mutex> lock(g_ai_mtx);
                current_detected = g_person_detected;
                current_rel_meters = g_person_rel_meters;
                current_boxes = g_person_boxes;
            }

            // 카메라 화면에 감지된 객체 바운딩 박스 표시
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

            // IMU로부터 절대 방위각(Heading) 읽기[cite: 1]
            Bno055::Tilt tilt = imu.readMotion();
            double currentHeading = tilt.headingDeg;

            cv::Mat display = makeDisplay(frame, measuredFps, tilt, gpsdata, speedSetting, driveCommand, steeringAngle);
            cv::imshow("Robot Camera Control", display);

            // 위성 지도 복사본 생성 (실시간 마킹용)
            cv::Mat display_map = satelliteMap.clone();

            if (gpsdata.gpsfix) {
                MapManager::GeoPoint carGeo{ gpsdata.lat, gpsdata.lon };
                
                // 1. RC카 현재 위치 마킹 (파란색 점)
                cv::Point carPx = mapManager.geoToMapPixel(carGeo);
                cv::circle(display_map, carPx, 6, cv::Scalar(255, 0, 0), -1);
                cv::putText(display_map, "RC CAR", carPx + cv::Point(8, -4), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 0, 0), 1);

                // 2. 감지된 사람의 실제 위경도 계산 및 마킹 (빨간색 점)
                if (current_detected) {
                    for (const auto& rel_m : current_rel_meters) {
                        // (차량 GPS + 절대 방위각 + 카메라 상대 거리)를 결합하여 사람의 실제 위경도 산출
                        MapManager::GeoPoint personGeo = mapManager.calculateTargetGeo(carGeo, currentHeading, rel_m);
                        
                        // 위경도를 위성 지도 픽셀 좌표로 변환
                        cv::Point personPx = mapManager.geoToMapPixel(personGeo);
                        
                        // 지도 위에 마커 표시
                        cv::circle(display_map, personPx, 6, cv::Scalar(0, 0, 255), -1);
                        cv::putText(display_map, "PERSON", personPx + cv::Point(8, -4), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 255), 1);
                    }
                }
            } else {
                cv::putText(display_map, "Waiting for GPS Fix...", cv::Point(20, 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
            }

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

        if (ai_thread.joinable()) {
            ai_thread.join(); 
        }

        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}