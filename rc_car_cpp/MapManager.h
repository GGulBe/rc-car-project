#ifndef MAP_MANAGER_H
#define MAP_MANAGER_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

class MapManager {
private:
    cv::Mat map_image;
    cv::Mat current_display_map;
    cv::Mat homography_matrix;
    bool has_homography;

public:
    MapManager();
    ~MapManager();

    bool loadMap(const std::string& map_path);
    bool setHomography(const std::vector<cv::Point2f>& video_points, const std::vector<cv::Point2f>& map_points);
    cv::Point2f transformToMap(const cv::Point2f& video_point);

    // 단일 대상 마커 그리기 (반환 타입 cv::Mat)
    cv::Mat drawMarkers(double lat, double lon, bool person_detected, const cv::Point2f& person_map_pos);

    // 다중 대상 마커 그리기 (반환 타입 cv::Mat)
    cv::Mat drawMarkers(double lat, double lon, bool person_detected, const std::vector<cv::Point2f>& person_map_positions);

    cv::Mat getDisplayMap() const;
};

#endif
