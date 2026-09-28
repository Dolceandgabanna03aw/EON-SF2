# Aoi YUME sustained full-processor scenario

기준: 2026-09-22, checkout `codex/mkii`, 현재 working tree. 테스트 실행 파일
`build-plugin/plugins/rompler/test/rompler_tests` SHA-256
`f66b72badf276f168b906d2a599f2507c667c24c972a77f4fef083d033af638b`.
현재 빌드 VST3 실행 파일 SHA-256은
`05ae66a23f1562972d596cf13b5d2a8e82f5a0c270c10885c7d7a9184ac65990`이다.

## 목적과 범위

기존 `full processor sustained CPU deadline baseline`은 80개 블록의 steady-state
측정이었다. 새 hidden benchmark
`plugins/rompler/test/test_full_processor_cpu.cpp`의
`full processor sustained scenario matrix`는 대표 패치에서 각 조건을 30초씩
렌더한다.

- macOS 15.7.9, `Mac13,2`, logical CPU 20
- `Crystal Legacy.sf2`에서 loop-enabled·audible key가 가장 많은 preset
- 48/96 kHz, 64/128/512 samples, 32/128 target voices
- 각 블록 전체 L/R 샘플의 finite 검사, 최대 peak, active voice 범위
- 4초 주기 note-on, note-off, sustain pedal on/off와 MIDI CC1/CC10/pitch wheel
- 4초 주기의 polyphony 축소와 전체 key range 재입력으로 voice steal 경로 실행
- 250 ms 주기의 host-style APVTS parameter automation
  (voice/bus drive, fold, output trim, chorus, compressor)
- 동일 processor·패치에서 active voice 없는 1초 구간을 no-load 기준으로 측정

deadline은 `blockSize / sampleRate`이고 miss는 timing이 그보다 큰 블록이다. 목표는
0 miss이며, timing은 다른 프로세스의 선점 영향을 받으므로 portable assertion으로
강제하지 않는다. finite·voice 범위는 assertion으로 강제한다.

## 실행

```sh
cmake --build --preset plugin --target rompler_tests
build-plugin/plugins/rompler/test/rompler_tests \
  'full processor sustained scenario matrix'
```

## 결과

12개 조건 모두 30초, 총 60 assertions 통과. `nonfinite=0`, active voice 최대치는
요청한 target 이하였고 모든 조건에서 signal이 존재했다. 표의 p95/p99/max는 µs이며
`missed`는 장시간 scenario, `no-load-missed`는 같은 rate/block에서 active voice가
없는 1초 기준이다.

| SR | Block | Voices | No-load p95/p99/max | No-load missed | Scenario p95/p99/max | Deadline | Scenario missed |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 48k | 64 | 32 | 15/44/79 | 0/750 | 204/253/438 | 1333 | 0/22500 |
| 48k | 64 | 128 | 16/29/68 | 0/750 | 771/862/3807 | 1333 | 41/22500 |
| 48k | 128 | 32 | 34/61/94 | 0/375 | 406/464/652 | 2667 | 0/11250 |
| 48k | 128 | 128 | 28/37/96 | 0/375 | 1512/1670/5100 | 2667 | 16/11250 |
| 48k | 512 | 32 | 115/170/183 | 0/94 | 1694/1771/1898 | 10667 | 0/2813 |
| 48k | 512 | 128 | 132/160/169 | 0/94 | 6368/6591/7925 | 10667 | 0/2813 |
| 96k | 64 | 32 | 16/40/90 | 0/1500 | 203/255/1152 | 667 | 3/45000 |
| 96k | 64 | 128 | 17/35/110 | 0/1500 | 773/864/3834 | 667 | 9809/45000 |
| 96k | 128 | 32 | 29/58/102 | 0/750 | 400/473/3402 | 1333 | 26/22500 |
| 96k | 128 | 128 | 28/64/93 | 0/750 | 1502/1605/3499 | 1333 | 4816/22500 |
| 96k | 512 | 32 | 112/157/192 | 0/188 | 1675/1758/2134 | 5333 | 0/5625 |
| 96k | 512 | 128 | 118/145/150 | 0/188 | 6711/7399/9446 | 5333 | 1551/5625 |

## 판정

유한성·voice 범위·무부하 기준은 통과했다. 하지만 P3의 0-miss 목표는 자동화와
voice-steal 이벤트를 포함한 128-voice 단블록 조건에서 통과하지 못했다. 특히
96 kHz/64/128은 9,809/45,000 miss, 96 kHz/128/128은 4,816/22,500 miss였고,
48 kHz에서도 128 voices의 64/128 블록에 각각 41/22,500, 16/11,250 miss가
있었다. 48 kHz/512는 miss가 없었지만 96 kHz/512/128은 1,551/5,625
miss를 기록했다.

이 결과는 no-load 문제가 아니다. no-load는 전 조건 0 miss였고 p95도 16~132 µs
범위였다. 또한 기존 80블록 steady-state baseline은 별도 설정(1x/4x·drive 0/12,
자동화 없음)에서 0/80이므로, 이번 결과는 128 voices와 이벤트·parameter
automation을 함께 유지하는 실제 시나리오의 CPU headroom 부족을 드러낸다.

## 다음 조치

P3는 아직 닫지 않는다. 다음 구현 후보는 (1) 96k/64와 96k/128에서 voice-rate
비용을 줄이는 경로, (2) automation/event가 있는 블록의 bounded work 확인,
(3) 필요하면 기본 polyphony 또는 quality mode 정책을 level-matched listening과
함께 정하는 것이다. 최적화 후에는 같은 12개 matrix를 동일 머신에서 재실행하고
0-miss 여부를 비교한다. pluginval, Ableton/REAPER 로드·리콜, 청취는 이 문서의
CPU 결과로 대체하지 않는다.

