#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class MapManager {
private:
    cv::Mat satellite_map;       // .cpp 파일과 일치
    cv::Mat homography_matrix;   // .cpp 파일과 일치
    
    double lat_max;
    double lat_min;
    double lon_min;
    double lon_max;

public:
    explicit MapManager(const std::string& map_path);
    
    void setHomography(const std::vector<cv::Point2f>& video_pts, const std::vector<cv::Point2f>& map_pts);
    cv::Point2f transformToMap(const cv::Point2f& video_point);
    cv::Point2f gpsToPixel(double lat, double lon);
    
    cv::Mat drawMarkers(double rc_lat, double rc_lon, bool person_detected, const cv::Point2f& person_map_pos);
};
