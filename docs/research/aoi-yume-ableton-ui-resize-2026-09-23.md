# Aoi YUME Ableton UI 리사이즈 검증

기준: 2026-09-23, 체크아웃 `codex/mkii`, HEAD `eb6766f66caa39a96706716ebcd3fcd4c32ba111` + 현재 미커밋 WIP.

## 빌드 산출물

- Ableton이 읽는 사용자 VST3: `~/Library/Audio/Plug-Ins/VST3/Aoi YUME.vst3`
- 빌드 산출물과 설치 사본의 실행 파일 SHA256: `47fa14f1cba78bfdd4465754a759d94ef54fb4dbfebab48a6b91ec96d4ed9227`
- 이전 설치 사본은 `/tmp/Aoi YUME.vst3.previous-2026-09-23`에 백업했다.
- `pluginval_vst3_strictness10`: 통과, 279.12 s.
- `ctest --preset dev --output-on-failure`: 88/88 통과.
- `ctest --preset plugin --output-on-failure -E pluginval`: 167/167 통과.
- 리사이즈 회귀 테스트: `editor resize preserves the skin canvas aspect ratio`, 31 assertions 통과.

## Ableton Live 호스트 게이트

- 호스트: Ableton Live 12 Suite 12.4.5 (`2026-08-19_225ce5e356`).
- 오디오 설정: CoreAudio / Audient iD14, 44.1 kHz, 256 samples.
- Browser에서 `Aoi YUME`를 검색해 MIDI 트랙에 삽입했다. 트랙명이 `1-Aoi YUME`로 바뀌고 Live의 Device View에 `Aoi YUME`가 표시됐다.
- Device Parameters를 펼쳤을 때 Drive, Curve, Polyphony, Oversampling, Output Trim 등 플러그인 파라미터가 Live에 노출됐다.
- Drive를 Live의 Increment로 0.00% → 0.79%로 변경하고 Decrement로 0.00%로 되돌려 호스트 파라미터 경로를 확인했다.
- 플러그인 창 Show/Hide 토글은 Live에서 노출되고 동작했다. Live의 플로팅 자식 창은 이 자동화 캡처 대상에 별도 창으로 노출되지 않아, 창 자체의 드래그 크기 수치는 이 게이트에서 측정하지 못했다.
- 창 크기 보정은 `PluginEditor`의 fixed aspect-ratio constrainer와 headless canvas 테스트로 확인했다. 960×618, 1200×800, 1563×1006, 2000×1000 입력에서 canvas가 1563:1006 비율을 유지하며, 비율이 다른 입력은 중앙 letterbox 된다. 2000×1000 스냅샷에서 좌우 여백과 왜곡 없는 스킨을 확인했다.
- 스냅샷 증거: `/tmp/aoi-yume-resize-min.png` (960×618, SHA256 `94d6eafd64e5df14561f57895e4a935515f4c0c904f75c55ab31710bcc887c73`), `/tmp/aoi-yume-resize-letterbox.png` (2000×1000, SHA256 `9c49a7d28cff5e35c53f08ab0ffb3c1378f8adb96e15068c2834f277aeea0480`), `/tmp/aoi-yume-resize-default.png` (1563×1006, SHA256 `4f85d7c90a84eb89598bc94d76ca9dee6e0141a608aa23d45641a09e1b36c67d`).
- 이 기록에서는 REAPER를 실행하지 않았다. 사용자의 Ableton 단독 검증 지시에 따른다.
