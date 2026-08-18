# RC-Car AI 개발 및 경량화

Raspberry Pi 기반 RC카에서 사람을 실시간 감지하기 위한 AI 파트 저장소입니다. 모델 설계, 학습 실험, ONNX/INT8 경량화, Raspberry Pi 검증, 팀 연동 자료를 단계별로 관리합니다.

> **현재 Valid 최고 checkpoint:** `results27 / longrun_s5_stage1_640_seed11 / epoch 52`
>
> **현재 최고 수치:** validation mAP50:95 `0.288050`, AP50 `0.579422`, AP75 `0.253529`
>
> **기존 Raspberry Pi 통합 기준 모델:** `results20_fpn48_int8_qdq_percentile.onnx`
>
> **Pi 통합 실측:** 전체 기능 동시 실행 상태에서 약 `21~27 FPS`, 현재 과부하 없이 동작
>
> **새 배포 후보:** `results27_640_int8_round4_q3_q4_suffix50.onnx`
>
> **최종 Test split:** 아직 최종 모델 확정 전에는 사용하지 않음

마지막 정리: **2026-08-18**

---

## 빠른 길찾기

| 찾는 내용 | 위치 |
|---|---|
| 모델 구조와 개발 코드 | `01_model-development/` |
| 학습 실험 | `02_training-experiments/` |
| R25~R31 / 640×480 champion 분석 | `02_training-experiments/2026-08-18_r25-r31-and-longrun-study/` |
| ONNX / INT8 / Raspberry Pi 배포 | `03_lightweight-deployment/` |
| results27 selective INT8 Round 1~5 | `03_lightweight-deployment/2026-08-18_results27-selective-int8/` |
| results20 카메라 검증 | `03_lightweight-deployment/2026-08-16_results20-camera-validation/` |
| 캘리브레이션 및 팀 연동 | `04_team-integration/` |

---

## 현재 모델 구조 방향

프로젝트 요구에 맞춘 custom anchor-free person detector를 사용한다.

```text
RGB input
  → DSConv + Residual lightweight backbone
  → lightweight FPN
  → anchor-free detection head
  → classification + quality + LTRB box regression
  → decode + NMS
```

평가 기준:

- validation mAP50:95
- AP50 / AP75
- Precision / Recall / F1
- 작은 사람 및 먼 사람 검출
- Raspberry Pi inference latency
- end-to-end FPS
- CPU / RAM / 온도
- 운동장 실제 FP / FN

최종 모델은 mAP 하나가 아니라 **정확도와 Raspberry Pi 실시간성의 균형**으로 결정한다.

---

## 현재 Accuracy Champion — results27

```text
run: longrun_s5_stage1_640_seed11
input: 640×480
seed: 20260811
best epoch: 52

mAP50:95: 0.2880503189
AP50: 0.579422
AP75: 0.253529
Precision: 0.828927
Recall: 0.381651
F1: 0.522661
```

640×480 seed14도 mAP50:95 `0.277340`을 기록해 고해상도 개선이 다른 seed에서도 재현됐다.

320×240 장기 실험은 대체로 약 0.24~0.25 범위였다.

세부 결과:

`02_training-experiments/2026-08-18_r25-r31-and-longrun-study/README.md`

---

## Results27 경량화 결론

일반적인 full INT8 / Conv INT8 PTQ는 출력 보존성이 부족했다.

Round 1~3에서 일반 PTQ와 broad selective quantization을 시험한 뒤, Round 4에서 backbone을 Q1~Q4로 나누어 sensitivity를 분석했다.

핵심 발견:

- Q1 early backbone: PTQ에 매우 민감
- Q2~Q4: 상대적으로 안전
- Q3+Q4 selective INT8: FP32 출력 보존성이 가장 안정적인 후보 중 하나

현재 results27 selective INT8 1순위:

`results27_640_int8_round4_q3_q4_suffix50.onnx`

구성:

```text
early backbone : FP32
Q2             : FP32
Q3 + Q4        : INT8
FPN            : FP32
head           : FP32
```

파일 크기는 FP32 약 1.39 MB에서 mixed INT8 약 0.76 MB로 감소했다.

세부 Round 1~5 수치:

`03_lightweight-deployment/2026-08-18_results27-selective-int8/README.md`

---

## Raspberry Pi 현재 상태

현재 Raspberry Pi 4 전체 시스템 통합 시험에서 실제 확인된 모델은:

`results20_fpn48_int8_qdq_percentile.onnx`

카메라, 사람 탐지, RC카 관련 기능을 동시에 실행한 상태에서 약 **21~27 FPS**가 확인됐다.

현재 관찰:

- 시스템 과부하 없음
- 다른 기능과 동시 실행 가능
- 사람 검출은 실용 가능한 수준
- 일부 FP/FN 존재

이 결과 때문에 현재 최적화 목표는 **FPS 극대화**보다 **충분한 실시간성을 유지하면서 정확도를 높이는 것**으로 이동한다.

results27 FP32와 results27 mixed INT8는 Raspberry Pi에서 별도 실측 후 기존 results20 INT8와 비교한다.

---

## 2026-08-18 추가 해상도 탐색 — 진행 중

학교 노트북 6대에서 다음 해상도 실험을 병렬 수행 중이다.

- 512×384 seed11
- 576×432 seed11
- 576×432 seed14
- 640×480 seed15
- 704×528 seed11
- 768×576 seed11

오후 기준 epoch 약 17~25 구간이며 일부 실행에서 mAP50:95 약 0.3 수준의 중간 결과가 관찰됐다.

**진행 중 수치이므로 최종 결과로 확정하지 않는다.**

완료 후 결과를 별도 커밋한다.

---

## 운동장 검증 및 Hard Negative 계획

운동장 전체 촬영에서 반복적으로 특정 구조물을 사람으로 인식하는 경우 hard-negative 학습을 검토한다.

우선순위:

1. 여러 프레임에서 반복되는 FP
2. 여러 각도/거리에서도 반복되는 FP
3. 실제 주행 경로에서 자주 등장하는 물체

예:

- 골대
- 나무
- 기둥
- 표지판
- 가방
- 운동장 구조물

사람이 포함된 이미지를 annotation 없이 negative로 사용하지 않는다.

---

## 저장소 구조

```text
ai-model/
├─ 01_model-development/       모델 구조, 데이터 분할, 학습 코드
├─ 02_training-experiments/    날짜별 학습 결과와 비교 분석
├─ 03_lightweight-deployment/  ONNX, INT8, Raspberry Pi 시험
└─ 04_team-integration/        캘리브레이션 팀 및 다른 파트 인계 자료
```

새 작업 폴더는 `YYYY-MM-DD_작업명` 형태를 유지한다.

---

## 저장 원칙

1. 각 실험 폴더에는 목적, 설정, 결과, 결론을 기록한 README를 둔다.
2. 학습 실험은 `best.pt`, `config.json`, `device.json`, `history.csv`, `training_status.json`을 우선 보존한다.
3. 대용량 원본 ZIP과 불필요한 `last.pt` 중복은 Git에 올리지 않는다.
4. 배포 모델은 파일명에 precision / quantization 방식을 표시한다.
5. 실패한 경량화 모델도 선택 근거를 재현할 가치가 있으면 결과와 함께 보존한다.
6. 최종 배포 선택은 Pi 실측 FPS와 실제 운동장 FP/FN까지 확인한 뒤 확정한다.
