# RC-Car AI 개발 및 경량화

RC카의 Raspberry Pi에서 사람을 실시간으로 감지하기 위한 AI 파트 저장소입니다. 이 폴더에는 모델 설계, 학습 실험, 경량화·현장 시험, 다른 팀과의 연동 자료를 개발 순서대로 보관합니다.

> **현재 기준 모델:** `results.4` FPN48, epoch 37 `best.pt`
>
> **선정 기준:** validation mAP50:95 `0.2525`, F1 `0.5713`, small-person recall `0.5294`
>
> **현재 배포 우선순위:** FP32 ONNX → INT8 Percentile → INT8 MinMax 순으로 Raspberry Pi 실측 비교
>
> **최종 Test split:** 아직 사용하지 않음

마지막 정리: **2026-08-13**

## 처음 보는 사람을 위한 빠른 길찾기

| 찾는 내용 | 먼저 볼 위치 |
|---|---|
| 프로젝트와 모델 구조 | [`01_model-development/2026-08-12_custom-anchor-free-detector`](01_model-development/2026-08-12_custom-anchor-free-detector/) |
| 6대 노트북 1차 실험 결과와 선택 근거 | [`02_training-experiments/2026-08-13_round1-six-laptop-study`](02_training-experiments/2026-08-13_round1-six-laptop-study/) |
| 날짜별 INT8 경량화 모델 전체와 변화·비교 | [`03_lightweight-deployment/2026-08-13_int8-model-evolution-and-finalists`](03_lightweight-deployment/2026-08-13_int8-model-evolution-and-finalists/) |
| Raspberry Pi에서 바로 시험할 최신 모델 | [`03_lightweight-deployment/2026-08-13_results4-pi-model-variants`](03_lightweight-deployment/2026-08-13_results4-pi-model-variants/) |
| Raspberry Pi 1차 카메라 실측 기록 | [`03_lightweight-deployment/2026-08-12_raspberry-pi-first-benchmark`](03_lightweight-deployment/2026-08-12_raspberry-pi-first-benchmark/) |
| 캘리브레이션 팀 입력·출력 규약 | [`04_team-integration/2026-08-12_calibration-interface-guide`](04_team-integration/2026-08-12_calibration-interface-guide/) |
| results.4 체크포인트와 이어학습 방법 | [`04_team-integration/2026-08-13_results4-training-handoff`](04_team-integration/2026-08-13_results4-training-handoff/) |

## 폴더 구조

```text
ai-model/
├─ 01_model-development/       모델 구조, 데이터 분할, 학습 코드
├─ 02_training-experiments/    날짜별 학습 결과와 비교 분석
├─ 03_lightweight-deployment/  ONNX·INT8 변환과 Raspberry Pi 시험
└─ 04_team-integration/        캘리브레이션 팀 전달 규약과 인계본
```

각 파트 안의 작업 폴더는 `YYYY-MM-DD_작업명` 형식을 사용합니다. 이름에 `legacy`가 붙은 폴더는 개발 이력을 보존하기 위한 구형 방식이며 현재 배포 후보가 아닙니다.

## 현재 모델의 방향

이 프로젝트는 YOLO 완성 모델을 가져다 쓰지 않았습니다. YOLO 형식은 데이터 라벨 저장 형식에만 사용하고, 탐지 모델은 프로젝트 요구에 맞춰 직접 구성했습니다.

```text
RGB 320×240
  → DSConv + Residual 경량 Backbone
  → P2/P3/P4/P5 Lightweight FPN
  → Anchor-free Detection Head
  → Classification + Quality + LTRB Box Regression
  → Decode + NMS
```

- 목적: Raspberry Pi에서 실시간 사람 감지
- 데이터: source group 단위 Train/Valid/Test 분할, group leakage `0`
- 학습 평가: mAP50:95, Precision, Recall, F1, tiny/small recall, loss, 속도, VRAM
- 배포 평가: 추론 지연시간, sensor-to-result 지연시간, FPS, 발열, 오검출, 박스 위치
- `best.pt` 저장 기준: validation mAP50:95
- 최종 선택 기준: mAP 하나가 아니라 정확도·Recall·작은 사람 검출·Pi 속도·발열을 함께 비교

## 날짜별 진행 기록

