# RC-Car 객체 탐지 및 위치 추정 프로젝트

라즈베리파이4를 탑재한 RC카가 카메라로 사람을 인식하고, 그 위치·방향·속도를 위성지도 위에 실시간으로 정확히 표시하는 프로젝트입니다.

- **기간**: 2026년 여름방학 프로젝트
- **팀 구성 (3인)**: 제어 알고리즘 / 객체 탐지 딥러닝([`ai-model/`](ai-model/) 참고) / **좌표 변환 알고리즘 (본인 담당)**
- **코드**: [`rc_car_cpp/`](rc_car_cpp/) — C++, OpenCV, Raspberry Pi 4

---

## 본인 담당: 호모그래피 기반 좌표 변환 알고리즘

카메라 픽셀 좌표를 실제 바닥의 미터 좌표로 변환하는 호모그래피 캘리브레이션을 설계·구현했습니다. 이 값을 이용해 카메라에 탐지된 사람과 RC카의 위치·방향을 위성지도 위에 매핑하고, RC카의 속도 계산과 지도 마킹까지 담당했습니다.

### 구현 방식

- 카메라가 바라보는 바닥면에서 좌우 간격이 동일한 4개의 기준점을 1m, 3m 거리에 각각 찍어 `cv::findHomography`로 픽셀 좌표 ↔ 실측 좌표 간 3x3 변환 행렬을 계산했습니다.
- 이 변환 행렬을 통해 객체 탐지 모델이 반환한 픽셀 좌표를 실제 미터 단위 좌표로 변환하고, 위성지도 위에 정확한 위치로 표시했습니다.
- 하드웨어의 한계와 위성지도가 완전히 수직으로 촬영된 것이 아니라는 점 때문에 발생하는 오차는 일정 부분 보정을 적용해 지도 위에 자연스럽게 나타나도록 조정했습니다.

### 트러블슈팅

1. **움직이는 화면에서의 캘리브레이션** — 차량이 움직이는 상태에서도 위성지도 위에 객체 위치를 정확히 찍어내는 것이 관건이었습니다. 좌우 간격을 동일하게 맞춘 4개 기준점(1m, 3m 거리)을 이용해 캘리브레이션하는 방식으로 해결했습니다.
2. **점이 너무 많이 찍혀 발생한 프로그램 충돌** — 캘리브레이션 중 점이 과도하게 찍히면서 병목현상으로 프로그램이 자꾸 충돌했습니다. 원인을 찾아 점 개수에 제한을 걸어 해결했습니다.
3. **프레임 저하** — mutex를 이용한 멀티스레드 구조(① 메인 ② 객체 탐지 ③ GPS)로 작업을 분리해 20~25fps 수준의 안정적인 프레임을 확보했습니다.
4. **날씨에 따른 GPS 오차** — 기상 조건에 따라 GPS 신호가 잘 잡히지 않는 문제가 있어, 핸드폰의 GPS 값을 라즈베리파이로 송수신하는 방식으로 전환했습니다. 결과적으로 더 깔끔하고 빠른 위치 추정이 가능해졌습니다.
5. **지도상 위치 왜곡 보정** — 위성지도가 완전한 수직 시점이 아니고 하드웨어 자체의 오차도 있어, 이를 반영해 좌표 값을 보정하여 지도 위에 실제 위치와 가깝게 표시되도록 했습니다.

### 결과

- 객체(사람) 인식이 끊김 없이 안정적으로 동작
- 멀티스레드 최적화를 통해 20~25fps 수준의 안정적인 프레임 확보
- RC카와 인식된 사람의 위치·방향·속도가 위성지도 위에 정확한 좌표로 표시되는 데모 완성
- 휴대폰 GPS 릴레이 방식으로 GPS 오차 문제를 해결해, 기상 조건에 관계없이 안정적인 위치 추정 가능

캘리브레이션 좌표 기하학, 멀티스레드 구조 설계 등 생소했던 부분은 Gemini를 활용해 학습하고 실전에 적용했습니다.

---

## AI 모델 개발 (팀원 담당)

이 브랜치의 AI 개발·학습·경량화 자료는 [`ai-model/`](ai-model/)에 정리되어 있습니다.

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

## 저장 범위

GitHub에는 재현과 검토에 필요한 코드, 설정, 지표, 대표 checkpoint, 최종 ONNX 및 검증 보고서를 보존합니다. 원본 학습 데이터셋, calibration 이미지, 중복 ZIP, 캐시, 로컬 가상환경, 발표 준비 자료는 제외했습니다.
