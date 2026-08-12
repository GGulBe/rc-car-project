#include "Detector.h"
#include <iostream>

Detector::Detector(const std::string& model_path, float threshold) {
    conf_threshold = threshold;
    try {
        net = cv::dnn::readNet(model_path);
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        std::cout << "✨ INT8 딥러닝 모델 로드 성공!" << std::endl;
    } catch (const cv::Exception& e) {
        std::cerr << "모델 로드 실패: " << e.what() << std::endl;
    }
}

bool Detector::detectPerson(const cv::Mat& frame) {
    if (frame.empty() || net.empty()) return false;

    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0/255.0, cv::Size(320, 240), cv::Scalar(0,0,0), true, false);
    net.setInput(blob);
    cv::Mat outputs = net.forward();

    // TODO: 모델 출력 결과에 맞춰 사람 검출 여부(true/false) 판별 로직 구현
    // 현재는 테스트용으로 false 리턴 (추후 모델 구조에 맞게 수정)
    bool person_detected = false; 

    return person_detected;
}