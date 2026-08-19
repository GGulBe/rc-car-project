#ifndef MAP_MANAGER_H
#define MAP_MANAGER_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

class MapManager {
public:
    struct GeoPoint {
        double lat = 0.0;
        double lon = 0.0;
    };

    MapManager();
    ~MapManager();

    // 1. 위성 지도 이미지 로드
    bool loadMap(const std::string& map_path);

    // 2. 위성 지도의 좌상단(NW), 우하단(SE) 실제 위경도 및 이미지 크기 설정
    void setMapGeoBounds(GeoPoint top_left, GeoPoint bottom_right, cv::Size map_size);

    // 3. 카메라 화소 -> RC카 기준 전방/측면 미터(m) 호모그래피 계산
    bool calibrateCameraToMeters(const std::vector<cv::Point2f>& cam_points, 
                                const std::vector<cv::Point2f>& ground_meter_points);

    // 4. 카메라 화소(발밑) -> RC카 기준 상대 위치 (x: 전방 m, y: 우측 m)
    cv::Point2f transformToRelativeMeters(const cv::Point2f& cam_pixel);

    // 5. RC카 GPS + BNO055 방위각 + 상대 거리(m) -> 대상의 실제 위경도 산출
    GeoPoint calculateTargetGeo(const GeoPoint& car_gps, double car_heading_deg, const cv::Point2f& rel_meters);

    // 6. 실제 위경도 -> 위성 지도 이미지 내 픽셀 (X, Y) 좌표 변환
    cv::Point2i geoToMapPixel(const GeoPoint& target_gps);

    // 7. 실시간 지도 렌더링 함수
    cv::Mat drawMarkers(const GeoPoint& car_gps, double car_heading_deg, 
                        bool person_detected, const std::vector<cv::Point2f>& rel_meters_list);

private:
    cv::Mat map_image_;
    cv::Mat current_display_map_;
    cv::Mat H_cam_to_meters_;
    GeoPoint geo_top_left_;
    GeoPoint geo_bottom_right_;
    cv::Size map_size_;
    bool is_calibrated_;
};

#endif