# Person Detector v2 INT8 ONNX

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
