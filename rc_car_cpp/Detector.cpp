#include "Detector.h"
#include <iostream>

Detector::Detector(const std::string& model_path, float threshold) {
    conf_threshold = threshold;
    try {
        net = cv::dnn::readNet(model_path);
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU); // RPi4 최적화
        std::cout << "✨ INT8 경량화 모델 로드 성공!" << std::endl;
    } catch (const cv::Exception& e) {
        std::cerr << "모델 로드 실패: " << e.what() << std::endl;
    }
}

bool Detector::detectPerson(const cv::Mat& frame, cv::Point2f& bottom_center) {
    if (frame.empty() || net.empty()) return false;

    // INT8 모델 입력 사이즈에 맞게 blob 생성 (예: 320x240)
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0/255.0, cv::Size(320, 240), cv::Scalar(0,0,0), true, false);
    net.setInput(blob);
    cv::Mat outputs = net.forward();

    // TODO: YOLO ONNX 출력 텐서 파싱 로직 구현
    // 사람이 감지되었다고 가정할 때의 바운딩 박스 좌표 추출 예시:
    // float xmin = ..., ymin = ..., xmax = ..., ymax = ...;
    // bottom_center = cv::Point2f((xmin + xmax) / 2.0f, ymax); // 발 위치 산출
    
    bool person_detected = false; // 테스트용 초기값 (모델 구조에 맞춰 파싱 구현 필요)
    return person_detected;
}