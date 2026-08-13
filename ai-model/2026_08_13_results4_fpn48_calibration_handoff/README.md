# Results.4 FPN48 모델 및 학습 코드 전달

작성일: 2026-08-13

대상: 캘리브레이션 팀 확인용

이 폴더는 1차 병렬 실험 6개 중 가장 좋은 결과를 낸 `results.4`의 최고 checkpoint와, 그 checkpoint를 생성한 학습 코드·설정을 별도로 정리한 전달본입니다.

## 핵심 주의사항

- 이 모델은 **학습 중간 후보 모델**이며 최종 배포 모델이 아닙니다.
- `best.pt`는 F1이 아니라 **grouped Valid의 mAP50:95가 최고였던 epoch**를 저장한 파일입니다.
- Test split은 아직 사용하지 않았습니다.
- `.pt`는 PyTorch 학습 checkpoint입니다. Raspberry Pi 또는 C++ ONNX Runtime에 직접 넣는 배포 파일이 아닙니다.
- 캘리브레이션 파이프라인에 실제로 연결할 때는 최종 후보를 FP32 ONNX로 export한 뒤 동일한 letterbox·decode·NMS·좌표 복원 규칙을 사용해야 합니다.

## 최고 결과

`best.pt` 저장 epoch: **37**

고정 score threshold `0.25` 기준:

| 지표 | 결과 |
|---|---:|
| mAP50:95 | **0.252478** |
| AP50 | 0.553034 |
| AP75 | 0.193246 |
| Precision | 0.585655 |
| Recall | 0.557562 |
| F1 | 0.571263 |
| Tiny recall (`<16 px`) | 0.072936 |
| Small recall (`16~32 px`) | 0.529412 |
| Medium recall (`32~96 px`) | 0.690264 |
| Large recall (`>=96 px`) | 0.813253 |

Validation에서 F1을 가장 높인 threshold는 약 `0.282715`였고 그때의 F1은 `0.585554`였습니다. 단, 실제 카메라 운영 threshold는 최종 모델과 실제 환경 데이터로 다시 조정해야 합니다.

## 모델 설정

| 항목 | 값 |
|---|---:|
| 입력 크기 | 320×240 |
| 클래스 | person 1개 |
| 구조 | DSConv + Residual backbone + FPN + anchor-free head |
| FPN channels | 48 |
| Backbone expansion | 2.0 |
| 파라미터 수 | 323,546 |
| 학습률 | 0.001 |
| Batch size | 8 |
| Box loss weight | 2.0 |
| Center sampling radius | 1.5 |
| AMP | 사용 |

학습 장비는 NVIDIA GeForce MX570 A 4GB, PyTorch 2.3.1, CUDA 12.1입니다. 100 epoch 설정이었으나 외부 실행 중단으로 48 epoch까지 완료됐습니다. 최고점은 epoch 37에 이미 발생했고 checkpoint는 정상 저장됐습니다.

## 폴더 구성

```text
results4_fpn48_calibration_handoff/
├─ README.md
├─ SHA256SUMS.txt
├─ model/
│  └─ results4_fpn48_best.pt
├─ results/
│  ├─ config.json
│  ├─ device.json
│  ├─ history.csv
│  └─ warnings.log
└─ training_code/
   ├─ environment-school.yml
   ├─ pyproject.toml
   ├─ configs/
   │  └─ school2_lightweight_100e.json
   ├─ scripts/
   │  └─ 16_train.py
   └─ src/rc_detector/
      ├─ backbone.py
      ├─ fpn.py
      ├─ head.py
      ├─ model.py
      ├─ assignment.py
      ├─ losses.py
      ├─ dataset.py
      ├─ inference.py
      ├─ metrics.py
      ├─ training.py
      └─ __init__.py
```

## Checkpoint 확인

프로젝트 루트에서 다음처럼 구조와 설정을 확인할 수 있습니다.

```python
import sys
from pathlib import Path

import torch

root = Path(".").resolve()
sys.path.insert(0, str(root / "training_code" / "src"))

from rc_detector.model import PersonDetector

checkpoint = torch.load(
    root / "model" / "results4_fpn48_best.pt",
    map_location="cpu",
    weights_only=False,
)
config = checkpoint["config"]

model = PersonDetector(
    fpn_channels=int(config["fpn_channels"]),
    backbone_expansion=float(config["backbone_expansion"]),
)
model.load_state_dict(checkpoint["model"])
model.eval()

print("saved epoch:", checkpoint["epoch"])
print("best mAP50:95:", checkpoint["best_map50_95"])
print("input:", config["image_width"], config["image_height"])
```

## 학습 재현 조건

학습 코드만으로는 재학습할 수 없으며 원본 프로젝트와 같은 group-aware 데이터셋이 다음 경로에 필요합니다.

```text
data/processed/v1_grouped/
├─ train/images
├─ train/labels
├─ valid/images
└─ valid/labels
```

전체 프로젝트 구조로 배치했을 때 실행 명령은 다음과 같습니다.

```powershell
python -m pip install -e .
python scripts\16_train.py --config configs\experiments\school2_lightweight_100e.json
```

데이터셋은 크기와 라이선스·중복 문제 때문에 이 전달 폴더와 GitHub에 포함하지 않습니다.

## 캘리브레이션 팀이 참고할 출력 규약

현재 checkpoint가 직접 제공하는 것은 학습된 anchor-free detector 가중치입니다. 최종 통합 시 AI 팀은 다음 정보를 생성해야 합니다.

- 원본 프레임의 `frame_id`, `capture_timestamp`
- 원본 640×480 기준 bbox `(x1, y1, x2, y2)`
- confidence
- bbox 하단 중심점 `(foot_x, foot_y)`
- letterbox scale과 padding을 제거한 원본 좌표
- 사용 모델 SHA-256과 score/NMS threshold

캘리브레이션 팀은 bbox 하단 중심점을 무조건 실제 발 위치로 확정하면 안 됩니다. bbox가 프레임 아래·위·좌우 경계에 닿는 경우 부분 인물일 수 있으므로 `position_valid=false` 또는 별도의 잘림 상태를 사용해야 합니다.

정확한 실제 거리·지도 좌표 계산에는 이 모델 외에도 카메라 내부 행렬, 왜곡 계수, 설치 높이·자세, 프레임 촬영 시각과 대응되는 GPS·IMU·pan/tilt 값이 필요합니다.
