#include "CalibrationWorker.h"
#include "Detector.h"
#include <iostream>
#include <chrono>
#include <thread>

// 전역 변수 정의
std::mutex g_ai_mtx;
cv::Mat g_latest_frame;
bool g_person_detected = false;
std::vector<cv::Point2f> g_person_rel_meters; // 상대 거리 벡터 정의
std::vector<cv::Rect> g_person_boxes;
std::atomic<bool> g_ai_running{true};

struct ClickContext {
    std::vector<cv::Point2f> points;
    std::string window_name;
    cv::Mat image;
};

static void mouseCallback(int event, int x, int y, int flags, void* userdata) {
    if (event == cv::EVENT_LBUTTONDOWN) {
        auto* context = static_cast<ClickContext*>(userdata);
        if (context->points.size() < 4) {
            context->points.emplace_back(static_cast<float>(x), static_cast<float>(y));
            std::cout << "포인트 선택 [" << context->points.size() << "/4]: (" << x << ", " << y << ")" << std::endl;
        }
    }
}

std::vector<cv::Point2f> getCalibrationPoints(cv::Mat& img, const std::string& win_name) {
    ClickContext context;
    context.window_name = win_name;
    context.image = img.clone();

    cv::namedWindow(win_name, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(win_name, mouseCallback, &context);

    std::cout << "\n=======================================================" << std::endl;
    std::cout << win_name << " 창에서 전방 바닥 마커 4개를 순서대로 클릭하세요." << std::endl;
    std::cout << "1) 전방 1m 좌측 (-0.5m)" << std::endl;
    std::cout << "2) 전방 1m 우측 (+0.5m)" << std::endl;
    std::cout << "3) 전방 3m 좌측 (-0.5m)" << std::endl;
    std::cout << "4) 전방 3m 우측 (+0.5m)" << std::endl;
    std::cout << "=======================================================\n" << std::endl;

    while (true) {
        cv::Mat display = context.image.clone();
        for (size_t i = 0; i < context.points.size(); ++i) {
            cv::circle(display, context.points[i], 6, cv::Scalar(0, 0, 255), -1);
            cv::putText(display, std::to_string(i + 1), cv::Point(context.points[i].x + 8, context.points[i].y - 8),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
        }

        cv::imshow(win_name, display);
        int key = cv::waitKey(30);

        if (context.points.size() >= 4) {
            std::cout << "4개 포인트 선택 완료!" << std::endl;
            cv::waitKey(500);
            break;
        }
        if (key == 27) { // ESC
            std::cout << "캘리브레이션 중단됨" << std::endl;
            break;
        }
    }

    cv::destroyWindow(win_name);
    return context.points;
}

void aiAndTransformThread(const std::string& model_path, MapManager& mapManager) {
    try {
        Detector detector(model_path, 0.3f, 0.45f);

        std::vector<cv::Point2f> bottom_centers;
        std::vector<cv::Rect> detected_boxes;
        std::vector<float> confidences;

        while (g_ai_running) {
            try {
                cv::Mat target_frame;

                {
                    std::lock_guard<std::mutex> lock(g_ai_mtx);
                    if (!g_latest_frame.empty()) {
                        target_frame = g_latest_frame.clone();
                    }
                }

                if (target_frame.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                // AI 객체 검출
                bool detected = detector.detectMultiplePersons(target_frame, bottom_centers, detected_boxes, confidences);
                std::vector<cv::Point2f> rel_meters;

                if (detected) {
                    for (const auto& center : bottom_centers) {
                        // 카메라 화소(발밑) -> RC카 기준 전방/측면 미터(m) 변환
                        cv::Point2f meter_pos = mapManager.transformToRelativeMeters(center);
                        rel_meters.push_back(meter_pos);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(g_ai_mtx);
                    g_person_detected = detected;
                    g_person_boxes = detected_boxes;
                    g_person_rel_meters = rel_meters; // 상대 미터 벡터로 갱신
                }
            } catch (const std::exception& e) {
                std::cerr << "[AI Loop Error] " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[AI Loop Error] Unknown exception occurred in loop." << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    } catch (const std::exception& e) {
        std::cerr << "[AI Thread Fatal Error] " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[AI Thread Fatal Error] Unknown exception occurred during initialization." << std::endl;
    }
}