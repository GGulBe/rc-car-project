#include "Detector.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <onnxruntime_cxx_api.h>

struct DetectorImpl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "YoloDetector"};
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;

    std::vector<std::string> input_node_names;
    std::vector<std::string> output_node_names;
    std::vector<const char*> input_names_ptr;
    std::vector<const char*> output_names_ptr;

    int input_width = 448;   // 기본 모델 너비
    int input_height = 336;  // 기본 모델 높이

    DetectorImpl(const std::string& model_path) {
        session_options.SetIntraOpNumThreads(2);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        session = std::make_unique<Ort::Session>(env, model_path.c_str(), session_options);

        Ort::AllocatorWithDefaultOptions allocator;

        // Input shape 자동 추출
        const size_t num_inputs = session->GetInputCount();
        if (num_inputs > 0) {
            auto name = session->GetInputNameAllocated(0, allocator);
            input_node_names.push_back(name.get());
            input_names_ptr.push_back(input_node_names.back().c_str());

            auto type_info = session->GetInputTypeInfo(0);
            auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
            auto input_shape = tensor_info.GetShape();

            if (input_shape.size() == 4) {
                if (input_shape[3] > 0) input_width = static_cast<int>(input_shape[3]);
                if (input_shape[2] > 0) input_height = static_cast<int>(input_shape[2]);
            }
        }

        size_t num_outputs = session->GetOutputCount();
        for (size_t i = 0; i < num_outputs; ++i) {
            auto name = session->GetOutputNameAllocated(i, allocator);
            output_node_names.push_back(name.get());
        }
        for (auto& name : output_node_names) {
            output_names_ptr.push_back(name.c_str());
        }

        std::cout << "[AI] ONNX 모델 로드 성공: " << model_path 
                  << " | 요구 입력 크기: " << input_width << "x" << input_height << std::endl;
        for (size_t i = 0; i < output_node_names.size(); ++i) {
            std::cout << "Output[" << i << "]: " << output_node_names[i] << std::endl;
        }
    }
};

Detector::Detector(const std::string& model_path, float threshold, float nms_thresh)
    : conf_threshold(threshold), nms_threshold(nms_thresh), pImpl(nullptr) {
    try {
        pImpl = new DetectorImpl(model_path);
    }
    catch (const std::exception& e) {
        std::cerr << "모델 로드 실패: " << e.what() << std::endl;
    }
}

Detector::~Detector() {
    delete pImpl;
    pImpl = nullptr;
}

