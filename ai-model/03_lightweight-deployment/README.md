# 03. 경량화 및 Raspberry Pi 배포

PyTorch 모델을 ONNX/INT8로 변환하고 실제 카메라와 Raspberry Pi에서
정확도·지연시간·FPS·발열·False Positive를 검증하는 파트입니다.

모든 신규 작업은 `YYYY-MM-DD_작업명` 형식으로 관리합니다.

## 날짜별 기록

| 날짜 | 폴더 | 상태 | 설명 |
|---|---|---|---|
| 2026-08-07 | [`2026-08-07_legacy-int8-onnx`](2026-08-07_legacy-int8-onnx/) | Legacy | 초기 Person Detector v2 ONNX/INT8 변환 |
| 2026-08-12 | [`2026-08-12_preliminary-calibration-handoff`](2026-08-12_preliminary-calibration-handoff/) | Legacy / 인계 | 최종 모델 전 캘리브레이션 팀 연결 확인용 임시 FP32/INT8 |
| 2026-08-12 | [`2026-08-12_raspberry-pi-first-benchmark`](2026-08-12_raspberry-pi-first-benchmark/) | 완료 | OV5647 + Raspberry Pi 1차 실측, 스레드 및 발열 분석 |
| 2026-08-13 | [`2026-08-13_int8-model-evolution-and-finalists`](2026-08-13_int8-model-evolution-and-finalists/) | 완료 | INT8 모델 발전 과정과 후보 비교 |
| 2026-08-13 | [`2026-08-13_results4-pi-model-variants`](2026-08-13_results4-pi-model-variants/) | 기존 Pi 기준 | results.4 FP32 / INT8 MinMax / Percentile 비교 |
| 2026-08-14 | [`2026-08-14_results14-onnx-export`](2026-08-14_results14-onnx-export/) | 보관 | results.14 FP32 / INT8 ONNX 변환본 |
| 2026-08-16 | [`2026-08-16_results20-camera-validation`](2026-08-16_results20-camera-validation/) | **현재 검증** | results20 FP32/INT8 카메라 비교 및 출력 이상 진단 |

## 현재 진행 위치

```text
Custom Anchor-Free Detector
        ↓
Training Experiments
        ↓
FP32 ONNX
        ↓
INT8 QDQ
        ↓
results20 Camera Validation   ← 현재
        ↓
Raspberry Pi Real-Camera Benchmark
        ↓
Final Deployment Selection
```

## 현재 판단

- 기본 모델 입력: `320×240`
- 정확도 비교 기준: FP32
- INT8 후보: QDQ MinMax / QDQ Percentile
- Raspberry Pi 우선 실행 설정: 2 threads
- 최종 선택은 PC FPS만으로 결정하지 않음

반드시 함께 비교할 항목:

- Precision / Recall / F1 / mAP
- tiny / small person recall
- False Positive
- False Negative
- FP32 ↔ INT8 raw output 차이
- inference latency
- End-to-End FPS
- CPU / RAM
- 발열

## 2026-08-16 최신 판단

results20 노트북 카메라 재검증을 완료했습니다.

핵심 결론:

- 최종 FP32 기준 checkpoint: `home_final_s1_continue_results14_to100`, epoch 35
- validation mAP50:95: `0.2607381170`
- PyTorch `best.pt` ↔ FP32 ONNX raw output 일치 확인 완료
- 기존 `FP32 0 detection` 결과는 **사람이 없는 장면에서의 정상 결과**였음
- 같은 무인 장면에서 INT8 MinMax/Percentile은 False Positive가 발생
- 사람 포함 300프레임에서 Percentile의 FP32 보존율은 87.04%, MinMax는 84.26%
- 두 INT8 모두 FP32보다 extra detection이 많음
- Windows CPU에서는 FP32 약 182.53 FPS, INT8 약 98~101 FPS로 QDQ INT8이 오히려 느렸음
- 이 속도 결과는 Raspberry Pi로 일반화하지 않고 Pi 실측으로 최종 결정

현재 우선순위:

1. **FP32 ONNX를 Primary로 유지**
2. INT8 MinMax / Percentile은 Pi benchmark 후보로 유지
3. Raspberry Pi 4 + OV5647에서 2 threads 기준 실측
4. False Positive, Recall, End-to-End FPS, CPU/RAM, 발열 비교 후 최종 선택

최신 검증:

[`2026-08-16_results20-camera-validation`](2026-08-16_results20-camera-validation/)
