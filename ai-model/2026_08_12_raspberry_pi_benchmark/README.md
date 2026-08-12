# Raspberry Pi FP32 / INT8 1차 벤치마크

RC-Car Person Detection 모델을 Raspberry Pi 4B CPU에서 실제 카메라와 함께 실행해 보는 1차 배포 시험 코드입니다.

이 시험의 목적은 최종 정확도를 결정하는 것이 아니라 다음 사항을 먼저 확인하는 것입니다.

- ONNX FP32와 Static INT8 모델이 Raspberry Pi에서 정상적으로 로드되는가
- 카메라 입력을 포함했을 때 실제 처리속도와 지연시간은 어느 정도인가
- INT8 경량화가 속도, 메모리 및 온도에 어떤 차이를 만드는가
- 같은 프레임에서 FP32와 INT8의 검출 개수, confidence, bounding box가 얼마나 일치하는가
- 장시간 CPU 추론 중 저전압 또는 thermal throttling이 발생하는가

> 현재 사용 모델은 최종 학습 모델이 아니라 Raspberry Pi 실행 경로를 검증하기 위한 임시 모델입니다. 최종 `best.pt` 선정 후 같은 시험을 다시 실행해야 합니다.

## 파일 구성

| 파일 | 설명 |
|---|---|
| `pi_first_benchmark.py` | 측정 및 결과 저장 프로그램 |
| `pi_int8_camera_test.py` | VNC에서 INT8 사람 검출을 확인하고 수치를 저장하는 주 시험 프로그램 |
| `01_setup_pi.sh` | Raspberry Pi 최초 환경 설치 |
| `02_run_int8_camera_test.sh` | INT8 카메라 시각 시험 실행 |
| `03_run_fp32_int8_benchmark.sh` | FP32/INT8 비교 벤치마크 실행 |
| `requirements-pi.txt` | Python 패키지 목록 |
| `.gitignore` | 가상환경과 측정 결과 제외 |

모델 파일은 저장소의 `2026_08_12_calibration_handoff` 폴더에 있습니다.

- `person_detector_fp32.onnx`
- `person_detector_int8.onnx`

실행 전 두 모델을 이 README와 같은 폴더로 복사해야 합니다.

```text
2026_08_12_raspberry_pi_benchmark/
├── person_detector_fp32.onnx
├── person_detector_int8.onnx
├── pi_first_benchmark.py
├── pi_int8_camera_test.py
├── requirements-pi.txt
├── 01_setup_pi.sh
├── 02_run_int8_camera_test.sh
└── 03_run_fp32_int8_benchmark.sh
```

## 모델 입출력

입력:

```text
Shape  : [1, 3, 240, 320]
Layout : NCHW
Type   : float32
Color  : RGB
Range  : 0.0 ~ 1.0
```

출력:

```text
Shape : [1, 5, 15, 20]

0 : objectness logit
1 : center X offset logit
2 : center Y offset logit
3 : bounding-box width logit
4 : bounding-box height logit
```

현재 임시 후처리 설정은 confidence `0.20`, NMS IoU `0.20`입니다.

## Raspberry Pi 준비

Raspberry Pi 터미널에서 이 폴더로 이동합니다.

```bash
cd /복사한/경로/2026_08_12_raspberry_pi_benchmark
```

최초 한 번만 다음을 실행합니다.

```bash
chmod +x 01_setup_pi.sh 02_run_int8_camera_test.sh 03_run_fp32_int8_benchmark.sh
./01_setup_pi.sh
```

스크립트는 다음 작업을 수행합니다.

1. `python3-venv`, `python3-opencv` 설치
2. CSI 카메라용 Picamera2와 OpenCV 설치
3. 시스템 패키지를 사용할 수 있는 `.venv` 생성
4. ONNX Runtime과 `psutil` 설치
5. 필수 라이브러리 import 검사

마지막에 `SETUP PASS`가 출력되어야 합니다.

카메라 장치를 확인합니다.

```bash
ls -l /dev/video*
```

기본 카메라 번호는 `0`입니다. OV5647 CSI 카메라는 RAW 장치인 `/dev/video0`을 OpenCV로 직접 열지 않고 Picamera2/libcamera를 사용합니다. `rpicam-hello --list-cameras`의 번호가 다르면 실행 스크립트의 `--camera 0`을 변경합니다.

Picamera2 포맷 이름은 메모리의 채널 순서와 직관적으로 반대로 보일 수 있습니다. OpenCV가 기대하는 BGR 배열을 받기 위해 `RGB888` 스트림을 사용하며, 모델 입력 직전에 BGR에서 RGB로 변환합니다.

## INT8 카메라 시각 시험

```bash
./02_run_int8_camera_test.sh
```

기본 설정:

