# 2026-08-16 results20 Camera Validation

`results20_fpn48` FP32/INT8 모델을 실제 카메라 입력에서 비교·진단하기 위한 패키지입니다.

## 목적

- FP32 / INT8 MinMax / INT8 Percentile 실시간 검출 비교
- 동일 카메라 프레임 기반 모델 비교
- ONNX Runtime 추론 지연시간 측정
- FP32 ↔ INT8 출력 차이 진단
- Raspberry Pi 배포 전 이상 동작 확인

## 구조

```text
2026-08-16_results20-camera-validation/
├─ README.md
├─ requirements.txt
├─ detect_compare.py
├─ diagnose_models.py
├─ models/
│  ├─ results20_fpn48_fp32.onnx
│  ├─ results20_fpn48_int8_qdq_minmax.onnx
│  └─ results20_fpn48_int8_qdq_percentile.onnx
└─ results/
   └─ diagnostics/
      ├─ diagnostic_summary.csv
      └─ diagnostic_pairwise.csv
```

## 설치

```bash
pip install -r requirements.txt
```

## 실시간 카메라 비교

```bash
python detect_compare.py --webcam 0
```

모델 전환:

- `1`: FP32
- `2`: INT8 MinMax
- `3`: INT8 Percentile
- `s`: 화면 저장
- `q`: 종료

## 상세 진단

```bash
python diagnose_models.py --frames 300 --threads 2
```

## 2026-08-16 진단 결과 주의사항

기존 진단에서 FP32와 INT8의 검출 동작이 정상적으로 대응하지 않는 이상 징후가 확인되었습니다.

특히 저장된 진단 결과에서는:

- FP32가 300 프레임에서 검출 0
- INT8 MinMax에서는 검출 발생
- INT8 Percentile에서도 검출 발생

따라서 현재 pairwise 결과만으로
**INT8이 FP32 출력을 잘 보존한다거나 INT8이 더 정확하다고 판단하면 안 됩니다.**

우선 재검증할 항목:

1. FP32/INT8 입력 tensor 동일 여부
2. RGB/BGR 처리
3. resize 또는 letterbox 전처리
4. output tensor 순서와 decode
5. confidence threshold
6. NMS
7. 동일 프레임 raw output 비교

## 현재 프로젝트 단계

```text
Training
→ FP32 ONNX
→ INT8 Quantization
→ Laptop Camera Validation  ← 현재
→ Raspberry Pi Real-Camera Benchmark
→ Final Deployment Selection
```

최종 목표는 PC에서 가장 빠른 모델이 아니라,
**Raspberry Pi 4에서 사람을 안정적으로 검출하면서 RC카의 다른 프로세스를 방해하지 않는 모델**입니다.
