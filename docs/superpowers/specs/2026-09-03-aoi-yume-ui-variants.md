# Aoi YUME UI 구조안 5종

**상태:** 선택 대기 중인 디자인 초안  
**기준:** 2026-09-03, 1120×900 기본 플러그인 창 / 800×600 축소 대응

**비교 목업:** [5안 비교 이미지](../../../outputs/aoi_yume_ui_variants_20260903.png)

**추가 목업 10종:** [안 6–10](../../../outputs/aoi_yume_ui_variants_06-10_20260903.png), [안 11–15](../../../outputs/aoi_yume_ui_variants_11-15_20260903.png)

**색상 목업 30종:** [01–05](../../../outputs/aoi_yume_colorways_01-05_20260903.png), [06–10](../../../outputs/aoi_yume_colorways_06-10_20260903.png), [11–15](../../../outputs/aoi_yume_colorways_11-15_20260903.png), [16–20](../../../outputs/aoi_yume_colorways_16-20_20260903.png), [21–25](../../../outputs/aoi_yume_colorways_21-25_20260903.png), [26–30](../../../outputs/aoi_yume_colorways_26-30_20260903.png)

**리얼리스틱 금속 패널 패스:** [재질 개선 목업](../../../outputs/aoi_yume_realistic_metal_panel_20260903.png)

## 이해 요약

- Aoi YUME의 현재 DSP와 파라미터 ID는 유지한다.
- Voice/Bus/Comp/Envelope/FX를 반드시 현재 위치에 둘 필요는 없다.
- 실제 하드웨어 신스의 조작 철학을 반영해 서로 다른 UI 계층 5개를 비교한다.
- 핵심 성공 기준은 프리셋 식별, 핵심 음색 조절, 출력 레벨 확인, 작은 창에서의 가독성이다.
- 이번 문서는 구조 선택용이며, 선택 전에는 소스 코드를 수정하지 않는다.

## 공통 제약과 가정

- 모든 안은 JUCE의 현재 knob, preset browser, keyboard, pitch/mod 컨트롤을 재사용할 수 있어야 한다.
- `Bank / Program / Source / Dirty`와 master output은 어느 페이지에서도 잃지 않는다.
- 색은 mint=status, amber=drive/warning, ivory=label/key로 제한한다.
- 실제로 동작하지 않는 장식용 knob·meter·LED는 만들지 않는다.

## 안 1 — Signal Rail Console (추천)

Prophet-6의 즉시 조작과 JUPITER-X의 Scene/part 계층을 결합한다. 상단에 `VOICE → BUS → COMP → ENV → FX → OUT`을 항상 표시하고, 왼쪽은 28% preset browser, 오른쪽은 선택한 모듈의 상세 편집으로 사용한다. 하단에는 SoundFont·pitch/mod·keyboard를 고정한다.

```text
┌ Aoi YUME | BANK 00 | PROGRAM 003 | USER • DIRTY       OUT ▂▅▇ ┐
│ VOICE ─── BUS ─── COMP ─── ENV ─── FX ─── OUTPUT           │
├──────────────┬────────────────────────────────────────────┤
│ PRESETS      │ [VOICE]  DRIVE  FILTER  POLY  LEGATO        │
│ 00-000 Piano │          핵심 knob + numeric readout        │
│ 00-001 Pad   │ [BUS/COMP/ENV/FX] 선택 모듈 상세            │
├──────────────┴────────────────────────────────────────────┤
│ SOUNDFONT | PITCH | MOD                         KEYBOARD   │
└───────────────────────────────────────────────────────────┘
```

**장점:** 현재 UI와 가장 자연스럽게 이어지고, 신호 경로와 출력 문제를 한눈에 확인한다.  
**단점:** 모듈 전환이 잦은 사용자는 클릭이 한 단계 늘어난다.  
**구현 난이도:** 중간. 현재 컴포넌트를 탭/선택형 컨테이너로 재배치하면 된다.

## 안 2 — Modular Patch Bay

Matriarch처럼 신호 흐름을 좌→우로 크게 펼친다. 각 모듈은 독립 faceplate이고, 모듈 사이의 선은 실제 라우팅과 bypass 상태를 나타낸다. Preset browser는 접히는 왼쪽 drawer로 두어 작업 중에는 패널 폭을 확보한다.

```text
┌ Aoi YUME | PRESET 003 | MASTER/PEAK ─────────────────────┐
│ [VOICE] ───▶ [BUS] ───▶ [COMP] ───▶ [ENV] ───▶ [FX] ─▶ OUT │
│  knobs        knobs        GR meter     ADSR        FX mix │
│  ┌────────────── expanded module / routing inspector ───┐ │
│  │ selected source → destination, bypass, value history  │ │
├──┴────────────────────────────────────────────────────────┤
│ PITCH / MOD                 SOUNDFONT             KEYBOARD │
└───────────────────────────────────────────────────────────┘
```