## 검증 명령 결과

- `cmake --build --preset plugin`: 통과, plugin 산출물 변경 없음
- `ctest --preset plugin --output-on-failure -E pluginval`: **166/166 통과**
- `cmake --build --preset dev && ctest --preset dev --output-on-failure`: **88/88 통과**
- 장시간 hidden benchmark: **60 assertions 통과**, 위 표의 deadline miss는 별도
  실패 조건으로 기록
- `pluginval_vst3_strictness10`: 이 턴에서는 재실행하지 않았다. 같은 VST3 실행 파일
  SHA-256에 대해 앞선 P1 게이트가 통과한 기록이 있으며, 장시간 CPU 결과가
  pluginval·DAW 로드·청취를 대체하지 않는다.

## 후속 CPU 최적화 재측정 (2026-09-28)

환경은 macOS 15.7.9 (24G830), Mac13,2, arm64, logical CPU 20이다. checkout은
`codex/mkii`이며 기존 dirty WIP를 보존한 상태에서 다음 CPU 경로만 추가로 수정했다.
plugin 프리셋의 `rompler_tests` 실행 파일 SHA-256은
`48071aad7c7fb9941f97f407d56730d03b4378f0e686a5d1b2968cd8b4ada69d`다. 이 해시는
테스트 실행 파일의 것이며 VST3/AU 산출물 해시가 아니다.

최적화는 vibrato interpolation 좌표의 불필요한 per-sample `log2` 제거, 안정된
Drive 값의 blend/gain 캐시, 변경되지 않은 SF2 필터 계수의 블록 간 캐시,
단일 rate bracket 경로, vibrato rate 계산의 `exp2` 전환, 48-tap 보간의 네 누산기
unroll을 포함한다. 장시간 테스트는 miss가 MIDI 이벤트 블록인지, 자동화 변경 블록인지,
일반 블록인지와 miss 당시 활성 보이스 수를 출력하도록 계측했다.

동일 12조건 matrix를 다시 실행한 결과다. 각 행은 30초이며 timing 단위는 µs다.

| SR | Block | Voices | p95 / p99 / max | Deadline | 이전 miss | 현재 miss |
|---:|---:|---:|---:|---:|---:|---:|
| 48k | 64 | 32 | 146.38 / 171.96 / 403.00 | 1333.33 | 0/22500 | 0/22500 |
| 48k | 64 | 128 | 555.88 / 616.75 / 933.38 | 1333.33 | 41/22500 | 0/22500 |
| 48k | 128 | 32 | 287.62 / 325.08 / 420.21 | 2666.67 | 0/11250 | 0/11250 |
| 48k | 128 | 128 | 1043.21 / 1130.54 / 1501.83 | 2666.67 | 16/11250 | 0/11250 |
| 48k | 512 | 32 | 1246.25 / 1326.12 / 1666.58 | 10666.67 | 0/2813 | 0/2813 |
| 48k | 512 | 128 | 4676.50 / 4817.50 / 5298.67 | 10666.67 | 0/2813 | 0/2813 |
| 96k | 64 | 32 | 150.88 / 177.54 / 287.29 | 666.67 | 3/45000 | 0/45000 |
| 96k | 64 | 128 | 530.42 / 609.00 / 1072.79 | 666.67 | 9809/45000 | 83/45000 |
| 96k | 128 | 32 | 281.42 / 321.71 / 539.21 | 1333.33 | 26/22500 | 0/22500 |
| 96k | 128 | 128 | 1036.88 / 1123.42 / 1298.96 | 1333.33 | 4816/22500 | 0/22500 |
| 96k | 512 | 32 | 1252.96 / 1337.88 / 1528.25 | 5333.33 | 0/5625 | 0/5625 |
| 96k | 512 | 128 | 4626.75 / 4733.33 / 5606.29 | 5333.33 | 1551/5625 | 1/5625 |

finite 검사와 voice 범위를 포함해 **60 assertions 통과**, 모든 no-load 조건에서
deadline miss 0, 모든 시나리오에서 `nonfinite=0`이었다. 12조건 전체의 p99는 deadline
미만이다. miss는 기존 총 16,262회에서 84회로 줄었다(약 99.5%). 남은 84회는
96k/64/128에서 83회, 96k/512/128에서 1회였고, 모두 `activeVoices == targetVoices`인
일반 렌더 블록이었다. MIDI 이벤트 블록과 자동화 변경 블록에서는 miss가 없었다. 이
분류는 초과의 블록 종류와 보이스 수를 확인하지만, 실제 full-load 계산과 OS 선점 중
어느 쪽이 각 timing outlier를 만들었는지는 단독으로 확정하지 않는다.

별도 VoicePool baseline의 96k/64/128 voices/12 dB drive 조건은 p95 567.29 µs,
max 616.75 µs, deadline 666.67 µs, 0/80 misses였다. 이 벤치마크도 48 assertions
통과했다. 따라서 96k 작은 블록의 128-voice CPU 여유가 여전히 좁다는 진단과 일치한다.

최종 DSP 회귀 검사 `[dsp]`는 46 cases / 10,423 assertions, `[quality]`는 9 cases /
25,008 assertions 통과했다. `git diff --check`도 통과했다. 0-miss 목표는 아직
충족하지 않아 P3는 열어 둔다. 이 CPU 결과는 pluginval, VST3/AU 형식 빌드, DAW 로드,
실제 청취의 증거를 대신하지 않는다.
