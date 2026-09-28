# Aoi YUME 현재 품질 리뷰와 개선 계획

기준: 2026-09-22, `codex/mkii`, HEAD `eb6766f66caa39a96706716ebcd3fcd4c32ba111` + 현재 미커밋 변경.
초기 문장은 2026-09-22 리뷰 시점의 상태를 보존한 기록이다. 2026-09-23 후속 실행 결과는 문서 하단의 진행 상태에 추가했다.

## 현재 상태와 증거

- 기존 출력 보호: FX/출력 trim 뒤 0.97 knee, 0.985 sample ceiling. 기존 stage-2 기록은 159/159 및 EONQC 24개 시나리오 통과를 보고한다. 이는 true-peak 보호나 청취 품질 보증이 아니다.
- 미커밋 작업: UI 스킨, 보이스별 스테레오 합성, CPU 및 스테레오 테스트. 기존 추적 파일 8개 변경과 신규 파일 4개가 있어 현재 결과를 HEAD만으로 재현할 수 없다.
- 스테레오 기록: dev 88/88, plugin 162/162(pluginval 제외), strictness 10 별도 통과가 보고되어 있다. 현재 LastTest.log는 이전 실행 자료이며 전체 검증을 이번 리뷰에서 재실행하지 않았다.
- 현재 build-plugin VST3 실행 파일 SHA256을 직접 확인했다: `8c9344ac9978cf0ce87809e05a280a9049c13b7e16fb789a2090e8ca1b6c0353`. 스테레오 문서와 일치한다. 이것만으로 현재 모든 소스 변경이 바이너리에 반영되었다고 단정하지 않는다.
- Ableton 삽입·MIDI 실행은 기존 문서에 기록되어 있다. 해당 기록에는 정확한 호스트 버전/버퍼가 없으며, REAPER·상태 복원·레벨 매칭 청취 증거는 별도로 필요하다.
- 이번 `git diff --check`는 통과했다.

## 우선 해결할 발견

### 1. 스테레오 음량 기준 불일치 — 우선순위 높음

`plugins/rompler/Source/Sampler.cpp:686-696`: 중앙 슬롯은 L/R gain이 각각 1이고, 나머지는 equal-power gain이다. 같은 신호에 대한 L²+R² 계수는 중앙 2, 나머지 1로 약 3.01 dB 차이가 난다. 실제 음악 출력 차이는 위상·음색·합산에 따라 달라지지만 슬롯마다 다른 음량 기준을 쓰는 점은 코드로 확인된다.

width=0에서는 기존 dual-mono, 작은 양수에서는 비중앙 슬롯이 약 0.707/0.707로 바뀐다. 현재 width는 내부 상수라 사용자 자동화 문제는 아직 아니지만 Width 컨트롤 추가 전 연속성을 고쳐야 한다.

### 2. 좌우 배치 및 연주 이력 의존 — 우선순위 높음

`Sampler.cpp:19-21`: 위치 합은 -0.52로 좌우 대칭이 아니다. 동일 에너지 보이스를 8개 슬롯에 배치할 때 좌측 편향이 생길 구조다. 실제 프리셋에서 편향 크기는 추가 측정이 필요하다.

배치는 활성 음 개수가 아닌 슬롯 인덱스로 정해진다. 따라서 화음에서 비중앙 슬롯의 음 하나만 남는 경우 중앙에 오지 않는다. 문서의 단음 중앙 설명은 새 구절의 첫 슬롯에 해당하며 모든 단음 상황을 보장하지 않는다. 잔향 도중 급격히 재중앙화하는 수정도 피해야 한다.

### 3. CPU 측정 범위가 짧음 — 우선순위 높음

`plugins/rompler/test/test_full_processor_cpu.cpp:157-178`: 80블록은 96k/64에서 약 53 ms, 48k/512에서 약 853 ms다. 노트 입력은 워밍업 첫 블록에서 발생하므로 타이밍 표에는 노트 생성 비용이 포함되지 않는다.

