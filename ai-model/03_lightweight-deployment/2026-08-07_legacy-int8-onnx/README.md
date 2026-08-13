# Person Detector v2 INT8 ONNX — Legacy

> 이 폴더는 2026-08-07 초기 v2 모델 변환 이력입니다. 현재 Raspberry Pi 시험은 상위 폴더의 `2026-08-13_results4-pi-model-variants`를 사용하세요.

## Description

Person detection model converted from PyTorch to ONNX
and quantized to INT8 format.

## Model Files

### FP32 Model

- person_detector_v2_best.onnx

Original ONNX model.

### INT8 Model

- person_detector_v2_best_int8.onnx

Quantized INT8 model for lightweight inference.

## Conversion Pipeline


## Test Result

ONNX Runtime inference test completed successfully.

Input:


[batch, 3, 240, 320]


Output:


(1, 5, 5, 30, 40)


## Target Device

- Raspberry Pi
- OpenCV camera detection system
