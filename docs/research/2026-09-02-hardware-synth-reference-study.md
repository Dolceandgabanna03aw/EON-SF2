# Hardware Synthesizer Reference Study

**조사일:** 2026-09-02 (KST)  
**목적:** 실제 하드웨어 신디사이저의 신호 구조, 조작 계층, 표시 방식, 프리셋/성능 흐름을 비교하고 Aoi YUME 플러그인 UI에 재사용할 원칙을 추출한다.

## 조사 방법과 범위

- 제조사 제품 페이지와 제조사 매뉴얼만 사용했다. 리뷰·포럼·리셀러의 평가는 제품 사실의 근거로 사용하지 않았다.
- 사실(제품이 실제로 제공하는 기능)과 추론(Aoi YUME에 적용할 때의 UI 판단)을 분리했다.
- 여섯 제품을 서로 다른 하드웨어 archetype으로 골랐다. Digitakt II는 전통적인 키보드 신디사이저가 아니라 샘플러/그루브박스지만, 시퀀서 중심 조작을 연구하기 위한 비교군이다.

## 1. 제품별 원자료 요약

| 제품 / archetype | 엔진·음성 구조 (검증된 사실) | 조작·표시·성능 (검증된 사실) | 프리셋·연결성 (검증된 사실) | UI에서 배울 점 | 주의점 (추론) |
|---|---|---|---|---|---|
| [**Moog Matriarch**](https://cdn.inmusicbrands.com/Moog/Matriarch/Matriarch_Manual_012023.pdf)  semi-modular analog | 4 아날로그 VCO, mono/2-note/4-note paraphonic; 듀얼 Ladder filter를 series/parallel/stereo로 구성; stereo analog delay | 90개 patch point, 색으로 구분된 single-function 노브/스위치/버튼; 49 velocity+aftertouch 키; 12개 시퀀스(각 최대 256 note), arp | 5-pin DIN·USB MIDI, 외부 오디오 입력, expression pedal, Eurorack/CV 친화; 전용 프로그램 메모리보다 물리적 patch-sheet/수동 재현 흐름에 가깝다 | 신호 흐름을 물리적 섹션과 색으로 드러내고, 고빈도 조작은 전용 컨트롤로 둔다 | patch point와 paraphony는 강력하지만 작은 화면에서 설명 비용이 크다. 모든 기능을 한 화면에 노출하면 시각 밀도가 급증한다. |
| [**Sequential Prophet-6**](https://sequential.com/modern-analog/prophet-6/)  knob-per-function analog poly | 6 voice, voice당 2 discrete VCO+sub; 4-pole resonant LPF와 2-pole resonant HPF; poly glide/unison | “no menu diving” 조작 철학; front-panel 설정을 즉시 소리로 쓰는 Manual 모드; 64-step poly sequencer, arp, stereo distortion·dual digital FX; velocity/channel aftertouch | 500 factory+500 user 프로그램(10×100), direct program access; MIDI In/Out/Thru, USB, pedal 입력, stereo out | 패널의 보이는 값과 실제 소리가 일치해야 한다. 프리셋 호출과 수동 편집을 분리하지 말고 즉시 전환 가능하게 한다 | 전용 노브는 즉시성이 높지만, 구조가 깊어질수록 패널 면적과 비용이 커진다. 메뉴/페이지형 기능을 억지로 노브처럼 꾸미면 오히려 오해를 낳는다. |
| [**Korg minilogue xd**](https://www.korg.com/us/products/synthesizers/minilogue_xd/)  compact analog/digital hybrid | 아날로그+Multi digital engine(Noise/VPM/User), 4 voice; POLY/UNISON/CHORD/ARP-LATCH voice mode | 16-step poly sequencer(실시간·step 입력), 최대 4 parameter motion sequence; joystick; OLED oscilloscope와 motion view; micro-tuning | 500 program(300 user), favorite; CV IN 2, sync in/out, MIDI, USB, damper | 작은 본체에서도 파형/모션을 직접 보여주면 조작 결과를 학습할 수 있다. voice mode와 depth를 한 묶음으로 제시한다 | 4 voice와 16 step은 명확한 한계다. OLED가 모든 깊은 파라미터를 대체하지 않으므로 핵심 조작과 설정 페이지를 분리해야 한다. |
| [**Roland JUPITER-X**](https://www.roland.com/Us/products/jupiter-x/)  multitimbral ZEN-Core/model platform | ZEN-Core와 여러 Model engine; 5 parts(신스 4+리듬 1); I-ARPEGGIO가 5 parts를 동시에 생성/편집; 512 scenes | 61 semi-weighted+channel aftertouch; 큰 노브·슬라이더·버튼과 informative 128×64 LCD; 레이어 빠른 전환; balanced out·mic·pedal | preset tone 4,000+, user tone 512, drum kit 90+, scene 512; USB audio/MIDI, DIN MIDI, USB memory, Bluetooth; 무료 Editor | 복잡한 여러 레이어는 Scene을 최상위 작업 단위로 묶고, 현재 part/effect를 항상 보여준다. 하드웨어와 editor의 역할을 분리한다 | 작은 LCD에 모델·part·effect·arp를 모두 담으려 하면 탐색 비용이 커진다. “많은 기능”을 “쉬운 기능”으로 오해하지 않게 정보 우선순위를 강제해야 한다. |
| [**Arturia PolyBrute**](https://www.arturia.com/products/hardware-synths/polybrute/overview)  morphing analog polysynth | 6 voice analog; 2 VCO, Steiner+Ladder 듀얼 filter, series/parallel; preset마다 A/B 두 상태를 저장하고 연속 morph | 8×12 Matrix panel: Presets/Mods/Morph/Sequencer 모드; 최대 32 modulation destinations·64 connections; Morphée X/Y/Z pressure, ribbon, motion recorder; 64-step poly seq+3 automation track | 768 preset slots; 61 velocity+pressure 키; MIDI·USB·analog clock; PolyBrute Connect가 양방향 편집 | 하나의 물리 표면을 모드별로 재사용하되, 버튼 색/상태로 현재 모드를 명확히 한다. 매크로 하나가 여러 파라미터를 움직일 때 결과를 시각화한다 | Matrix는 강력하지만 모드 기억 부담이 있다. 작은 플러그인 창에 96셀 격자를 복제하면 장식이 기능을 압도할 수 있다. |
| [**Elektron Digitakt II**](https://www.elektron.se/wp-content/uploads/2025/07/Digitakt-2-User-Manual_ENG_OS1.15A_250708.pdf)  sequencer/sampler reference | 16 tracks가 audio 또는 MIDI; stereo sampling; 128-step sequencer; audio track은 sample+parameter pages를 가진다 | 8개 DATA ENTRY encoder, PARAMETER page, 16 trig key; parameter lock(패턴당 최대 80 parameter), trig condition/conditional lock; grid/live/step recording | project→pattern→kit→preset→sample 계층; 외부 MIDI 장비 제어; 매뉴얼 기준 OS 1.15A(2025-07) | 상태를 색과 점멸로 즉시 보여주는 step grid, “현재 track/page”를 명확히 고정하는 방식, 반복 가능한 프로젝트 계층 | 키보드 신디의 신호 경로와 다른 제품이다. [FUNC]+키 같은 modifier가 많아 초보자에게는 발견성이 낮다. 이를 Aoi YUME에 그대로 복제하지 않는다. |

### 제품별 출처

1. Moog/inMusic, [Matriarch User Manual PDF](https://cdn.inmusicbrands.com/Moog/Matriarch/Matriarch_Manual_012023.pdf) 및 [Moog manual download listing](https://www.moogmusic.com/downloads/?product=Matriarch&type=User+Manual) — 4 VCO, paraphony, 90 patch points, 색 구분 패널, 시퀀서/연결성, 수동 patch 기록 흐름.
2. Sequential, [Prophet-6 product page](https://sequential.com/modern-analog/prophet-6/) 및 [Operation Manual](https://www.sequential.com/wp-content/uploads/2021/02/Prophet-6-Operation-Manual-2.1.pdf) — voice/filter 구조, no-menu 조작, 프로그램·sequencer·MIDI 사양.
3. Korg, [minilogue xd product page](https://www.korg.com/us/products/synthesizers/minilogue_xd/) 및 [specifications](https://www.korg.com/se/products/synthesizers/minilogue_xd/specifications.php) — 4 voice, voice mode, motion sequence, OLED, 프로그램·CV/MIDI/USB.
4. Roland, [JUPITER-X product page](https://www.roland.com/Us/products/jupiter-x/) 및 [Reference Manual](https://static.roland.com/assets/media/pdf/JUPITER-X_Reference_eng06_W.pdf) — ZEN-Core/model, 5-part scene, I-ARPEGGIO, LCD, preset·연결성.
5. Arturia, [PolyBrute overview](https://www.arturia.com/products/hardware-synths/polybrute/overview) 및 [details](https://www.arturia.com/products/hardware-synths/polybrute/details) — A/B morph, Matrix, Morphée/ribbon, sequencer, Connect.
6. Elektron, [Digitakt II User Manual OS 1.15A](https://www.elektron.se/wp-content/uploads/2025/07/Digitakt-2-User-Manual_ENG_OS1.15A_250708.pdf) — 16 audio/MIDI tracks, 128 steps, 화면/패널, parameter lock, trig condition.

## 2. 공통 패턴과 차이

### A. 물리적 신호 흐름이 먼저다

Matriarch·Prophet-6·PolyBrute는 oscillator/mixer/filter/envelope/output을 패널의 공간과 이름으로 구획한다. JUPITER-X도 다섯 part와 scene을 중심으로 현재 레이어를 전환한다. 이 제품군에서 패널은 장식이 아니라 사용자가 “어디에서 어디로 소리가 가는지”를 기억하는 지도다.

**Aoi YUME 적용:** 현재의 `Voice → Bus → Comp → Envelope/FX` 섹션을 단순 박스 모음으로 두지 말고, 상단에 얇은 signal-rail을 고정한다. rail에는 현재 활성화, bypass, peak/clip, master trim만 표시하고 상세 노브는 각 섹션에 둔다.

### B. 고빈도 조작과 저빈도 설정을 분리한다

Prophet-6의 knob-per-function과 Matriarch의 single-function 패널은 cutoff, envelope, mix처럼 반복해서 만지는 값을 즉시 노출한다. 반대로 JUPITER-X의 방대한 모델/scene과 Digitakt II의 프로젝트·샘플 계층은 별도의 탐색 구조가 필요하다.

**Aoi YUME 적용:** 현재처럼 Drive, Filter, Envelope, FX의 핵심 값은 전용 knob를 유지한다. SoundFont 경로, MIDI/oversampling, preset 관리처럼 빈도가 낮은 항목은 drawer/page로 분리하고, 페이지를 바꿔도 preset identity와 master/output은 고정한다.

### C. 표시 장치는 값을 읽는 것보다 변화를 이해하게 해야 한다

Korg는 OLED oscilloscope/motion view로 파라미터 변화와 결과를 함께 보여준다. PolyBrute Matrix는 버튼 색으로 preset/mod/sequence 상태를 나타내고, Digitakt II는 trig의 색·점멸·반전 그래픽으로 note/lock/page 상태를 구분한다. JUPITER-X의 128×64 LCD는 작은 면적에 scene/part 정보를 제공하지만, 깊은 편집은 Editor가 담당한다.

**Aoi YUME 적용:** knob를 움직일 때만 numeric readout을 띄우는 현재 패턴을 유지하되, 다음 세 가지를 추가 우선순위로 둔다.

1. master/output peak와 clip 경고는 항상 보이게 한다.
2. envelope/FX에는 값 숫자만이 아니라 짧은 shape/amount feedback을 제공한다.
3. `factory/user`, `saved/dirty`, `bypass/active`, `init/non-init` 상태는 색 하나만으로 표현하지 말고 텍스트·아이콘·대비를 함께 사용한다.

### D. 성능 제어는 별도 악기처럼 취급한다

Matriarch의 aftertouch·glide·pitch/mod wheel, Korg joystick, PolyBrute의 Morphée/ribbon, JUPITER-X의 wheels/sliders는 음색 편집과 연주를 연결한다. 이것들은 “부가 기능”이 아니라 preset이 살아 움직이는 표면이다.

**Aoi YUME 적용:** pitch/mod wheel 영역을 단순 장식으로 두지 말고, 현재 preset에서 실제로 영향을 주는 destinations 또는 velocity-to-drive의 상태를 작은 legend로 노출한다. 이후 macro가 추가되면 PolyBrute식 A/B morph처럼 여러 파라미터의 변화를 한 개의 명확한 surface로 묶는다.

### E. 프리셋은 이름 목록이 아니라 작업 단위다

Prophet-6은 직접 프로그램 접근과 Manual 모드를, JUPITER-X는 512 Scene을, PolyBrute는 preset 안의 A/B 상태를, Digitakt II는 project/pattern/kit 계층을 제공한다. 공통점은 사용자가 “현재 무엇을 호출했는가”를 잃지 않게 하는 것이다.

**Aoi YUME 적용:** header에 `bank / program / source(factory|user) / dirty`를 고정한다. 브라우저에는 favorite와 최근 사용을 넣되, 수천 개의 가상 preset 수를 흉내 내기보다 SoundFont bank와 program의 실제 범위를 정확히 보여준다.

## 3. Aoi YUME에 대한 우선순위 제안

### P0 — 제품처럼 느껴지는 최소 골격

- **Persistent signal rail:** Voice → Bus → Comp → FX → Output. 각 노드에 active/bypass와 peak 상태.
- **명확한 출력 단계:** master trim/volume, peak meter, clip 표시, headphone/host output 혼동 방지. “소리가 작다”는 호스트·샘플·plugin gain 중 어디에서든 확인할 수 있어야 한다.
- **Preset identity lock:** bank/program/source/dirty를 항상 노출하고, load 후 현재 선택과 실제 파라미터가 어긋나지 않게 한다.
- **Live panel semantics:** 수동 편집 모드에서는 Prophet-6의 “what you see is what you hear”처럼 화면의 knob가 실제 DSP 값과 즉시 일치해야 한다.

### P1 — 표현력과 발견성

- pitch/mod·velocity를 hardware performance strip으로 묶고 destination을 표시한다.
- envelope와 FX에 짧은 시각 feedback을 추가한다. 숫자만 읽는 UI보다 Korg식 변화 시각화가 학습 속도가 빠르다.
- 파라미터가 늘어날 경우 `core / modulation / routing / system` 페이지로 계층화한다. Matrix를 그대로 복제하지 말고 현재 선택한 source→destination 한 행만 먼저 보여준다.

### P2 — 선택적 확장

- 제품 목표가 “움직이는 패치”로 확장될 때만 8–16 step motion lane을 도입한다. Digitakt II 수준의 conditional lock은 별도 제품 요구가 없으면 과하다.
- A/B morph 또는 macro는 여러 파라미터를 동시에 바꿀 때 실제 변경 범위와 bypass를 시각적으로 설명할 수 있을 때만 추가한다.
- DAW와의 preset/editor 연동은 양방향 동기화 규칙과 충돌 해결 정책을 먼저 정의한 뒤 도입한다.

## 4. 하지 말아야 할 것

- 작은 플러그인 창에 JUPITER-X의 방대한 모델 메뉴나 PolyBrute의 96셀 Matrix를 장식적으로 복제하지 않는다.
- 실제 동작하지 않는 가짜 노브·미터·LED를 배치하지 않는다. 하드웨어처럼 보일수록 표기와 DSP의 불일치가 더 큰 오류로 느껴진다.
- 색 하나만으로 active/bypass/dirty/error를 구분하지 않는다. 색각·저대비 환경과 스크린샷 검증을 고려한다.
- preset 수를 부풀려 탐색 피로를 만들지 않는다. 실제 SoundFont의 bank/program 구조와 즐겨찾기/검색을 우선한다.
- hidden modifier 조작을 핵심 작업으로 만들지 않는다. Digitakt II의 `FUNC` 조합은 강력하지만, Aoi YUME에서는 기본 조작을 한 번의 명시적 동작으로 유지한다.

## 5. 다음 검증 루프

1. **UI:** 1120×900와 작은 호스트 창에서 signal rail, preset identity, output meter가 동시에 읽히는지 캡처한다.
2. **상태:** preset load, manual edit, bypass, reset 후 `saved/dirty/init` 상태를 자동 테스트한다.
3. **오디오:** DAW에서 동일 SoundFont의 dry/wet와 master trim을 비교해 output 단계의 실제 gain을 측정한다. UI 스크린샷과 DAW 청취는 별도 증거로 기록한다.
4. **회귀:** 핵심 knob와 preset browser에 대해 현재 `[ui]`, `[preset][ui]` 테스트를 유지하고, 새 signal rail에는 상태 전이 테스트를 추가한다.

이 문서는 조사 결과와 설계 추론을 분리한 레퍼런스다. 제품 기능을 그대로 복제하자는 사양서가 아니며, 실제 구현 전에는 Aoi YUME의 DSP 파라미터·호스트 제약·창 크기와 대조해야 한다.