deadline 초과는 출력만 하며 assertion이 아니다. 따라서 테스트 성공과 deadline 준수는 별도로 판독해야 한다. peak의 유한성 검사도 모든 출력 샘플의 NaN/Inf 검사를 대체하지 못한다. 기존 문서의 장시간 CPU 게이트 종결 표현은 측정 범위를 좁혀 수정할 필요가 있다.

## 실행 순서와 완료 기준

| 단계 | 작업 | 완료 기준 |
|---|---|---|
| P0 기준 고정 | 현재 WIP 변경별 목록, 소스/빌드/설치 바이너리 구분, 기존 로그 보존 | 같은 소스로 재빌드할 수 있는 체크포인트와 증거 목록 |
| P1 스테레오 정교화 | 슬롯 간 gain 기준 통일, 대칭 배치, width 연속성, 연주 중 위치 정책 정의 | 같은 톤의 슬롯별 파워 일치, 대칭 fixture L/R 균형, width 0 근방 연속성, mono fold-down·release·steal·가변 블록 테스트, RT 무할당 |
| P2 음색·다이내믹 | 대표 SF2/프리셋의 코드·강한 벨로시티·고음 렌더, safety 개입량 측정, 패치별 gain staging | 일반 연주에서 과도한 ceiling 의존을 줄이고, 기존/개선본의 음량을 맞춘 A/B 승인; true-peak·aliasing·클릭 측정 결과 보존 |
| P3 실제 부하 | 대표 패치별 30–60초 렌더, 48/96k 및 64/128/512 블록, 반복 note-on/off·sustain·steal·자동화 | 실제 voice 수·전체 finite 검사·p95/p99/max·deadline miss 기록; 동일 머신 무부하 기준에서 0 miss 목표, 실패 조건 공개 |
| P4 UI 완성도 | 스킨의 표시값/히트 영역/라벨/키보드 초점, 작은 화면 적합성, preset dirty/Init/복원 점검 | 기본·작은 화면·HiDPI에서 조작 가능, 파라미터와 표시 일치, 저장/복원 일치; 스크린샷과 실제 조작 증거 |
| P5 배포 검증 | dev/plugin 빌드와 CTest, strictness 10, AU 검증, 사용자 Ableton·REAPER 실행/리콜/청취 | 최종 바이너리 SHA256·호스트 버전·SR·버퍼와 각 게이트 결과를 함께 기록 |

P1을 먼저 완료한 다음 P2/P3 결과에 따라 DSP 최적화 필요성을 결정한다. 전체 리팩터링이나 새 FX 추가를 선행하지 않는다. 기존 파라미터 ID/PLUGIN_CODE를 보존한다. 사용자용 Width 추가는 기존 세션 사운드 및 상태 이행 설계를 먼저 정한 뒤 별도 범위로 다룬다. 설치/패키징은 별도 요청 시 수행한다.

### 진행 상태 (2026-09-22)

- P1 완료: 슬롯별 gain 기준을 equal-power + sqrt(2) 정규화로 통일하고, 9개 대칭 레인으로 교체했으며, `stereoGainsForVoice()` 회귀 테스트 4종을 추가했다. 근거와 해시는 `aoi-yume-stereo-lane-policy-2026-09-22.md` 참조. pluginval strictness 10 통과, VST3 SHA256 `05ae66a2...ac65990`.
- P2 측정 완료: 헤드룸 프로브로 기본 설정에서 3음 화음이 safety knee/ceiling에 실제로 닿는 것을 확인했다(6개 중 5개 knee, 2개 ceiling, Flute 화음은 9.7%가 ceiling). knee를 벗어나려면 추가 3~6 dB 감쇠가 필요하다. 게인 스테이징 변경 자체는 레벨 매칭 청취 비교가 필요해 대기한다 — `aoi-yume-output-headroom-probe-2026-09-22.md`.
- 측정 도구 결함 수정: `AudioBuffer::getMagnitude(1, blockSize)`는 채널이 아니라 `(startSample=1)`이라 1샘플 범위 초과 읽기였다. 프로브와 기존 WIP 테스트 2곳(`test_full_processor_cpu.cpp`, `test_sf2_playback.cpp`)을 3-인자/단일 호출로 고쳤다.
- P3 진행: 30초 sustained scenario matrix를 추가해 48/96k × 64/128/512 ×
  32/128 voices에서 note-off/sustain/steal/MIDI·parameter automation과 전체
  finite 검사를 수행했다. 12개 조건의 finite·voice assertion은 통과했지만,
  128 voices 단블록에서 deadline miss가 남아 P3는 미종결이다. 상세 수치는
  `aoi-yume-quality-sustained-scenario-2026-09-22.md`에 기록했다. 기존 UI 스킨
  WIP는 그대로 보존했다.

