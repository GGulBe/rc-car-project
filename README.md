# RC-Car Project — AI Model Branch

이 브랜치에서 AI 개발 및 경량화 자료는 [`ai-model/`](ai-model/) 아래에 정리되어 있습니다.

## AI 자료 바로가기

- 전체 안내와 현재 진행 상태: [`ai-model/README.md`](ai-model/README.md)
- 모델 개발 코드: [`ai-model/01_model-development`](ai-model/01_model-development/)
- 학습 실험과 비교 결과: [`ai-model/02_training-experiments`](ai-model/02_training-experiments/)
- 2026-08-14 Round 2 분석과 Round 3 최종 성능 탐색: [`ai-model/02_training-experiments/2026-08-14_round2-seven-run-analysis-and-round3-search`](ai-model/02_training-experiments/2026-08-14_round2-seven-run-analysis-and-round3-search/)
- 경량화와 Raspberry Pi 배포: [`ai-model/03_lightweight-deployment`](ai-model/03_lightweight-deployment/)
- 날짜별 INT8 모델 발전·성능 비교: [`ai-model/03_lightweight-deployment/2026-08-13_int8-model-evolution-and-finalists`](ai-model/03_lightweight-deployment/2026-08-13_int8-model-evolution-and-finalists/)
- 캘리브레이션 팀 연동과 인계본: [`ai-model/04_team-integration`](ai-model/04_team-integration/)

현재 AI 기준 모델은 2026-08-13 1차 병렬 실험의 `results.4` FPN48입니다. 2026-08-14 Round 2 최고 모델도 이 기준을 넘지 못해, FPN48 재현성·backbone capacity·박스 정밀도·작은 사람 Recall을 검증하는 Round 3 병렬 실험을 진행하고 있습니다. 상세 지표와 최신 실행 방법은 `ai-model/README.md`에서 확인하세요.

`rc_car_cpp/`는 로봇 제어 코드 영역이며 이번 AI 폴더 정리에서는 수정하지 않았습니다.
