#pragma once
#include <opencv2/opencv.hpp>
#include <string>

struct DetectorImpl;

class Detector {
private:
    float conf_threshold;
    DetectorImpl* pImpl;

public:
    Detector(const std::string& model_path, float threshold);
    ~Detector();
    bool detectPerson(const cv::Mat& frame, cv::Point2f& bottom_center);
};
