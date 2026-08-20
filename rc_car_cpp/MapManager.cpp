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
        return dst[0];
    }
    return cv::Point2f(0.0f, 0.0f);
}

MapManager::GeoPoint MapManager::calculateTargetGeo(const GeoPoint& car_gps, double car_heading_deg, const cv::Point2f& rel_meters) {
    // IMU 방위각을 라디안으로 변환
    double heading_rad = car_heading_deg * (M_PI / 180.0);
    double forward_m = rel_meters.x;
    double right_m = rel_meters.y;

    // 북향(dN), 동향(dE) 상대 변위 계산
    double dN = forward_m * std::cos(heading_rad) - right_m * std::sin(heading_rad);
    double dE = forward_m * std::sin(heading_rad) + right_m * std::cos(heading_rad);

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

    // 현장 캘리브레이션 오프셋 유지 (+6, +30)
    int px = static_cast<int>(x_ratio * map_size_.width) - 7;
    int py = static_cast<int>(y_ratio * map_size_.height) + 28;

    return cv::Point2i(
        std::clamp(px, 0, map_size_.width - 1),
        std::clamp(py, 0, map_size_.height - 1)
    );
}

cv::Mat MapManager::drawMarkers(const GeoPoint& car_gps, double car_heading_deg, 
                                bool person_detected, const std::vector<cv::Point2f>& rel_meters_list) {
    if (map_image_.empty()) return current_display_map_;

    current_display_map_ = map_image_.clone();

    // 1. 궤적 점 추가 및 최대 개수 관리 (150개 제한)
    if (trajectory_.empty() || 
        std::abs(trajectory_.back().lat - car_gps.lat) > 0.000008 || 
        std::abs(trajectory_.back().lon - car_gps.lon) > 0.000008) {
        
        trajectory_.push_back(car_gps);
        
        if (trajectory_.size() > 150) {
            trajectory_.erase(trajectory_.begin());
        }
    }

    // 2. CPU 연산 부하가 적은 LINE_8로 궤적 선 그리기
    if (trajectory_.size() > 1) {
        std::vector<cv::Point> pts;
        pts.reserve(trajectory_.size());
        for (const auto& pt : trajectory_) {
            pts.push_back(geoToMapPixel(pt));
        }
        cv::polylines(current_display_map_, pts, false, cv::Scalar(0, 165, 255), 2, cv::LINE_8);
    }

    // 3. RC카 현재 위치 렌더링
    cv::Point2i car_px = geoToMapPixel(car_gps);
    cv::circle(current_display_map_, car_px, 7, cv::Scalar(255, 0, 0), -1);
    cv::circle(current_display_map_, car_px, 9, cv::Scalar(255, 255, 255), 2);
    cv::putText(current_display_map_, "RC CAR", cv::Point(car_px.x + 10, car_px.y + 4),
                cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(255, 0, 0), 2);

    // 4. 감지된 사람 위치 렌더링 (음수 거리 및 비정상 범위 필터링 적용)
    if (person_detected) {
        for (size_t i = 0; i < rel_meters_list.size(); ++i) {
            // 전방 거리가 0m 이하이거나 15m 이상인 비정상 투영값은 지도에 렌더링하지 않음
            if (rel_meters_list[i].x <= 0.1f || rel_meters_list[i].x > 15.0f) {
                continue;
            }

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
