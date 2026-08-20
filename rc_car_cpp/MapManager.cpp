#include "MapManager.h"
#include <iostream>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MapManager::MapManager() 
    : is_calibrated_(false), map_size_(0, 0) {}

MapManager::~MapManager() {}

bool MapManager::loadMap(const std::string& map_path) {
    map_image_ = cv::imread(map_path);
    if (map_image_.empty()) {
        std::cerr << "[MapManager Error] 지도 이미지 로드 실패: " << map_path << std::endl;
        return false;
    }
    map_size_ = map_image_.size();
    current_display_map_ = map_image_.clone();
    return true;
}

void MapManager::setMapGeoBounds(GeoPoint top_left, GeoPoint bottom_right, cv::Size map_size) {
    geo_top_left_ = top_left;
    geo_bottom_right_ = bottom_right;
    map_size_ = map_size;
}

bool MapManager::calibrateCameraToMeters(const std::vector<cv::Point2f>& cam_points, 
                                        const std::vector<cv::Point2f>& ground_meter_points) {
    if (cam_points.size() < 4 || ground_meter_points.size() < 4) {
        std::cerr << "[MapManager Error] 호모그래피 계산에는 최소 4개의 대응점이 필요합니다." << std::endl;
        return false;
    }

    H_cam_to_meters_ = cv::findHomography(cam_points, ground_meter_points);
    if (H_cam_to_meters_.empty()) {
        std::cerr << "[MapManager Error] 바닥 거리 변환 행렬 계산 실패" << std::endl;
        is_calibrated_ = false;
        return false;
    }

    is_calibrated_ = true;
    std::cout << "[MapManager] 카메라-거리(m) 호모그래피 행렬 계산 완료" << std::endl;
    return true;
}

cv::Point2f MapManager::transformToRelativeMeters(const cv::Point2f& cam_pixel) {
    if (!is_calibrated_ || H_cam_to_meters_.empty()) {
        return cv::Point2f(0.0f, 0.0f);
    }

    std::vector<cv::Point2f> src = { cam_pixel };
    std::vector<cv::Point2f> dst;
    cv::perspectiveTransform(src, dst, H_cam_to_meters_);

    if (!dst.empty()) {
        return dst[0]; // dst[0].x: 전방 거리(m), dst[0].y: 우측 거리(m)
    }
    return cv::Point2f(0.0f, 0.0f);
}

MapManager::GeoPoint MapManager::calculateTargetGeo(const GeoPoint& car_gps, double car_heading_deg, const cv::Point2f& rel_meters) {
    double heading_rad = car_heading_deg * (M_PI / 180.0);
    double forward_m = rel_meters.x;
    double right_m = rel_meters.y;

    // 헤딩 기준 회전 변환 (북쪽 이동량 dN, 동쪽 이동량 dE)
    double dN = forward_m * std::cos(heading_rad) - right_m * std::sin(heading_rad);
    double dE = forward_m * std::sin(heading_rad) + right_m * std::cos(heading_rad);

    // 미터 -> 위경도 변화량 변환 (WGS84 평면 근사식)
    double delta_lat = dN / 111139.0;
    double delta_lon = dE / (111139.0 * std::cos(car_gps.lat * (M_PI / 180.0)));

    return { car_gps.lat + delta_lat, car_gps.lon + delta_lon };
}

cv::Point2i MapManager::geoToMapPixel(const GeoPoint& target_gps) {
    if (map_size_.width == 0 || map_size_.height == 0) {
        return cv::Point2i(0, 0);
    }

    double x_ratio = (target_gps.lon - geo_top_left_.lon) / (geo_bottom_right_.lon - geo_top_left_.lon);
    double y_ratio = (geo_top_left_.lat - target_gps.lat) / (geo_top_left_.lat - geo_bottom_right_.lat);

    int px = static_cast<int>(x_ratio * map_size_.width) +6;
    int py = static_cast<int>(y_ratio * map_size_.height) +30;

    return cv::Point2i(
        std::clamp(px, 0, map_size_.width - 1),
        std::clamp(py, 0, map_size_.height - 1)
    );
}

cv::Mat MapManager::drawMarkers(const GeoPoint& car_gps, double car_heading_deg, 
                                bool person_detected, const std::vector<cv::Point2f>& rel_meters_list) {
    if (map_image_.empty()) return current_display_map_;

    current_display_map_ = map_image_.clone();

    // 1. RC카 현재 GPS 위치 렌더링 (파란색 원)
    cv::Point2i car_px = geoToMapPixel(car_gps);
    cv::circle(current_display_map_, car_px, 7, cv::Scalar(255, 0, 0), -1);
    cv::circle(current_display_map_, car_px, 9, cv::Scalar(255, 255, 255), 2);
    cv::putText(current_display_map_, "RC CAR", cv::Point(car_px.x + 10, car_px.y + 4),
                cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(255, 0, 0), 2);

    // 2. 감지된 사람 위치 렌더링 (빨간색 원)
    if (person_detected) {
        for (size_t i = 0; i < rel_meters_list.size(); ++i) {
            GeoPoint person_geo = calculateTargetGeo(car_gps, car_heading_deg, rel_meters_list[i]);
            cv::Point2i person_px = geoToMapPixel(person_geo);

            cv::circle(current_display_map_, person_px, 6, cv::Scalar(0, 0, 255), -1);
            cv::circle(current_display_map_, person_px, 8, cv::Scalar(0, 0, 0), 1);

            std::string label = "P" + std::to_string(i + 1);
            cv::putText(current_display_map_, label, cv::Point(person_px.x + 8, person_px.y - 4),
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 255), 1);
        }
    }

    return current_display_map_;
}
