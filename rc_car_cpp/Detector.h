#ifndef DETECTOR_H
#define DETECTOR_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class Detector {
private:
    float conf_threshold;
    float nms_threshold;
    struct DetectorImpl* pImpl;

public:
    Detector(const std::string& model_path, float threshold = 0.3f, float nms_thresh = 0.45f);
    ~Detector();

    // 기존 단일 객체 검출 (가장 높은 Conf 1명)
    bool detectPerson(
        const cv::Mat& frame,
        cv::Point2f& bottom_center,
        cv::Rect& out_box
    );

    // 다중 객체 검출 (NMS 적용, 감지된 모든 사람 반환)
    bool detectMultiplePersons(
        const cv::Mat& frame,
        std::vector<cv::Point2f>& bottom_centers,
        std::vector<cv::Rect>& out_boxes,
        std::vector<float>& confidences
    );
};

#endif