## 다음 구현 단위

다음 변경은 P3의 96k 단블록/128-voice miss를 원인별로 분리하는 CPU 계측과
bounded-work 최적화로 제한한다. UI WIP와 P2의 gain-staging 결정은 보존한다.
최적화 후 같은 12개 sustained matrix를 재실행해 0-miss 목표와 finite/voice
assertion을 다시 확인한 뒤 P4 UI 점검으로 넘어간다.

### 후속 실행 상태 (2026-09-23)

- P4 UI 리사이즈 구현: `RomplerEditor`를 resizable로 만들고 스킨 디자인 비율 1563:1006의 fixed-aspect constrainer, 960×618~2345×1509 제한, 중앙 letterbox canvas를 적용했다. artwork 좌표와 hit target은 같은 canvas transform을 사용한다. `ui_shot`은 선택적 width/height를 받아 960×618, 2000×1000, 1563×1006 스냅샷을 생성한다.
- P4 자동 검증: 리사이즈 테스트 31 assertions, plugin CTest 167/167, dev CTest 88/88 통과. 2000×1000 스냅샷에서 스킨 비율 유지와 좌우 letterbox를 확인했다.
- P5 현재 증거: pluginval strictness 10 통과(279.12 s), 최신 VST3 SHA256 `47fa14f1cba78bfdd4465754a759d94ef54fb4dbfebab48a6b91ec96d4ed9227`.
- Ableton Live 12.4.5에서 최신 VST3를 검색·삽입하고 파라미터 목록과 Drive 자동화 왕복을 확인했다. 호스트 설정은 44.1 kHz / 256 samples, 검증 중 CPU 표시 3%였다. 플로팅 편집기 자식 창은 CUA 캡처에 노출되지 않아 창 드래그 크기 자체의 Ableton 수치 증거는 미확정이며, headless canvas 증거로 보완했다. 상세 기록은 `aoi-yume-ableton-ui-resize-2026-09-23.md`.
- REAPER 검증은 사용자 지시에 따라 실행하지 않았다. 실제 청취 게이트도 아직 별도 확인이 필요하다.

### P3 CPU 최적화 후속 상태 (2026-09-28)

- 보간 rate 좌표, 고정 Drive gain, 변경 없는 필터 계수, bracket 경계 및 48-tap
  누산을 최적화했다. 12조건 sustained matrix의 deadline miss는 16,262회에서
  84회로 줄었고, 모든 조건 p99는 deadline 이내다.
- 남은 miss 84회는 96k/64/128에서 83회, 96k/512/128에서 1회다. 모두 128개
  보이스가 활성인 일반 렌더 블록이며 MIDI/자동화 변경 블록은 아니다. 무부하,
  finite, voice-count assertions는 통과했지만 P3의 0-miss 목표는 아직 미달이다.
- 상세 표·환경·테스트 실행 파일 해시는
  `aoi-yume-quality-sustained-scenario-2026-09-22.md`의 후속 CPU 최적화 재측정
  절을 참조한다. P3가 미종결이므로 P4 점검으로 넘어가지 않는다.
