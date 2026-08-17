#include "Detector.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <onnxruntime_cxx_api.h>

struct DetectorImpl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "YoloDetector"};
    Ort::SessionOptions session_options;
    std::unique_ptr<Ort::Session> session;

    std::vector<std::string> input_node_names;
    std::vector<std::string> output_node_names;
    std::vector<const char*> input_names_ptr;
    std::vector<const char*> output_names_ptr;

    DetectorImpl(const std::string& model_path) {
        session_options.SetIntraOpNumThreads(2);
        session_options.SetGraphOptimizationLevel(
            GraphOptimizationLevel::ORT_ENABLE_ALL
        );

        session = std::make_unique<Ort::Session>(
            env,
            model_path.c_str(),
            session_options
        );

        Ort::AllocatorWithDefaultOptions allocator;

        size_t num_inputs = session->GetInputCount();

        for (size_t i = 0; i < num_inputs; ++i) {
            auto name = session->GetInputNameAllocated(i, allocator);
            input_node_names.push_back(name.get());
        }

        for (auto& name : input_node_names) {
            input_names_ptr.push_back(name.c_str());
        }

        size_t num_outputs = session->GetOutputCount();

        for (size_t i = 0; i < num_outputs; ++i) {
            auto name = session->GetOutputNameAllocated(i, allocator);
            output_node_names.push_back(name.get());
        }

        for (auto& name : output_node_names) {
            output_names_ptr.push_back(name.c_str());
        }

        std::cout << "ONNX 모델 로드 성공" << std::endl;

        for (size_t i = 0; i < output_node_names.size(); ++i) {
            std::cout << "Output[" << i << "]: "
                      << output_node_names[i] << std::endl;
        }
    }
};

Detector::Detector(
    const std::string& model_path,
    float threshold
)
    : conf_threshold(threshold),
      pImpl(nullptr) {

    try {
        pImpl = new DetectorImpl(model_path);
    }
    catch (const std::exception& e) {
        std::cerr << "모델 로드 실패: "
                  << e.what()
                  << std::endl;
    }
}

Detector::~Detector() {
    delete pImpl;
    pImpl = nullptr;
}

bool Detector::detectPerson(
    const cv::Mat& frame,
    cv::Point2f& bottom_center,
    cv::Rect& out_box
) {
    if (frame.empty() ||
        !pImpl ||
        !pImpl->session) {
        return false;
    }

    cv::Mat rgb;
    cv::Mat float_img;

    cv::cvtColor(
        frame,
        rgb,
        cv::COLOR_BGR2RGB
    );

    rgb.convertTo(
        float_img,
        CV_32FC3,
        1.0 / 255.0
    );

    cv::Mat blob = cv::dnn::blobFromImage(
        float_img,
        1.0,
        cv::Size(320, 240),
        cv::Scalar(0, 0, 0),
        true,
        false
    );

    std::vector<int64_t> input_shape = {
        1, 3, 240, 320
    };

    size_t input_tensor_size =
        1 * 3 * 240 * 320;

    auto memory_info =
        Ort::MemoryInfo::CreateCpu(
            OrtDeviceAllocator,
            OrtMemTypeCPU
        );

    Ort::Value input_tensor =
        Ort::Value::CreateTensor<float>(
            memory_info,
            blob.ptr<float>(),
            input_tensor_size,
            input_shape.data(),
            input_shape.size()
        );

    try {
        auto output_tensors =
            pImpl->session->Run(
                Ort::RunOptions{nullptr},
                pImpl->input_names_ptr.data(),
                &input_tensor,
                1,
                pImpl->output_names_ptr.data(),
                pImpl->output_names_ptr.size()
            );

        if (output_tensors.size() < 2) {
            std::cerr
                << "ONNX 출력 개수가 2개 미만입니다."
                << std::endl;

            return false;
        }

        float* boxes =
            output_tensors[0].GetTensorMutableData<float>();

        float* scores =
            output_tensors[1].GetTensorMutableData<float>();

        auto boxes_shape =
            output_tensors[0]
                .GetTensorTypeAndShapeInfo()
                .GetShape();

        auto scores_shape =
            output_tensors[1]
                .GetTensorTypeAndShapeInfo()
                .GetShape();

        if (boxes_shape.size() != 3 ||
            scores_shape.size() != 2) {

            std::cerr
                << "잘못된 ONNX 출력 형태입니다."
                << std::endl;

            return false;
        }

        const int num_boxes =
            static_cast<int>(boxes_shape[1]);

        float best_score = conf_threshold;
        cv::Rect best_box;
        bool detected = false;

        for (int i = 0; i < num_boxes; ++i) {

            float score = scores[i];

            if (score < best_score) {
                continue;
            }

            float x1 = boxes[i * 4 + 0];
            float y1 = boxes[i * 4 + 1];
            float x2 = boxes[i * 4 + 2];
            float y2 = boxes[i * 4 + 3];

            int left =
                static_cast<int>(std::round(x1));

            int top =
                static_cast<int>(std::round(y1));

            int right =
                static_cast<int>(std::round(x2));

            int bottom =
                static_cast<int>(std::round(y2));

            left = std::max(0, std::min(left, frame.cols - 1));
            top = std::max(0, std::min(top, frame.rows - 1));
            right = std::max(0, std::min(right, frame.cols - 1));
            bottom = std::max(0, std::min(bottom, frame.rows - 1));

            int width = right - left;
            int height = bottom - top;

            if (width <= 1 || height <= 1) {
                continue;
            }

            best_score = score;

            best_box =
                cv::Rect(
                    left,
                    top,
                    width,
                    height
                );

            detected = true;
        }

        if (!detected) {
            out_box = cv::Rect();
            return false;
        }

        float center_x =
            best_box.x +
            best_box.width * 0.5f;

        float bottom_y =
            best_box.y +
            best_box.height;

        bottom_center =
            cv::Point2f(
                center_x,
                bottom_y
            );

        out_box = best_box;

        std::cout
            << "[DEBUG] Person Detected!\n"
            << "Confidence : "
            << best_score
            << "\n"
            << "Center     : ("
            << center_x
            << ", "
            << bottom_y
            << ")\n"
            << "Box        : ("
            << best_box.x
            << ", "
            << best_box.y
            << ", "
            << best_box.width
            << ", "
            << best_box.height
            << ")\n"
            << "========================================"
            << std::endl;

        return true;
    }
    catch (const std::exception& e) {

        std::cerr
            << "추론 실행 중 에러: "
            << e.what()
            << std::endl;
    }

    return false;
}
