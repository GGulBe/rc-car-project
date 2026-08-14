#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include "MapManager.h"

extern std::mutex g_ai_mtx;
extern cv::Mat g_latest_frame;
extern bool g_person_detected;
extern cv::Point2f g_person_map_pos;
extern std::atomic<bool> g_ai_running;

struct ClickContext {
    std::vector<cv::Point2f> points;
    std::string window_name;
    cv::Mat image;
};

void mouseCallback(int event, int x, int y, int flags, void* userdata);
std::vector<cv::Point2f> getCalibrationPoints(cv::Mat& img, const std::string& win_name);
void aiAndTransformThread(const std::string& model_path, MapManager& mapManager);