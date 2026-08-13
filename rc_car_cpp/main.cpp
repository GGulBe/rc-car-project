#include <cerrno>
#include <cstring>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <thread>
#include <mutex>
 
#include "MotorController.h"
#include "PwmController.h"
#include "ServoController.h"
#include "TerminalInput.h"
#include "I2CDevice.h"
#include "util.h"
#include "Bno055.h"
#include "MapManager.h"
#include "Detector.h"

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

// 멀티스레드 간 데이터 보호를 위한 뮤텍스 및 공유 전역 변수
std::mutex g_mtx;
cv::Mat g_latest_frame;
bool g_person_detected = false;
cv::Point2f g_person_map_pos(-1, -1);
bool g_running = true;

// [스레드 분할] 객체 탐지 및 좌표 변환 전용 백그라운드 스레드
// (AI 추론뿐만 아니라 호모그래피 좌표 변환까지 스레드로 빼내어 메인 제어 루프의 부하를 최소화)
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
            // 백그라운드 스레드에서 곧바로 호모그래피 행렬을 이용해 위성지도 좌표로 변환[cite: 1]
            transformed_pos = mapManager.transformToMap(bottom_center);
        }

        // 결과를 안전하게 공유 변수에 반영
        {
            std::lock_guard<std::mutex> lock(g_mtx);
            g_person_detected = detected;
            g_person_map_pos = transformed_pos;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20)); // RPi4 CPU 부하 방지
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
        imu.initialize();

        servos.setCalibration(0, { P0_MIN, P0_MAX});
        servos.setCalibration(1, { P1_MIN, P1_MAX });
        servos.setCalibration(2, { P2_MIN, P2_MAX});

        // 카메라 화각 고정 (팬/틸트 값 고정)
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

        // 위성지도 관리자 초기화
        MapManager mapManager("map.jpg");

        // [요구사항 1] 프로그램 시작 시 1회 호모그래피 캘리브레이션 진행[cite: 1]
        // (실제 시연 운동장의 위성지도 픽셀 점들과 카메라 영상 속 점들을 매칭하여 입력)
        std::vector<cv::Point2f> video_points = { {100, 100}, {500, 100}, {500, 500}, {100, 500} };
        std::vector<cv::Point2f> map_points   = { {200, 200}, {600, 200}, {600, 600}, {200, 600} };
        mapManager.setHomography(video_points, map_points);
        std::cout << "✨ 1회 호모그래피 캘리브레이션 및 매트릭스 계산 완료!" << std::endl;

        // [요구사항 2] 객체 탐지 및 좌표 변환 스레드 백그라운드 구동
        std::thread background_thread(aiAndTransformThread, "person_detector_v2_best_int8.onnx", std::ref(mapManager));
        background_thread.detach();

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
        // [요구사항 3] 위성지도 모니터링 창 활성화 (총 2개의 창)
        cv::namedWindow("RC Car Real-time Monitoring", cv::WINDOW_AUTOSIZE);
        
        std::cout << "Keyboard input is read from this terminal. Press W/A/S/D without Enter.\n";

        while (g_running) {
            if (!camera.read(frame) || frame.empty()) throw systemError("Failed to read camera frame");
            
            // 최신 프레임을 백그라운드 AI 스레드에 전달하기 위해 공유 변수에 갱신
            {
                std::lock_guard<std::mutex> lock(g_mtx);
                g_latest_frame = frame.clone();
            }

            // FPS 측정용
            frameCounter++;

            // IMU 센서 데이터 읽기[cite: 4]
            const Bno055::Tilt tilt = imu.readMotion();
            std::cout << "heading : " << tilt.headingDeg << " roll : " << tilt.rollDeg << " pitch : " << tilt.pitchDeg << std::endl; 
            
            const auto now = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double>(now - fpsStart).count();
            if (elapsed >= 1.0) {
                measuredFps = frameCounter / elapsed;
                frameCounter = 0;
                fpsStart = now;
            }
            
            // 창 1: 카메라 뷰어 출력[cite: 4]
            cv::Mat display = frame.clone();
            drawStatus(display, speedSetting, driveCommand, steeringAngle, cameraPan, cameraTilt, measuredFps);
            cv::imshow("Robot Camera Control", display);

            // [요구사항 3] 백그라운드 스레드에서 처리된 결과를 안전하게 가져와 위성지도 마커 토글 반영
            bool current_detected;
            cv::Point2f current_map_pos;
            {
                std::lock_guard<std::mutex> lock(g_mtx);
                current_detected = g_person_detected;
                current_map_pos = g_person_map_pos;
            }

            double current_lat = 37.58680; 
            double current_lon = 127.09790; 

            // 창 2: 위성 뷰어 출력 (사람이 감지되면 마커 표시, 사라지면 자동으로 제거)
            cv::Mat display_map = mapManager.drawMarkers(current_lat, current_lon, current_detected, current_map_pos);
            cv::imshow("RC Car Real-time Monitoring", display_map);

            const int windowKey = cv::waitKey(1);
            int key = keyboard.readKey(0);
            if (key < 0 && windowKey >= 0) key = windowKey & 0xFF;
            if (key < 0) continue;

            // 키보드 제어 로직 (WASD 등) 유지[cite: 4]
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
                g_running = false;
                break;
            }
        }

        g_running = false;
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