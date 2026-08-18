#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include "MapManager.h"

// 공유 전역 변수 선언 (다중 객체 vector 구조 반영)
extern std::mutex g_ai_mtx;
extern cv::Mat g_latest_frame;
extern bool g_person_detected;
extern std::vector<cv::Point2f> g_person_map_positions;
extern std::vector<cv::Rect> g_person_boxes;
extern std::atomic<bool> g_ai_running;

// 함수 선언
std::vector<cv::Point2f> getCalibrationPoints(cv::Mat& img, const std::string& win_name);
void aiAndTransformThread(const std::string& model_path, MapManager& mapManager);
