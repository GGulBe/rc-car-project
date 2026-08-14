#include "Detector.h"
#include <iostream>
#include <vector>
#include <numeric>
#include <onnxruntime_cxx_api.h>

struct DetectorImpl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "YoloDetector"};
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;
    
    // 모델 입출력 이름 및 형태 캐싱용
    std::vector<std::string> input_node_names;
    std::vector<std::string> output_node_names;
    std::vector<const char*> input_names_ptr;
    std::vector<const char*> output_names_ptr;

    DetectorImpl(const std::string& model_path) {
        session_options.SetIntraOpNumThreads(2);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        // 💡 IR 버전 이슈가 해결된 results4_fixed.onnx 모델 로드
        session = std::make_unique<Ort::Session>(env, model_path.c_str(), session_options);

        Ort::AllocatorWithDefaultOptions allocator;

        // 입력 노드 정보 가져오기
        size_t num_inputs = session->GetInputCount();
        for (size_t i = 0; i < num_inputs; ++i) {
            auto input_name = session->GetInputNameAllocated(i, allocator);
            input_node_names.push_back(input_name.get());
        }
        for (auto& name : input_node_names) {
            input_names_ptr.push_back(name.c_str());
        }

        // 출력 노드 정보 가져오기
        size_t num_outputs = session->GetOutputCount();
        for (size_t i = 0; i < num_outputs; ++i) {
            auto output_name = session->GetOutputNameAllocated(i, allocator);
            output_node_names.push_back(output_name.get());
        }
        for (auto& name : output_node_names) {
            output_names_ptr.push_back(name.c_str());
        }

        std::cout << "✨ ONNX Runtime을 통한 INT8 QDQ 모델 로드 성공!" << std::endl;
    }
};

Detector::Detector(const std::string& model_path, float threshold) 
    : conf_threshold(threshold), pImpl(nullptr) {
    try {
        pImpl = new DetectorImpl(model_path);
    } catch (const std::exception& e) {
        std::cerr << "모델 로드 실패 (ONNX Runtime): " << e.what() << std::endl;
    }
}

Detector::~Detector() {
    if (pImpl) {
        delete pImpl;
        pImpl = nullptr;
    }
}

// 사람 감지하면 bottom_center좌표 반환하는 함수
bool Detector::detectPerson(const cv::Mat& frame, cv::Point2f& bottom_center) {
    if (frame.empty() || !pImpl || !pImpl->session) return false;

    cv::Mat rgb, float_img;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(float_img, CV_32FC3, 1.0 / 255.0);

    cv::Mat blob = cv::dnn::blobFromImage(float_img, 1.0, cv::Size(320, 240), cv::Scalar(0,0,0), true, false);

    size_t input_tensor_size = 1 * 3 * 240 * 320;
    std::vector<int64_t> input_shape = {1, 3, 240, 320};

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, 
        blob.ptr<float>(), 
        input_tensor_size, 
        input_shape.data(), 
        input_shape.size()
    );

    // 4. 모델 추론 실행 
    try {
        auto output_tensors = pImpl->session->Run(
            Ort::RunOptions{nullptr},
            pImpl->input_names_ptr.data(),
            &input_tensor,
            1,
            pImpl->output_names_ptr.data(),
            pImpl->output_names_ptr.size()
        );

        if (output_tensors.empty()) return false;

        // 출력 결과 파싱
        float* raw_output = output_tensors[0].GetTensorMutableData<float>();
        auto type_info = output_tensors[0].GetTensorTypeAndShapeInfo();
        auto shape = type_info.GetShape();

        int rows = 0;
        if (shape.size() == 3) {
            rows = shape[2];
        }

        float max_conf = conf_threshold;
        bool person_detected = false;
        cv::Rect best_box;

        // 신뢰도(conf)보다 가장 높은 객체 탐색
        for (int i = 0; i < rows; ++i) {
            float confidence = 0.0f;
            float cx = 0, cy = 0, w = 0, h = 0;

            if (shape.size() == 3 && shape[1] > 5) {
                cx = raw_output[0 * rows + i];
                cy = raw_output[1 * rows + i];
                w  = raw_output[2 * rows + i];
                h  = raw_output[3 * rows + i];
                confidence = raw_output[4 * rows + i];
            }

            // conf = 0.2
            if (confidence > max_conf) {
                max_conf = confidence;
                int xmin = static_cast<int>(cx - w / 2.0f);
                int ymin = static_cast<int>(cy - h / 2.0f);
                int xmax = static_cast<int>(cx + w / 2.0f);
                int ymax = static_cast<int>(cy + h / 2.0f);

                best_box = cv::Rect(xmin, ymin, xmax - xmin, ymax - ymin);
                person_detected = true;
            }
        }

        // true 받으면 bottom_center좌표 계산
        if (person_detected) {
            float u = static_cast<float>(best_box.x + best_box.width / 2);
            float v = static_cast<float>(best_box.y + best_box.height);
            bottom_center = cv::Point2f(u, v);
            return true;
        }

    } catch (const std::exception& e) {
        std::cerr << "추론 실행 중 에러 발생: " << e.what() << std::endl;
    }

    return false;
}
