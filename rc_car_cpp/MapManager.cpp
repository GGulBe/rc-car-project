#include "MapManager.h"
#include <iostream>

MapManager::MapManager() : has_homography(false) {}

MapManager::~MapManager() {}

bool MapManager::loadMap(const std::string& map_path) {
    map_image = cv::imread(map_path);
    if (map_image.empty()) {
        std::cerr << "지도 이미지 로드 실패: " << map_path << std::endl;
        return false;
    }
    current_display_map = map_image.clone();
    return true;
}

bool MapManager::setHomography(const std::vector<cv::Point2f>& video_points, const std::vector<cv::Point2f>& map_points) {
    if (video_points.size() < 4 || map_points.size() < 4) {
        std::cerr << "호모그래피를 계산하려면 최소 4개의 대응점이 필요합니다." << std::endl;
        return false;
    }

    homography_matrix = cv::findHomography(video_points, map_points);
    if (homography_matrix.empty()) {
        std::cerr << "호모그래피 행렬 계산 실패" << std::endl;
        has_homography = false;
        return false;
    }

    has_homography = true;
    std::cout << "호모그래피 행렬 계산 완료" << std::endl;
    return true;
}

cv::Point2f MapManager::transformToMap(const cv::Point2f& video_point) {
    if (!has_homography) {
        return cv::Point2f(-1.0f, -1.0f);
    }

    std::vector<cv::Point2f> src = {video_point};
    std::vector<cv::Point2f> dst;
    cv::perspectiveTransform(src, dst, homography_matrix);

    if (!dst.empty()) {
        return dst[0];
    }
    return cv::Point2f(-1.0f, -1.0f);
}

cv::Mat MapManager::drawMarkers(double lat, double lon, bool person_detected, const cv::Point2f& person_map_pos) {
    std::vector<cv::Point2f> single_pos;
    if (person_detected) {
        single_pos.push_back(person_map_pos);
    }
    return drawMarkers(lat, lon, person_detected, single_pos);
}

cv::Mat MapManager::drawMarkers(double lat, double lon, bool person_detected, const std::vector<cv::Point2f>& person_map_positions) {
    if (map_image.empty()) return current_display_map;

    current_display_map = map_image.clone();

    // 1. 다중 사람 위치 마커 렌더링 (파란색 원)
    if (person_detected) {
        for (size_t i = 0; i < person_map_positions.size(); ++i) {
            const auto& pt = person_map_positions[i];

            if (pt.x >= 0 && pt.x < current_display_map.cols && pt.y >= 0 && pt.y < current_display_map.rows) {
                cv::circle(current_display_map, pt, 6, cv::Scalar(255, 0, 0), -1);
                cv::circle(current_display_map, pt, 8, cv::Scalar(0, 0, 0), 1);

                std::string label = "P" + std::to_string(i + 1);
                cv::putText(current_display_map, label, cv::Point(pt.x + 8, pt.y - 4),
                            cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 0, 0), 1);
            }
        }
    }

    // 2. RC Car GPS 위치 렌더링 (빨간색 원 + "RC CAR" 텍스트)
    // 💡 사용하시는 지도(map.jpg) 구역의 실제 최소/최대 위·경도 값으로 수정해주세요!
    double map_lat_min = 37.5800; // 남쪽 끝 위도
    double map_lat_max = 37.5900; // 북쪽 끝 위도
    double map_lon_min = 127.0900;// 서쪽 끝 경도
    double map_lon_max = 127.1000;// 동쪽 끝 경도

    // GPS(위도, 경도)를 지도 픽셀(x, y) 좌표로 선형 변환
    // (위도는 북쪽이 지도 위쪽(y=0)이므로 max_lat - lat 계산)
    float rc_x = static_cast<float>((lon - map_lon_min) / (map_lon_max - map_lon_min) * current_display_map.cols);
    float rc_y = static_cast<float>((map_lat_max - lat) / (map_lat_max - map_lat_min) * current_display_map.rows);

    // 지도 범위 내에 유효하게 들어올 때만 마커 표시
    if (rc_x >= 0 && rc_x < current_display_map.cols && rc_y >= 0 && rc_y < current_display_map.rows) {
        cv::Point2f rc_pt(rc_x, rc_y);
        cv::circle(current_display_map, rc_pt, 8, cv::Scalar(0, 0, 255), -1);  // 빨간색 채워진 원
        cv::circle(current_display_map, rc_pt, 10, cv::Scalar(255, 255, 255), 2); // 흰색 외곽선
        cv::putText(current_display_map, "RC CAR", cv::Point(rc_pt.x + 10, rc_pt.y + 4),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2);
    }

    return current_display_map;
}

cv::Mat MapManager::getDisplayMap() const {
    return current_display_map;
}
