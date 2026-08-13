#include "MapManager.h"

MapManager::MapManager(double lat_min, double lat_max, double lon_min, double lon_max, const std::string& map_path) {
    this->lat_min = lat_min;
    this->lat_max = lat_max;
    this->lon_min = lon_min;
    this->lon_max = lon_max;
    this->satellite_map = cv::imread(map_path);
}

cv::Point2f MapManager::gpsToPixel(double lat, double lon) {
    double x_ratio = (lon - lon_min) / (lon_max - lon_min);
    double y_ratio = (lat_max - lat) / (lat_max - lat_min);
    
    float pixel_x = static_cast<float>(x_ratio * satellite_map.cols);
    float pixel_y = static_cast<float>(y_ratio * satellite_map.rows);
    
    return cv::Point2f(pixel_x, pixel_y);
}

cv::Mat MapManager::drawMarkers(double rc_lat, double rc_lon, bool person_detected, float speed) {
    cv::Mat display_map = satellite_map.clone();

    // RC카 현재 위치 (파란색 점)
    cv::Point2f rc_pixel = gpsToPixel(rc_lat, rc_lon);
    cv::circle(display_map, rc_pixel, 8, cv::Scalar(255, 0, 0), -1);
    cv::putText(display_map, "RC Car", rc_pixel + cv::Point2f(10, 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 2);

    // 사람이 감지되었을 때 지도에 빨간색 마커 표시
    if (person_detected) {
        cv::Point2f person_pixel = rc_pixel + cv::Point2f(20.0f, -20.0f); // 상대 위치 예시
        cv::circle(display_map, person_pixel, 10, cv::Scalar(0, 0, 255), -1);
        cv::putText(display_map, "Person", person_pixel + cv::Point2f(10, 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2);
    }

    // 속력 텍스트 출력 (test)
    std::string speed_str = "Speed: " + std::to_string(speed) + " m/s";
    cv::putText(display_map, speed_str, cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 255), 2);

    return display_map;
}