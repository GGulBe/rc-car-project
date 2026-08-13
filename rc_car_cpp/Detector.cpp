#include "Detector.h"
#include <iostream>

Detector::Detector(const std::string& model_path, float threshold) {
    conf_threshold = threshold;
    try {
        net = cv::dnn::readNet(model_path);
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU); // 라즈베리파이 4 CPU 최적화
        std::cout << "✨ INT8 경량화 모델 로드 성공!" << std::endl;
    } catch (const cv::Exception& e) {
        std::cerr << "모델 로드 실패: " << e.what() << std::endl;
    }
}

bool Detector::detectPerson(const cv::Mat& frame, cv::Point2f& bottom_center) {
    if (frame.empty() || net.empty()) return false;

    // 1. 카메라 해상도(320x240)와 동일한 크기로 모델 입력 blob 생성
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0/255.0, cv::Size(320, 240), cv::Scalar(0,0,0), true, false);
    net.setInput(blob);
    cv::Mat outputs = net.forward();

    // 2. ONNX 출력 파싱 (YOLO 포맷 구조 대응)
    cv::Mat det_output = outputs.reshape(1, outputs.size[1]); 
    cv::transpose(det_output, det_output); 

    float max_conf = conf_threshold;
    bool person_detected = false;
    cv::Rect best_box;

    int rows = det_output.rows;
    for (int i = 0; i < rows; ++i) {
        float confidence = det_output.at<float>(i, 4); // 신뢰도(Confidence)
        
        if (confidence > max_conf) {
            max_conf = confidence;
            
            // 카메라 해상도(320x240)와 모델 입력 크기가 같으므로 1:1 비율 적용
            float cx = det_output.at<float>(i, 0);
            float cy = det_output.at<float>(i, 1);
            float w  = det_output.at<float>(i, 2);
            float h  = det_output.at<float>(i, 3);

            int xmin = static_cast<int>(cx - w / 2.0f);
            int ymin = static_cast<int>(cy - h / 2.0f);
            int xmax = static_cast<int>(cx + w / 2.0f);
            int ymax = static_cast<int>(cy + h / 2.0f);

            best_box = cv::Rect(xmin, ymin, xmax - xmin, ymax - ymin);
            person_detected = true;
        }
    }
    
    if (person_detected) {
        // 바운딩 박스 하단 중앙(Bottom-center)을 발 위치로 지정[cite: 1]
        float u = static_cast<float>(best_box.x + best_box.width / 2);
        float v = static_cast<float>(best_box.y + best_box.height);
        bottom_center = cv::Point2f(u, v);
        return true;
    }

    return false;
}