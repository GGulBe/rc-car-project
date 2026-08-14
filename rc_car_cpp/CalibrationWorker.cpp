// 캘리브레이션 및 멀티스레드
#include "CalibrationWorker.h"
#include "Detector.h"
#include <iostream>
#include <thread>
#include <chrono>

std::mutex g_ai_mtx;
cv::Mat g_latest_frame;
bool g_person_detected = false;
cv::Point2f g_person_map_pos(-1, -1);
std::atomic<bool> g_ai_running{true};

void mouseCallback(int event, int x, int y, int flags, void* userdata) {
    flags = NULL;
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
// 마우스 클릭하여 캘리브레이션 진행
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
        if (context.points.size() >= 4) { // 현재 Default값이 4인데 추후 수정
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

// 백그라운드에서 AI 추론 및 좌표 변환 스레드 함수
void aiAndTransformThread(const std::string& model_path, MapManager& mapManager) {
    Detector detector(model_path, 0.3f);
    cv::Mat target_frame;

    while (g_ai_running.load()) {
        {
            std::lock_guard<std::mutex> lock(g_ai_mtx);
            if (g_latest_frame.empty()) continue;
            target_frame = g_latest_frame.clone();
        }

        cv::Point2f bottom_center(0, 0);
        // 사람 감지 및 bottom_center좌표 받는 함수
        bool detected = detector.detectPerson(target_frame, bottom_center);

        cv::Point2f transformed_pos(-1, -1);
        // 사람이 감지 되면 호모그래피로 계산하여 좌표변환
        if (detected) {
            transformed_pos = mapManager.transformToMap(bottom_center);
        }

        {
            // 감지 결과 및 지도 좌표 반환
            std::lock_guard<std::mutex> lock(g_ai_mtx);
            g_person_detected = detected;
            g_person_map_pos = transformed_pos;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}