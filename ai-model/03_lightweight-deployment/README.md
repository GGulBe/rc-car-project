# 03. 경량화 및 Raspberry Pi 배포

PyTorch 모델을 ONNX/INT8로 변환하고 Raspberry Pi 카메라에서 속도·발열·박스를 검증하는 파트입니다.

| 날짜 | 폴더 | 상태 | 설명 |
|---|---|---|---|
| 2026-08-07 | [`2026-08-07_legacy-int8-onnx`](2026-08-07_legacy-int8-onnx/) | Legacy | 초기 v2 모델의 ONNX/INT8 변환 실험 |
| 2026-08-12 | [`2026-08-12_preliminary-calibration-handoff`](2026-08-12_preliminary-calibration-handoff/) | Legacy | 최종 모델 전 캘리브레이션 팀 연결 확인용 임시 FP32/INT8 전달본 |
| 2026-08-12 | [`2026-08-12_raspberry-pi-first-benchmark`](2026-08-12_raspberry-pi-first-benchmark/) | 완료 | OV5647 실시간 카메라, 스레드별 지연시간·발열 실측 |
| 2026-08-13 | [`2026-08-13_results4-pi-model-variants`](2026-08-13_results4-pi-model-variants/) | **현재 배포 시험 기준** | results.4 기반 FP32, INT8 Percentile, INT8 MinMax 비교 묶음 |

## 현재 판단

- 정확도 기준 1순위: FP32 ONNX
- INT8 정확도 우선 후보: QDQ Percentile
- Raspberry Pi 실행 스레드: 우선 2 threads
- 1차 현장 목표: 8 FPS 이상, 평균 sensor-to-result 200 ms 이하, 75°C 미만

최신 시험은 `2026-08-13_results4-pi-model-variants`의 안내를 따릅니다.
