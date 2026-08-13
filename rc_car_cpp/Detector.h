#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <string>

class Detector {
private:
    cv::dnn::Net net;
    float conf_threshold;

public:
    Detector(const std::string& model_path, float threshold = 0.3f);
    
    // 딥러닝 추론 수행 및 사람의 발 위치(bottom-center) 반환 함수
    bool detectPerson(const cv::Mat& frame, cv::Point2f& bottom_center);
};