**장점:** “소리가 어디로 흐르는가”가 가장 명확하고 제품 개성이 강하다.  
**단점:** 1120px 이하에서 노브가 작아지며, 실제 라우팅이 단순한 현재 엔진에는 과할 수 있다.  
**구현 난이도:** 높음. signal graph, 연결선, 모듈 focus 상태가 필요하다.

## 안 3 — Morph Matrix Performance

PolyBrute의 Morph/Matrix를 축소해, 화면 중앙의 4×4 macro pad가 여러 파라미터를 동시에 움직인다. 상단은 A/B sound state, 중앙은 macro/voice edit, 오른쪽은 preset browser와 morph amount, 하단은 연주 컨트롤이다.

```text
┌ Aoi YUME | A  ◀──── MORPH ────▶  B | PRESET/FAV ────────┐
│  MACRO 1      MACRO 2       MACRO 3       OUTPUT ▂▅▇     │
├───────────────┬────────────────────────┬────────────────┤
│ 4×4 MACRO PAD  │ VOICE / BUS / FX edit  │ PRESET LIST    │
│ color = state  │ selected macro targets │ search/favorite│
│ x/y drag       │ value + modulation     │ bank/program   │
├───────────────┴────────────────────────┴────────────────┤
│ PITCH | MOD | VELOCITY      SOUNDFONT          KEYBOARD   │
└───────────────────────────────────────────────────────────┘
```

**장점:** 연주와 음색 변화를 하나의 화면에서 보여주며, 제품 아이덴티티가 강하다.  
**단점:** 현재 DSP에 macro/morph 모델이 없어 기능 정의가 선행되어야 한다.  
**구현 난이도:** 높음. 신규 상태 모델과 automation semantics가 필요하다.

## 안 4 — Motion Sequencer Desk

Digitakt II와 minilogue xd의 시간축 조작을 중심으로 한다. 가운데 16-step lane에서 note/motion/lock을 보고, 왼쪽은 preset/voice, 오른쪽은 현재 step의 parameter를 편집한다. 전통적인 정적 신스보다 움직이는 패치 제작에 적합하다.

```text
┌ Aoi YUME | PATTERN 01 | 120 BPM | PLAY/REC | OUT ▂▅▇     ┐
├────────────┬─────────────────────────────┬───────────────┤
│ PRESET/VOICE│ 01 02 03 ... 16            │ STEP 05       │
│ bank/program│ NOTE ────────────────      │ cutoff 42%    │
│ track select│ MOTION ···············     │ drive 18%      │
│             │ LOCK   ▲   ▲      ▲        │ clear/copy     │
├────────────┴─────────────────────────────┴───────────────┤
│ CORE KNOBS: DRIVE / FILTER / ENV / FX       KEYBOARD      │
└───────────────────────────────────────────────────────────┘
```

**장점:** 자동화와 반복 패턴이 즉시 보이고, 사운드가 정적으로 느껴지지 않는다.  
**단점:** SF2 롬플러의 핵심 사용 사례와 거리가 있고, modifier UX가 복잡해진다.  
**구현 난이도:** 매우 높음. 현재 엔진에 시퀀서/모션 데이터가 없다.

## 안 5 — Compact Instrument

작은 하드웨어 모듈처럼 3열로 압축한다. 왼쪽은 preset, 중앙은 항상 보이는 핵심 음색 knob, 오른쪽은 output/performance. Bus/Comp/FX의 깊은 값은 펼침 drawer로 이동한다.

```text
┌ Aoi YUME | PROGRAM 003 | SAVE | MASTER ▂▅▇           ┐
├───────────────┬──────────────────────┬───────────────┤
│ PRESET BROWSER│ CORE VOICE           │ PERFORMANCE   │
│ bank/program  │ DRIVE  FILTER  ENV   │ PITCH / MOD   │
│ favorite      │ MIX    OFFSET  POLY  │ velocity      │
│ search        │ [ADVANCED ▼]         │ OUTPUT/PEAK   │
├───────────────┴──────────────────────┴───────────────┤
│ FX / BUS / COMP drawer (필요할 때만 펼침)              │
├───────────────────────────────────────────────────────┤
│ SOUNDFONT                              KEYBOARD       │
└───────────────────────────────────────────────────────┘
```

**장점:** 작은 창에서도 읽기 쉽고, 핵심 작업이 빠르다.  
**단점:** 현재의 풍부한 패널 인상이 줄고, 깊은 파라미터 발견성이 낮아진다.  
**구현 난이도:** 낮음~중간. 기존 레이아웃을 3열과 drawer로 정리하면 된다.

