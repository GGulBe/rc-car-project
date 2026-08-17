#ifndef DETECTOR_H
#define DETECTOR_H

#include <opencv2/opencv.hpp>
#include <string>

class Detector {
private:
    float conf_threshold;
    struct DetectorImpl* pImpl;

public:
    Detector(const std::string& model_path, float threshold = 0.5f);
    ~Detector();

    bool detectPerson(
        const cv::Mat& frame,
        cv::Point2f& bottom_center,
        cv::Rect& out_box
    );
};

#endif
