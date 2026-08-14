#include "MapManager.h"

MapManager::MapManager(const std::string& map_path) {
    this->satellite_map = cv::imread(map_path);
    
    // 위성사진 규격(552x662)에 대응하는 위경도 범위 설정
    this->lat_max = 37.587197;
    this->lat_min = 37.583025;
    this->lon_min = 127.097463;
    this->lon_max = 127.098330;
}
// 호모그래피 행렬 설정 함수 
void MapManager::setHomography(const std::vector<cv::Point2f>& video_pts, const std::vector<cv::Point2f>& map_pts) {
    this->homography_matrix = cv::findHomography(video_pts, map_pts);
}

// GPS 위경도 좌표를 이미지 픽셀 좌표로 변환하는 함수
cv::Point2f MapManager::gpsToPixel(double lat, double lon) {
    double x_ratio = (lon - lon_min) / (lon_max - lon_min);
    double y_ratio = (lat_max - lat) / (lat_max - lat_min);
    
    float pixel_x = static_cast<float>(x_ratio * 552.0);
    float pixel_y = static_cast<float>(y_ratio * 662.0);
    
    return cv::Point2f(pixel_x, pixel_y);
}

// 객체 탐지하여 찍힌 바운드박스의 bottom_center값을 위성 지도 좌표로 변환하는 함수
cv::Point2f MapManager::transformToMap(const cv::Point2f& camera_bottom_center) {
    if (homography_matrix.empty()) return cv::Point2f(-1, -1);

    std::vector<cv::Point2f> src_pts = { camera_bottom_center };
    std::vector<cv::Point2f> dst_pts;
    
    cv::perspectiveTransform(src_pts, dst_pts, homography_matrix);
    return dst_pts[0];
}

// RC카 위치와 감지된 사람의 위치 마커를 그려주는 함수
cv::Mat MapManager::drawMarkers(double rc_lat, double rc_lon, bool person_detected, const cv::Point2f& person_map_pos) {
    cv::Mat display_map = satellite_map.clone();

    // RC카 현재 GPS 위치 마킹
    cv::Point2f rc_pixel = gpsToPixel(rc_lat, rc_lon);
    cv::circle(display_map, rc_pixel, 6, cv::Scalar(255, 0, 0), -1);
    cv::putText(display_map, "RC Car", rc_pixel + cv::Point2f(8, 4), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 0, 0), 1);
    //사람 감지 시 위성지도에 마킹
    if (person_detected && person_map_pos.x > 0 && person_map_pos.y > 0) {
        cv::circle(display_map, person_map_pos, 8, cv::Scalar(0, 0, 255), -1);
        cv::putText(display_map, "Person", person_map_pos + cv::Point2f(8, 4), cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 255), 1);
    }

    return display_map;
}