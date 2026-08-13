#pragma once

#include <opencv2/opencv.hpp>

class MapManager {
private:
    double lat_min, lat_max, lon_min, lon_max;
    cv::Mat satellite_map;

public:
    MapManager(double lat_min, double lat_max, double lon_min, double lon_max, const std::string& map_path);
    
    // 위경도를 픽셀 좌표로 변환하는 함수
    cv::Point2f gpsToPixel(double lat, double lon);
    
    // 지도 위에 RC카와 사람 마커를 그려주는 함수
    cv::Mat drawMarkers(double rc_lat, double rc_lon, bool person_detected, float speed);
};