| 날짜 | 단계 | 결과 |
|---|---|---|
| 2026-08-06 | 초기 Person Detector v2 학습 | 초기 PyTorch 모델과 노트북 생성, 현재는 legacy 보관 |
| 2026-08-07 | 초기 ONNX/INT8 변환 | 변환 파이프라인 검증, 현재는 legacy 보관 |
| 2026-08-12 | 커스텀 모델·데이터 파이프라인 | group-aware split, DSConv+Residual+FPN+anchor-free 모델, 학습·평가 코드 완성 |
| 2026-08-12 | 캘리브레이션 사전 전달 | 최종 모델 전 FP32/INT8 임시 모델로 팀 간 ONNX 연결 구조 확인 |
| 2026-08-12 | Raspberry Pi 1차 현장 시험 | 2 threads가 약 90 ms 추론으로 열적으로 안정적임을 확인 |
| 2026-08-13 | 6대 노트북 1차 실험 분석 | FPN48 results.4를 1순위로 선정, 불리한 설정 제거 |
| 2026-08-13 | results.4 팀 전달 | best/last 체크포인트, 학습 코드, 이어학습 안내 제공 |
| 2026-08-13 | 최신 배포 후보 생성 | FP32와 INT8 2종을 동일 조건에서 비교할 Pi 시험 묶음 생성 |
| 2026-08-13 | INT8 발전 과정 통합 | 선생님 코드 기반 초기 임시본부터 자체 FPN48 최종 후보까지 4개 INT8 모델과 선택·제외 근거 정리 |

세부 수치와 선택 근거는 각 날짜 폴더의 `README.md`에서 확인할 수 있습니다.

## 현재 추천 파일

### 학습을 재현하거나 이어갈 때

- 기본 개발 코드: [`01_model-development/2026-08-12_custom-anchor-free-detector`](01_model-development/2026-08-12_custom-anchor-free-detector/)
- results.4 인계본: [`04_team-integration/2026-08-13_results4-training-handoff`](04_team-integration/2026-08-13_results4-training-handoff/)

### Raspberry Pi에서 시험할 때

- INT8 전체 발전 과정과 비교: [`03_lightweight-deployment/2026-08-13_int8-model-evolution-and-finalists`](03_lightweight-deployment/2026-08-13_int8-model-evolution-and-finalists/)
- 최신 시험 묶음: [`03_lightweight-deployment/2026-08-13_results4-pi-model-variants`](03_lightweight-deployment/2026-08-13_results4-pi-model-variants/)
- 먼저 `README.md`와 `TEST_CHECKLIST_KO.txt`를 읽고 세 모델을 같은 조건으로 비교합니다.
- 현재 정확도 기준 1순위는 FP32입니다. INT8은 Raspberry Pi에서 실제 속도 이득이 정확도 하락을 보상할 때만 선택합니다.

## 다른 팀이 볼 자료

- 캘리브레이션 팀: bbox 원본 좌표, bottom-center, timestamp, letterbox 복원 규약은 [`04_team-integration/2026-08-12_calibration-interface-guide`](04_team-integration/2026-08-12_calibration-interface-guide/) 참고
- 모델을 이어서 학습할 팀원: [`README_CONTINUE_TRAINING_KO.md`](04_team-integration/2026-08-13_results4-training-handoff/README_CONTINUE_TRAINING_KO.md) 참고
- 로봇 제어 팀: AI는 모터를 직접 제어하지 않고 사람 감지 결과와 시각 정보를 제공하는 역할

## 저장소 운영 규칙

앞으로 AI 자료를 업로드할 때는 다음 원칙을 유지합니다.

1. 기능에 맞는 `01`~`04` 파트 아래에 넣습니다.
2. 새 작업 폴더 이름은 `YYYY-MM-DD_작업명`으로 만듭니다.
3. 각 작업 폴더에 목적·입력·결과·실행법·주의사항을 담은 `README.md`를 둡니다.
4. 모델 파일에는 형식과 정밀도를 이름에 표시합니다. 예: `fp32.onnx`, `int8_qdq_percentile.onnx`.
5. 실험 결과는 설정, 최고 epoch, 핵심 지표, 중단 원인, 다음 결정을 함께 기록합니다.
6. 새 자료를 올릴 때마다 이 최상위 README의 **현재 기준 모델**, **빠른 길찾기**, **날짜별 진행 기록**, **마지막 정리 날짜**를 갱신합니다.
7. 데이터셋 원본과 대용량 임시 결과는 올리지 않으며, 재현에 필요한 설정과 대표 결과만 보관합니다.

## 주의사항

- `legacy` 폴더는 현재 모델과 구조가 다르므로 새 학습이나 배포의 출발점으로 사용하지 않습니다.
- GitHub에 데이터셋은 포함하지 않습니다. 학습에는 별도의 `data/processed/v1_grouped`가 필요합니다.
- 최종 후보가 결정되기 전까지 Test split을 반복 사용하지 않습니다.
- self-contained 팀 전달본에는 재현 편의를 위해 일부 학습 코드가 중복 포함되어 있습니다.
