#include "GPSWorker.h"
#include <iostream>

UartDevice::gpsdata latestGps;
std::mutex gpsMutex;

// 💡 선배님 피드백 반영: GPS Out of Range(튀는 값) 필터링을 위한 경계값 설정
// (사용하시는 실제 주행 지역에 맞게 아래 Min/Max 범위를 수정하세요)
const double LAT_MIN = 37.5000;
const double LAT_MAX = 37.7000;
const double LON_MIN = 127.0000;
const double LON_MAX = 127.2000;

// 직전의 유효한 위치를 기억하기 위한 정적 변수
static double last_valid_lat = 37.58635;
static double last_valid_lon = 127.09746;
static bool has_initial_fix = false;

void gpsWorker(UartDevice& gps, std::atomic<bool>& running)
{
    while (running.load()) {
        UartDevice::gpsdata gpsdata = gps.parseRmc();

        // GPS 고정이 성공했을 때만 범위 검사 수행
        if (gpsdata.gpsfix) {
            // Out of Range 검사 (지정된 구역 내에 있는지 확인)
            if (gpsdata.lat >= LAT_MIN && gpsdata.lat <= LAT_MAX &&
                gpsdata.lon >= LON_MIN && gpsdata.lon <= LON_MAX) {
                
                // 범위 내 정상 값이면 최신값 갱신 및 유효 위치 기억
                last_valid_lat = gpsdata.lat;
                last_valid_lon = gpsdata.lon;
                has_initial_fix = true;

                std::lock_guard<std::mutex> lock(gpsMutex);
                latestGps = gpsdata;
            } else {
                // 범위 밖으로 튀는 값이면 쓰레기 값으로 판단하고 직전 정상 위치로 보정
                std::cerr << "[GPS Warning] Out of Range detected! Lat: " << gpsdata.lat 
                          << ", Lon: " << gpsdata.lon << " -> Ignored, keeping last valid position." << std::endl;
                
                UartDevice::gpsdata corrected_data = gpsdata;
                corrected_data.lat = last_valid_lat;
                corrected_data.lon = last_valid_lon;
                corrected_data.gpsfix = has_initial_fix;

                std::lock_guard<std::mutex> lock(gpsMutex);
                latestGps = corrected_data;
            }
        } else {
            // GPS 고정 안 됨 상태 그대로 전달
            std::lock_guard<std::mutex> lock(gpsMutex);
            latestGps = gpsdata;
        }
    }
}
