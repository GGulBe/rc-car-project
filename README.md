# 🚗 RC-Car 객체 탐지 및 위치 추정 시스템

![C++](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Platform](https://img.shields.io/badge/platform-Raspberry%20Pi%204-c51a4a)
![OpenCV](https://img.shields.io/badge/OpenCV-4.x-5C3EE8)
![ONNX Runtime](https://img.shields.io/badge/ONNX%20Runtime-aarch64-black)

라즈베리파이 4를 탑재한 RC카가 카메라로 사람을 인식하고, 그 위치·방향·속도를 위성지도 위에 실시간으로 표시하는 임베디드 비전 프로젝트입니다.

## 목차

- [프로젝트 개요](#프로젝트-개요)
- [주요 기능](#주요-기능)
- [시스템 아키텍처](#시스템-아키텍처)
- [기술 스택](#기술-스택)
- [팀 구성](#팀-구성)
- [담당: 호모그래피 기반 좌표 변환 알고리즘](#담당-호모그래피-기반-좌표-변환-알고리즘)
- [제어 시스템](#제어-시스템)
- [AI 모델 개발](#ai-모델-개발)
- [폴더 구조](#폴더-구조)
- [개발/실행 환경](#개발실행-환경)
- [저장 범위](#저장-범위)

## 프로젝트 개요

- **목표**: RC카 카메라로 사람을 실시간 탐지하고, 탐지된 객체와 차량 자신의 위치·방향·속도를 위성지도 위에 정확한 좌표로 매핑
- **기간**: 2026년 여름방학 프로젝트
- **하드웨어**: Raspberry Pi 4, 카메라 모듈, BNO055 IMU, GPS 모듈(+보조 스마트폰 GPS), 모터/서보/PWM 구동계

## 주요 기능

- 카메라 프레임 기반 실시간 사람 탐지 (INT8 양자화 ONNX 모델, ONNX Runtime)
- 호모그래피 캘리브레이션을 통한 카메라 픽셀 → 실측 좌표(m) 변환
- 위성지도 위 실시간 위치·방향·속도 매핑
- mutex 기반 멀티스레드 구조(메인 / 객체 탐지 / GPS)로 20~25fps 안정적인 프레임 확보
- 기상 조건에 영향을 덜 받는 스마트폰 GPS 릴레이 방식의 위치 보정

## 시스템 아키텍처

```
                ┌────────────────────┐
                │   Camera (실시간)    │
                └─────────┬──────────┘
                          │ frame
                          ▼
   ┌───────────────────────────────────────────┐
   │   AI Thread — Detector (ONNX Runtime)       │
   │   TAIL004 INT8 person detector              │
   └─────────────────────┬───────────────────────┘
                          │ pixel bbox (mutex 보호)
                          ▼
   ┌───────────────────────────────────────────┐
   │  CalibrationWorker — 호모그래피 좌표 변환     │
   │  (cv::findHomography, 픽셀 → 실측 m 좌표)    │
   └─────────────────────┬───────────────────────┘
                          │ 실측 좌표
                          ▼
   ┌───────────────────────────────────────────┐   ┌─────────────────────────┐
   │  MapManager — 위성지도 매핑 · 속도 계산       │◄──┤ GPS Thread              │
   │  (지도 왜곡 보정 포함)                        │  │ GPSWorker / PhoneGpsReceiver │
   └─────────────────────┬───────────────────────┘   └─────────────────────────┘
                          │
                          ▼
   ┌───────────────────────────────────────────┐
   │  Main Thread — 제어 루프                     │
   │  Bno055(IMU) · MotorController · ServoController │
   │  PwmController · GpioOutput · UartDevice · TerminalInput │
   └───────────────────────────────────────────┘
```

메인 / 객체 탐지(AI) / GPS 3개의 스레드가 `mutex`로 데이터를 안전하게 공유하며 동작합니다.

## 기술 스택

| 분류 | 사용 기술 |
|---|---|
| 언어 | C++17 |
| 컴퓨터 비전 | OpenCV (`cv::findHomography` 등) |
| AI 추론 | ONNX Runtime (aarch64), INT8 양자화 모델 |
| 하드웨어 인터페이스 | I2C, UART, GPIO, PWM |
| 센서 | BNO055 IMU, GPS(+스마트폰 GPS 릴레이) |
| 동시성 | std::thread, std::mutex |

## 팀 구성

| 이름 | GitHub | 담당 |
|---|---|---|
| **정민혁** | [@GGulBe](https://github.com/GGulBe) | 좌표 변환 알고리즘 — 호모그래피 캘리브레이션, GPS 융합, 지도 매핑, 속도 계산 |
| **김우석** | [@WooSICK](https://github.com/WooSICK) | 제어 알고리즘 — 모터/서보/PWM 구동, GPIO·UART 하드웨어 제어, IMU 연동 |
| **박지원** | [@parkjiwon3225-ship-it](https://github.com/parkjiwon3225-ship-it) | 객체 탐지 딥러닝 — 사람 탐지 모델 학습·경량화·ONNX 배포 ([`ai-model/`](ai-model/) 참고) |

## 담당: 호모그래피 기반 좌표 변환 알고리즘

카메라 픽셀 좌표를 실제 바닥의 미터 좌표로 변환하는 호모그래피 캘리브레이션을 설계·구현했습니다. 이 값을 이용해 카메라에 탐지된 사람과 RC카의 위치·방향을 위성지도 위에 매핑하고, RC카의 속도 계산과 지도 마킹까지 담당했습니다.

### 구현 방식

- 카메라가 바라보는 바닥면에서 좌우 간격이 동일한 4개의 기준점을 1m, 3m 거리에 각각 찍어 `cv::findHomography`로 픽셀 좌표 ↔ 실측 좌표 간 3x3 변환 행렬을 계산했습니다.
- 이 변환 행렬을 통해 객체 탐지 모델(`Detector`)이 반환한 픽셀 좌표를 실제 미터 단위 좌표로 변환하고, 위성지도(`MapManager`) 위에 정확한 위치로 표시했습니다.
- 하드웨어의 한계와 위성지도가 완전히 수직으로 촬영된 것이 아니라는 점 때문에 발생하는 오차는 일정 부분 보정을 적용해 지도 위에 자연스럽게 나타나도록 조정했습니다.

### 트러블슈팅

| # | 문제 | 해결 |
|---|---|---|
| 1 | 움직이는 화면에서의 캘리브레이션 정확도 | 좌우 간격을 동일하게 맞춘 4개 기준점(1m, 3m 거리)으로 캘리브레이션 |
| 2 | 캘리브레이션 점 과다로 인한 프로그램 충돌(병목) | 점 개수 제한 적용 |
| 3 | 프레임 저하 | mutex 기반 멀티스레드(메인 / 객체 탐지 / GPS)로 작업 분리, 20~25fps 확보 |
| 4 | 날씨에 따른 GPS 오차 | 스마트폰 GPS 값을 라즈베리파이로 송수신하는 릴레이 방식으로 전환 |
| 5 | 위성지도 시점 왜곡으로 인한 위치 표시 오차 | 좌표 값 보정을 적용해 실제 위치와 근접하게 표시 |

### 결과

- 객체(사람) 인식이 끊김 없이 안정적으로 동작
- 멀티스레드 최적화를 통해 20~25fps 수준의 안정적인 프레임 확보
- RC카와 인식된 사람의 위치·방향·속도가 위성지도 위에 정확한 좌표로 표시되는 데모 완성
- 스마트폰 GPS 릴레이 방식으로 GPS 오차 문제를 해결해, 기상 조건에 관계없이 안정적인 위치 추정 가능

## 제어 시스템

**김우석**([@WooSICK](https://github.com/WooSICK)) 담당 영역으로, RC카의 실제 구동을 맡는 하드웨어 제어 계층입니다.

| 모듈 | 역할 |
|---|---|
| `MotorController` | 구동 모터 제어 |
| `ServoController` / `PwmController` | 조향 및 PWM 신호 제어 |
| `GpioOutput` | GPIO 출력 제어 |
| `I2CDevice` / `UartDevice` | I2C·UART 하드웨어 통신 |
| `Bno055` | IMU 센서를 통한 자세(방향) 측정 |
| `TerminalInput` | 터미널 입력을 통한 조작 인터페이스 |

## AI 모델 개발

**박지원**([@parkjiwon3225-ship-it](https://github.com/parkjiwon3225-ship-it)) 담당 영역으로, 자세한 개발·학습·경량화 자료는 [`ai-model/`](ai-model/)에 정리되어 있습니다.

- 최종 배포 모델: [`person_detector_tail004_int8.onnx`](ai-model/05_final-release/2026-08-20_tail004/models/person_detector_tail004_int8.onnx)
- 모델 설명과 선택 근거: [`MODEL_CARD.md`](ai-model/05_final-release/2026-08-20_tail004/MODEL_CARD.md)
- C++/OpenCV/ONNX Runtime 연동 규격: [`INTEGRATION_CONTRACT.md`](ai-model/05_final-release/2026-08-20_tail004/INTEGRATION_CONTRACT.md)
- AI 전체 개발 이력: [`ai-model/README.md`](ai-model/README.md)

최종 모델은 R46 / 448×336 / seed 15 checkpoint를 선택적 QDQ INT8로 경량화한 TAIL004입니다. 학습 정확도만으로 결정하지 않고 사람 Recall, 작은 사람 Recall, Raspberry Pi 계산량, FP32 출력 보존성, 실제 카메라 검증을 함께 고려했습니다.
| 영역 | 내용 |
|---|---|
| [`01_model-development`](ai-model/01_model-development/) | group-aware 데이터 분할, custom anchor-free detector, 학습 기반 코드 |
| [`02_training-experiments`](ai-model/02_training-experiments/) | 날짜별 병렬 학습, hard-negative, 최종 해상도·seed 비교 |
| [`03_lightweight-deployment`](ai-model/03_lightweight-deployment/) | ONNX 변환, INT8 방식 비교, Raspberry Pi 검증 |
| [`04_team-integration`](ai-model/04_team-integration/) | 캘리브레이션·제어 팀 연동 자료 |
| [`05_final-release`](ai-model/05_final-release/) | 최종 배포 모델, 기준 FP32, 해시, 통합 계약 |

최종 모델은 `R46 / 448×336 / seed 15` checkpoint를 선택적 QDQ INT8로 경량화한 `TAIL004`입니다. 학습 정확도만으로 결정하지 않고 사람 Recall, 작은 사람 Recall, Raspberry Pi 계산량, FP32 출력 보존성, 실제 카메라 검증을 함께 고려했습니다.

## 폴더 구조

```
rc-car-project/
├── rc_car_cpp/              # C++ 로봇 제어 · 좌표 변환 · 객체 탐지 통합 코드
│   ├── Detector.*           # ONNX Runtime 기반 사람 탐지
│   ├── CalibrationWorker.*  # 호모그래피 캘리브레이션 
│   ├── MapManager.*         # 위성지도 매핑 · 속도 계산 
│   ├── GPSWorker.* / PhoneGpsReceiver.*  # GPS 처리 
│   ├── MotorController.* / ServoController.* / PwmController.* / GpioOutput.*  # 제어
│   ├── Bno055.* / I2CDevice.* / UartDevice.*  # 센서·하드웨어 통신
│   └── onnxruntime/         # Raspberry Pi(aarch64)용 ONNX Runtime 라이브러리
└── ai-model/                # AI 모델 개발 전체 이력 
```

## 개발/실행 환경

- Raspberry Pi 4 (aarch64) 환경에서 개발 및 검증
- OpenCV, 저장소에 포함된 ONNX Runtime(aarch64) 라이브러리(`rc_car_cpp/onnxruntime/`)를 사용해 빌드
- 카메라, BNO055 IMU, GPS 모듈, 모터/서보 드라이버 등 하드웨어 연결이 필요합니다

## 저장 범위

GitHub에는 재현과 검토에 필요한 코드, 설정, 지표, 대표 checkpoint, 최종 ONNX 및 검증 보고서를 보존합니다. 원본 학습 데이터셋, calibration 이미지, 중복 ZIP, 캐시, 로컬 가상환경, 발표 준비 자료는 제외했습니다.
