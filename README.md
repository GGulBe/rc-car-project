# RC-Car Project — AI Model Branch

이 브랜치의 AI 개발·학습·경량화 자료는 [`ai-model/`](ai-model/)에 정리되어 있습니다. 로봇 제어 영역인 `rc_car_cpp/`는 이번 정리에서 수정하지 않았습니다.

## 최종 AI 결과

- 최종 배포 모델: [`person_detector_tail004_int8.onnx`](ai-model/05_final-release/2026-08-20_tail004/models/person_detector_tail004_int8.onnx)
- 모델 설명과 선택 근거: [`MODEL_CARD.md`](ai-model/05_final-release/2026-08-20_tail004/MODEL_CARD.md)
- C++/OpenCV/ONNX Runtime 연동 규격: [`INTEGRATION_CONTRACT.md`](ai-model/05_final-release/2026-08-20_tail004/INTEGRATION_CONTRACT.md)
- AI 전체 개발 이력: [`ai-model/README.md`](ai-model/README.md)

최종 모델은 `R46 / 448×336 / seed 15` checkpoint를 선택적 QDQ INT8로 경량화한 `TAIL004`입니다. 학습 정확도만으로 결정하지 않고 사람 Recall, 작은 사람 Recall, Raspberry Pi 계산량, FP32 출력 보존성, 실제 카메라 검증을 함께 고려했습니다.

## 영역별 자료

| 영역 | 내용 |
|---|---|
| [`01_model-development`](ai-model/01_model-development/) | group-aware 데이터 분할, custom anchor-free detector, 학습 기반 코드 |
| [`02_training-experiments`](ai-model/02_training-experiments/) | 날짜별 병렬 학습, hard-negative, 최종 해상도·seed 비교 |
| [`03_lightweight-deployment`](ai-model/03_lightweight-deployment/) | ONNX 변환, INT8 방식 비교, Raspberry Pi 검증 |
| [`04_team-integration`](ai-model/04_team-integration/) | 캘리브레이션·제어 팀 연동 자료 |
| [`05_final-release`](ai-model/05_final-release/) | 최종 배포 모델, 기준 FP32, 해시, 통합 계약 |

## 저장 범위

GitHub에는 재현과 검토에 필요한 코드, 설정, 지표, 대표 checkpoint, 최종 ONNX 및 검증 보고서를 보존합니다. 원본 학습 데이터셋, calibration 이미지, 중복 ZIP, 캐시, 로컬 가상환경, 발표 준비 자료는 제외했습니다.
