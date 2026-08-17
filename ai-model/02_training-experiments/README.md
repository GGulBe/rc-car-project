# 02. 학습 실험

날짜별 학습 설정, 비교 결과, 선택·중단 근거를 보관합니다. 단순 체크포인트 모음이 아니라 다음 실험을 결정한 이유를 찾는 곳입니다.

| 날짜 | 실험 | 핵심 결론 |
|---|---|---|
| 2026-08-13 | [`2026-08-13_round1-six-laptop-study`](2026-08-13_round1-six-laptop-study/) | 6대 노트북 비교에서 FPN48 results.4가 mAP·F1·small recall 종합 1위 |
| 2026-08-14 | [`2026-08-14_round2-seven-run-analysis-and-round3-search`](2026-08-14_round2-seven-run-analysis-and-round3-search/) | Round 2 7개를 비교해 FPN48 중심으로 축소하고, 재현성·localization·small-person 성능을 확인할 Round 3 6개 실행 |
| 2026-08-14 | [`2026-08-14_round3-30epoch-screening-and-longrun-plan`](2026-08-14_round3-30epoch-screening-and-longrun-plan/) | Round 3 6개를 28~32 epoch에서 비교해 FPN48/exp2.0/box2.0/radius1.5를 확정하고, 연휴 장시간 6대 배치를 생성 |
| 2026-08-14 | [`2026-08-14_home-final-performance-search`](2026-08-14_home-final-performance-search/) | results.7 epoch 87 추세를 종료 판단하고 results.14에서 3단계 resume·미세조정으로 월요일 최종 후보를 자동 선택 |
| 2026-08-17 | [`2026-08-17_r22-r24-screening`](2026-08-17_r22-r24-screening/) | R22(WD 0.0002), R23(direct 384x288), PC방 direct 480x360을 비교해 모두 중단. 320x240 champion 0.260738을 유지하고 다음 홈 실험은 quality loss 1.5 단일 변수 탐색 |

새 실험을 추가할 때는 `README.md`, 설정 파일, 집계 CSV, 재현 가능한 분석 스크립트를 함께 보관합니다.