## 임시 비교표

| 안 | 하드웨어 레퍼런스 | 핵심 가치 | 작은 창 | 제품 개성 | 구현 위험 |
|---|---|---:|---:|---:|---:|
| 1 Signal Rail Console | Prophet-6 + JUPITER-X | 신호/프리셋/출력 균형 | 좋음 | 높음 | 중간 |
| 2 Modular Patch Bay | Matriarch | 라우팅 투명성 | 보통 | 매우 높음 | 높음 |
| 3 Morph Matrix | PolyBrute | 연주·매크로 표현력 | 보통 | 매우 높음 | 높음 |
| 4 Motion Desk | Digitakt II + minilogue xd | 시퀀스·자동화 | 보통 | 높음 | 매우 높음 |
| 5 Compact Instrument | 소형 신스 공통 | 즉시성·가독성 | 매우 좋음 | 보통 | 낮음 |

## 잠정 추천

**1번 Signal Rail Console을 기본안으로 추천한다.** 현재 구현된 UI 자산을 가장 많이 살리면서, 실제 하드웨어 신스에서 공통으로 발견되는 세 요소(신호 흐름, preset identity, output visibility)를 동시에 해결한다. 5번은 작은 호스트 창 대응용 fallback으로 흡수할 수 있다.

## 결정 로그

| 결정 | 상태 | 이유 |
|---|---|---|
| 최대 5개 구조안 | 확정 | 변형 중복을 줄이면서 충분한 선택폭 확보 |
| DSP/파라미터 ID 유지 | 확정 | UI 실험과 세션 호환성 분리 |
| 1번을 잠정 추천 | 대기 | 현재 UI 자산과 제품성의 균형이 가장 좋음 |
| 실제 구현안 선택 | 사용자 선택 필요 | 선택 전 코드를 바꾸지 않음 |

## 20개 신규 UI 콘셉트 시트 (2026-09-06)

현재 로드된 UI를 기준으로 구조·재질·색·사용 흐름을 바꾼 신규 콘셉트 20개를
5개씩 비교 시트로 만들었다. 이 산출물은 시각 비교용이며, 플러그인 소스나
파라미터 ID를 변경하지 않는다.

- [Variant 01–05](../../../outputs/aoi_yume_ui_new_versions_01-05_20260906.png)
  — split-browser, signal rail, patch bay, macro performance, meter bridge
- [Variant 06–10](../../../outputs/aoi_yume_ui_new_versions_06-10_20260906.png)
  — oscillator console, four-voice, routing matrix, performance macros, lab console
- [Variant 11–15](../../../outputs/aoi_yume_ui_new_versions_11-15_20260906.png)
  — silver, cream/oxblood, anodized blue, olive, matte black material passes
- [Variant 16–20](../../../outputs/aoi_yume_ui_new_versions_16-20_20260906.png)
  — preset-centric, performance keyboard, compact rack, FX matrix, modular desktop

## Aoi Yume 대표 방향 — Blue Dream

20개 시트에서 선별한 구조와 재질을 합쳐 `Aoi Yume = Blue Dream` 방향의
대표 히어로 콘셉트를 만들었다. 파란색을 단순한 포인트 컬러로 쓰지 않고,
야간의 깊이·달빛·수면처럼 잔잔한 파형·따뜻한 아이보리 건반으로 브랜드
의미를 UI 전체의 계층과 조명에 반영한다.

- [Blue Dream hero UI](../../../outputs/aoi_yume_blue_dream_hero_20260906.png)

이 이미지는 구현 전 방향 확인용이며, 현재 플러그인 소스와 파라미터 ID에는
아직 적용하지 않았다.

사용자 피드백 반영본:

- [Blue Dream refined — handwritten logo](../../../outputs/aoi_yume_blue_dream_hero_refined_20260906.png)

장식적인 달·산 이미지를 제거하고 저채도 graphite/blue-green 패널로 낮췄다.
`Aoi YUME`는 기존 UI와 같은 필기체 시그니처 방향으로 되돌렸고, 디스플레이는
최소한의 추상 파형만 남겼다.

## Blue Dream cinematic pass

사용자 피드백에 따라 달·산·수면·안개·별빛을 다시 늘린 버전이다. 장면을
개별 장식으로 분리하지 않고, ENV와 FX strip의 연속된 야경으로 연결했다.
필기체 `Aoi YUME` 시그니처와 실물 하드웨어 패널의 밀도는 유지한다.

- [Blue Dream cinematic UI](../../../outputs/aoi_yume_blue_dream_cinematic_20260906.png)
