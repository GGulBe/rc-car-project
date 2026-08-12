#pragma once
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

class Detector {
private:
    cv::dnn::Net net;
    float conf_threshold;

public:
    Detector(const std::string& model_path, float threshold = 0.3f);
    
    // 카메라 프레임을 받아 사람이 감지되었는지 판단하는 함수
    bool detectPerson(const cv::Mat& frame);
};