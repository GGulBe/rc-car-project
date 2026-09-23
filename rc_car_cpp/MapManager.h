#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class MapManager {
public:
    struct GeoPoint {
        double lat;
        double lon;
    };

    MapManager();
    ~MapManager();

    bool loadMap(const std::string& map_path);
    void setMapGeoBounds(GeoPoint top_left, GeoPoint bottom_right, cv::Size map_size);
    bool calibrateCameraToMeters(const std::vector<cv::Point2f>& cam_points, const std::vector<cv::Point2f>& ground_meter_points);
    cv::Point2f transformToRelativeMeters(const cv::Point2f& cam_pixel);
    GeoPoint calculateTargetGeo(const GeoPoint& car_gps, double car_heading_deg, const cv::Point2f& rel_meters);
    cv::Point2i geoToMapPixel(const GeoPoint& target_gps);
    
    cv::Mat drawMarkers(const GeoPoint& car_gps, double car_heading_deg, 
                        bool person_detected, const std::vector<cv::Point2f>& rel_meters_list);

    // 경로 초기화 함수
    void clearTrajectory() { trajectory_.clear(); }

private:
    bool is_calibrated_;
    cv::Mat map_image_;
    cv::Mat current_display_map_;
    cv::Size map_size_;
    
    GeoPoint geo_top_left_;
    GeoPoint geo_bottom_right_;
    cv::Mat H_cam_to_meters_;

    std::vector<GeoPoint> trajectory_;
};
