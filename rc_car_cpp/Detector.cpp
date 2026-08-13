#include "Detector.h"
#include <iostream>

Detector::Detector(const std::string& model_path, float threshold) {
    conf_threshold = threshold;
    try {
        net = cv::dnn::readNet(model_path);
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU); // 라즈베리파이 4 CPU 타겟 최적화
        std::cout << "✨ INT8 경량화 모델 로드 성공!" << std::endl;
    } catch (const cv::Exception& e) {
        std::cerr << "모델 로드 실패: " << e.what() << std::endl;
    }
}

bool Detector::detectPerson(const cv::Mat& frame, cv::Point2f& bottom_center) {
    if (frame.empty() || net.empty()) return false;

    // 1. 모델 입력 크기에 맞춰 blob 생성 (예: 320x240)
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0/255.0, cv::Size(320, 240), cv::Scalar(0,0,0), true, false);
    net.setInput(blob);
    cv::Mat outputs = net.forward();

    // 2. YOLO ONNX 출력 파싱 로직 (모델 구조에 맞게 구현)
    // -------------------------------------------------------------------------
    // [참고] 일반적으로 ONNX 내보내기 형식에 따라 outputs의 섀이프(Shape)가 다릅니다.
    // 보통 [1, 5, 8400] 또는 [1, num_boxes, 6] 형식을 가집니다.
    // 여기서는 가장 신뢰도가 높은 사람(Person) 박스를 찾아내는 표준 파싱 틀을 제공합니다.
    // -------------------------------------------------------------------------
    float* data = (float*)outputs.datastart;
    int dimensions = outputs.size[1]; // 예: 5 또는 85
    int rows = outputs.size[2];       // 예: 8400 (박스 개수)

    float max_conf = conf_threshold;
    bool person_detected = false;
    cv::Rect best_box;

    // 만약 outputs 형태가 전형적인 (1, 8400, attributes) 형태인 경우 대응
    // (모델 스펙에 따라 다를 수 있으므로 팀에서 사용하는 ONNX 스펙 확인 필요)
    for (int i = 0; i < rows; ++i) {
        // 데이터 구조 예시: [x_center, y_center, width, height, confidence, class_scores...]
        float confidence = data[4]; // 4번 인덱스가 객체 신뢰도인 경우
        if (confidence > max_conf) {
            max_conf = confidence;
            
            // 픽셀 좌표계로 역환산 (모델 입력 320x240 기준에서 원본 프레임 크기로 스케일링)
            float cx = data[0] * (float)frame.cols / 320.0f;
            float cy = data[1] * (float)frame.rows / 240.0f;
            float w  = data[2] * (float)frame.cols / 320.0f;
            float h  = data[3] * (float)frame.rows / 240.0f;

            int xmin = static_cast<int>(cx - w / 2.0f);
            int ymin = static_cast<int>(cy - h / 2.0f);
            int xmax = static_cast<int>(cx + w / 2.0f);
            int ymax = static_cast<int>(cy + h / 2.0f);

            best_box = cv::Rect(xmin, ymin, xmax - xmin, ymax - ymin);
            person_detected = true;
        }
        data += dimensions;
    }

    if (person_detected) {
        // 3. 발이 지면에 닿는 지점 (bottom-center) 계산[cite: 1]
        float u = (best_box.x + best_box.x + best_box.width) / 2.0f;
        float v = static_cast<float>(best_box.y + best_box.height);
        bottom_center = cv::Point2f(u, v);
        return true;
    }

    return false;
}