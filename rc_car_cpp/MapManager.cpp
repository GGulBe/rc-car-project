#include "MapManager.h"

MapManager::MapManager(const std::string& map_path) {
    this->satellite_map = cv::imread(map_path);
}

void MapManager::setHomography(const std::vector<cv::Point2f>& video_pts, const std::vector<cv::Point2f>& map_pts) {
    // 프로그램 시작 시 1회 캘리브레이션 행렬 계산
    this->homography_matrix = cv::findHomography(video_pts, map_pts);
}

cv::Point2f MapManager::transformToMap(const cv::Point2f& camera_bottom_center) {
    if (homography_matrix.empty()) return cv::Point2f(-1, -1);

    std::vector<cv::Point2f> src_pts = { camera_bottom_center };
    std::vector<cv::Point2f> dst_pts;
    
    // 호모그래피를 통한 투영 변환
    cv::perspectiveTransform(src_pts, dst_pts, homography_matrix);
    return dst_pts[0];
}

cv::Mat MapManager::drawMarkers(double rc_lat, double rc_lon, bool person_detected, const cv::Point2f& person_map_pos) {
    cv::Mat display_map = satellite_map.clone();

    // 1. RC카 현재 위치 출력 (예시 고정 또는 GPS 연동 위치)
    // cv::Point2f rc_pixel(300, 400); // 픽셀 변환 로직 연동 가능
    // cv::circle(display_map, rc_pixel, 8, cv::Scalar(255, 0, 0), -1);
    // cv::putText(display_map, "RC Car", rc_pixel + cv::Point2f(10, 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 2);

    // 2. 사람이 탐지되었을 때만 지도에 마커 표시, 사라지면 미출력
    if (person_detected && person_map_pos.x >= 0 && person_map_pos.y >= 0) {
        cv::circle(display_map, person_map_pos, 10, cv::Scalar(0, 0, 255), -1);
        cv::putText(display_map, "Person", person_map_pos + cv::Point2f(10, 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2);
    }

    return display_map;
}