bool Detector::detectMultiplePersons(
    const cv::Mat& frame,
    std::vector<cv::Point2f>& bottom_centers,
    std::vector<cv::Rect>& out_boxes,
    std::vector<float>& confidences
) {
    bottom_centers.clear();
    out_boxes.clear();
    confidences.clear();

    if (frame.empty() || !pImpl || !pImpl->session) {
        return false;
    }

    // 1. 카메라 해상도와 관계없이 모델이 요구하는 크기로 자동 리사이즈
    cv::Mat resized_frame;
    if (frame.cols != pImpl->input_width || frame.rows != pImpl->input_height) {
        cv::resize(frame, resized_frame, cv::Size(pImpl->input_width, pImpl->input_height));
    } else {
        resized_frame = frame;
    }

    cv::Mat blob = cv::dnn::blobFromImage(
        resized_frame,
        1.0 / 255.0,
        cv::Size(pImpl->input_width, pImpl->input_height),
        cv::Scalar(0, 0, 0),
        true,
        false
    );

    std::vector<int64_t> input_shape = {1, 3, pImpl->input_height, pImpl->input_width};
    size_t input_tensor_size = 1 * 3 * pImpl->input_height * pImpl->input_width;

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info,
        blob.ptr<float>(),
        input_tensor_size,
        input_shape.data(),
        input_shape.size()
    );

    try {
        auto output_tensors = pImpl->session->Run(
            Ort::RunOptions{nullptr},
            pImpl->input_names_ptr.data(),
            &input_tensor,
            1,
            pImpl->output_names_ptr.data(),
            pImpl->output_names_ptr.size()
        );

        if (output_tensors.size() < 2) {
            std::cerr << "ONNX 출력 개수가 2개 미만입니다." << std::endl;
            return false;
        }

        float* boxes = output_tensors[0].GetTensorMutableData<float>();
        float* scores = output_tensors[1].GetTensorMutableData<float>();

        auto boxes_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
        auto scores_shape = output_tensors[1].GetTensorTypeAndShapeInfo().GetShape();

        if (boxes_shape.size() != 3 || scores_shape.size() != 2) {
            std::cerr << "잘못된 ONNX 출력 형태입니다." << std::endl;
            return false;
        }

        const int num_boxes = static_cast<int>(boxes_shape[1]);
        std::vector<cv::Rect> candidate_boxes;
        std::vector<float> candidate_scores;

        // 모델 공간 좌표를 원래 카메라 프레임 크기로 스케일링하기 위한 비율 계산
        const float scale_x = static_cast<float>(frame.cols) / static_cast<float>(pImpl->input_width);
        const float scale_y = static_cast<float>(frame.rows) / static_cast<float>(pImpl->input_height);

        for (int i = 0; i < num_boxes; ++i) {
            float score = scores[i];
            if (score < conf_threshold) {
                continue;
            }

            // 모델 출력 좌표를 원본 카메라 해상도 기준으로 복원
            float x1 = boxes[i * 4 + 0] * scale_x;
            float y1 = boxes[i * 4 + 1] * scale_y;
            float x2 = boxes[i * 4 + 2] * scale_x;
            float y2 = boxes[i * 4 + 3] * scale_y;

            int left   = static_cast<int>(std::round(x1));
            int top    = static_cast<int>(std::round(y1));
            int right  = static_cast<int>(std::round(x2));
            int bottom = static_cast<int>(std::round(y2));

            left   = std::max(0, std::min(left, frame.cols - 1));
            top    = std::max(0, std::min(top, frame.rows - 1));
            right  = std::max(0, std::min(right, frame.cols - 1));
            bottom = std::max(0, std::min(bottom, frame.rows - 1));

            int width = right - left;
            int height = bottom - top;

            if (width <= 1 || height <= 1) {
                continue;
            }

            candidate_boxes.emplace_back(left, top, width, height);
            candidate_scores.push_back(score);
        }

        if (candidate_boxes.empty()) {
            return false;
        }

        // NMS로 겹치는 중복 박스 제거
        std::vector<int> indices;
        cv::dnn::NMSBoxes(candidate_boxes, candidate_scores, conf_threshold, nms_threshold, indices);

        for (int idx : indices) {
            cv::Rect box = candidate_boxes[idx];
            float conf = candidate_scores[idx];

            float center_x = box.x + box.width * 0.5f;
            float bottom_y = static_cast<float>(box.y + box.height);

            out_boxes.push_back(box);
            bottom_centers.emplace_back(center_x, bottom_y);
            confidences.push_back(conf);

            /*std::cout << "[DEBUG] Person Detected! | Conf: " << conf << " (" << (conf * 100.0f) << "%)"
                      << " | Box: (" << box.x << ", " << box.y << ", " << box.width << ", " << box.height << ")"
                      << std::endl;
            */
        }

        return !out_boxes.empty();
    }
    catch (const std::exception& e) {
        std::cerr << "추론 실행 중 에러: " << e.what() << std::endl;
    }

    return false;
}

bool Detector::detectPerson(
    const cv::Mat& frame,
    cv::Point2f& bottom_center,
    cv::Rect& out_box
) {
    std::vector<cv::Point2f> centers;
    std::vector<cv::Rect> boxes;
    std::vector<float> confs;

    if (detectMultiplePersons(frame, centers, boxes, confs) && !boxes.empty()) {
        bottom_center = centers[0];
        out_box = boxes[0];
        return true;
    }

    out_box = cv::Rect();
    return false;
}
