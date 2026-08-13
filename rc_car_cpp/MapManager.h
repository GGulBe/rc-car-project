#pragma once
#include <opencv2/opencv.hpp>
#include <string>

class MapManager {
private:
    cv::Mat satellite_map;
    cv::Mat homography_matrix; // 호모그래피 행렬 H

public:
    MapManager(const std::string& map_path);
    
    // 1회 캘리브레이션용 호모그래피 행렬 설정
    void setHomography(const std::vector<cv::Point2f>& video_pts, const std::vector<cv::Point2f>& map_pts);
    
    // 카메라 픽셀(발 위치)을 위성지도 픽셀로 변환
    cv::Point2f transformToMap(const cv::Point2f& camera_bottom_center);
    
    // 지도 시각화 렌더링 함수
    cv::Mat drawMarkers(double rc_lat, double rc_lon, bool person_detected, const cv::Point2f& person_map_pos);
};