| 항목 | 값 |
|---|---:|
| INT8 카메라 시험 | 600초 |
| ONNX Runtime CPU thread | 2 |
| 카메라 요청 크기 | 640×480 |
| 카메라 요청 FPS | 30 |
| 모델 입력 | 320×240 |
| 온도 자동 종료 | 75°C |

VNC 화면에 사람 박스, confidence, 추론시간, 전체 FPS와 CPU 온도가 표시됩니다. `Q`를 누르면 정상 종료되며 그 시점까지의 결과가 보존됩니다. `[`와 `]` 키로 confidence threshold를 0.05씩 조절할 수 있고 변경된 threshold도 프레임별 CSV에 기록됩니다.

영상 파일을 저장하지 않고도 장면별 결과를 구분할 수 있도록 OpenCV 카메라 창에 초점을 두고 다음 숫자 키를 누릅니다. 선택한 장면은 다음 프레임부터 CSV의 `scenario` 열에 기록됩니다.

| 키 | 장면 |
|---:|---|
| `0` | 사람이 없는 배경 |
| `1` | 가까운 사람 |
| `2` | 중간 거리 사람 |
| `3` | 먼 사람 |
| `4` | 움직이는 사람 |
| `5` | 여러 사람 |

영상과 카메라 이미지는 저장하지 않습니다. CSV와 JSON 숫자 결과만 저장합니다.

화면을 보면서 시험하려면 다음처럼 직접 실행할 수 있습니다.

```bash
.venv/bin/python pi_int8_camera_test.py --duration 600
```

화면 없이 순수 처리속도만 측정하려면 `--no-preview`를 추가합니다.

FP32와 INT8을 같은 조건으로 비교하는 추가 시험은 다음과 같습니다.

```bash
./03_run_fp32_int8_benchmark.sh
```

## 시험 장면

실제 RC카 카메라 높이와 각도에 가깝게 놓고 다음 장면을 골고루 포함합니다.

1. 사람이 없는 배경
2. 가까운 사람 1명
3. 중간 거리 사람 1명
4. 가능한 범위에서 먼 사람
5. 사람이 좌우로 이동하는 장면

두 모델의 공정한 비교를 위해 시험 중 카메라 위치와 조명을 가능한 한 유지합니다.

## 수집 결과

실행 후 다음 폴더가 생성됩니다.

```text
pi_int8_camera_results/YYYYMMDD_HHMMSS/
├── summary.json
├── frame_metrics.csv
└── detections.csv
```

### `summary.json`

- Raspberry Pi OS, CPU, RAM, Python, OpenCV, ONNX Runtime 버전
- 실제 카메라 해상도와 카메라가 보고한 FPS
- 평균/중앙값/P95 추론 지연시간과 카메라 포함 전체 FPS
- CPU 온도·주파수·사용률과 프로세스 CPU·RAM
- 카메라 timestamp부터 검출 결과가 만들어질 때까지 걸린 시간
- 노출시간, gain, Lux, 실제 프레임 주기
- 시험 시작/종료 throttling 상태
- confidence 및 사람 박스 크기 분포

### `frame_metrics.csv`

각 프레임의 다음 시간을 별도로 기록합니다.

- 카메라 캡처
- 전처리
- ONNX 추론
- 후처리
- 전체 반복
- CPU 사용률, RAM, 온도, 검출 개수

### `detections.csv`

검출된 사람마다 다음 값을 기록합니다.

- confidence와 프레임 번호
- 모델 입력 및 카메라 해상도 기준 bounding box
- 박스 중심점과 사람 발 위치에 가까운 bottom-center
- 정규화된 중심 및 bottom-center 좌표
- 사람 박스 너비·높이와 크기 구간

## 결과 해석 시 주의사항

- `model FPS`는 모델 추론만 기준으로 계산한 수치입니다.
- `end-to-end FPS`는 카메라 캡처, 전처리, 추론, 후처리를 포함합니다.
- P95 지연시간은 느린 쪽 5% 구간의 경계를 보여주므로 평균과 함께 봐야 합니다.
- `detection count agreement`는 두 모델의 상대적인 일치도이지 정답 기반 정확도가 아닙니다.
- 실제 mAP, Precision, Recall, F1은 정답 라벨이 있는 valid/test 데이터로 별도 평가해야 합니다.
- `vcgencmd get_throttled`가 `0x0`이 아니면 저전압 또는 온도 제한 기록을 해석해야 합니다.
- 현재 임시 모델 결과만으로 최종 FP32/INT8 배포 방식을 결정하지 않습니다.

## 추가 환경 정보

시험 결과와 함께 다음 출력도 기록합니다.

```bash
uname -a
cat /etc/os-release
vcgencmd get_throttled
```

최종 모델에서는 동일 시험을 반복한 뒤 정확도 손실, Raspberry Pi 전체 FPS, 발열 안정성을 함께 고려해 FP32 또는 INT8 배포 방식을 선택합니다.
