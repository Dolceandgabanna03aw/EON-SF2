# EON SF2 (AgeOfDistortion)

JUCE 기반 SF2 롬플러 플러그인. CMake 프로젝트이며 Git 저장소다.

## 빌드 / 테스트

CMakePresets가 있으므로 프리셋을 쓴다. 경로를 직접 추측하지 말 것.

| 프리셋 | 빌드 디렉터리 | 용도 |
|---|---|---|
| `dev` | `build/` | 기본 개발·테스트 |
| `asan` | `build-asan/` | 메모리 오류 추적 |
| `plugin` | `build-plugin/` | 플러그인 포맷 빌드·검증 |

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

플러그인 포맷은 `plugins/rompler`에서 `juce_add_plugin`으로 정의되며
VST3 / AU / Standalone을 생성한다.

### 함정: NOT_BUILT

빌드하지 않은 상태로 `ctest --preset dev`를 돌리면 테스트 이름이
`x10_dsp_tests_NOT_BUILT-...`처럼 표시된다 (2026-09-02 확인).
이는 테스트가 없는 것이 아니라 **아직 빌드되지 않았다는 뜻**이다.
반드시 `cmake --build --preset dev`를 먼저 실행한다.

`plugin` 프리셋은 별개의 테스트 세트를 가진다 (ADAA 수치 검증 등).
DSP 정확도 관련 작업이면 `ctest --preset plugin`도 함께 확인한다.

## 완료 기준 (각각 별개의 증거다)

아래는 단계이지 대체재가 아니다. 앞 단계 통과가 뒷 단계를 보장하지 않는다.

1. 빌드 성공
2. `ctest` 통과 — 라이브러리 테스트는 `libs/{instrument,sf2,x10_dsp}/test`,
   플러그인 테스트는 `plugins/rompler/test`
3. `pluginval_vst3_strictness10` 통과 — CTest 타깃으로 등록되어 있다
4. 호스트(DAW) 로딩 확인
5. 실제 청취 확인

"빌드됐다"를 "동작한다"로 보고하지 말 것. 4·5단계는 사용자만 확인할 수 있으므로,
거기까지 못 갔으면 어디까지 검증했는지 명시한다.

## 실시간 오디오 제약

오디오 처리 경로(`processBlock` 및 그 하위)에서는 금지한다.

- 메모리 할당 / 해제
- 락, 뮤텍스, 대기
- 파일 I/O, 로깅

`tests/RealtimeSafetyTests` 계열이 이를 검증한다. 위반 시 조용히 글리치가 나므로
테스트 통과 여부를 반드시 확인한다.

## 주의

- 파라미터 ID와 `PLUGIN_CODE`를 바꾸면 기존 프리셋·세션 호환이 깨진다.
  변경이 필요하면 먼저 사용자에게 확인한다.
- `tools/`의 패키징·설치 스크립트(`package_aoi_yume_macos.sh`,
  `install_aoi_yume_macos.sh`)는 시스템에 설치를 수행한다.
  요청 없이 실행하지 말 것.
- `build*/`, `dist/`, `outputs/`는 산출물이다. 소스로 취급하지 않는다.
- 워크트리(`.worktrees/`)가 존재한다. 작업 전 현재 체크아웃과 HEAD를 먼저 확인한다.

## 작업 시작 전

`git status`로 기존 변경을 확인한다. 더티 워크트리는 사용자 작업일 수 있으므로
보존하고, 겹치는 부분만 조심해서 다룬다. `git reset --hard`나
`git checkout --`는 명시적 요청 없이 쓰지 않는다.
