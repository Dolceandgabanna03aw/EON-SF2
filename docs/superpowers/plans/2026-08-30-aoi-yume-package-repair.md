# Aoi YUME Package Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Repair the stale macOS installation/archive and make validation/build limitations explicit without masking failures.

**Architecture:** Treat the current `RelWithDebInfo` plugin bundles as the single build source. Stage exact copies into a clean distribution directory, zip that directory with macOS metadata excluded, and replace installed VST3/AU bundles only after creating a recoverable backup. Keep pluginval as an independent host check and mark it blocked when AppKit aborts before validation.

**Tech Stack:** CMake/Ninja, JUCE, Catch2/CTest, macOS `ditto`, `zip`, `codesign`, `auval`, Tracktion pluginval 1.0.4.

---

### Task 1: Reconfirm root causes and build state

**Files:**
- Read: `plugins/rompler/CMakeLists.txt`
- Read: `docs/superpowers/specs/2026-08-30-aoi-yume-package-repair-design.md`

- [ ] Run a persistent full build for VST3, AU, Standalone, and `rompler_tests`.
- [ ] Run `ctest -E '^pluginval_vst3_strictness10$' --output-on-failure`.
- [ ] Reproduce pluginval with `--help` and record the macOS diagnostic report path.

### Task 2: Add repeatable packaging and installation tooling

**Files:**
- Create: `tools/package_aoi_yume_macos.sh`
- Create: `tools/install_aoi_yume_macos.sh`
- Test: `tools/test_aoi_yume_packaging.sh`

- [ ] Add a dry-run-capable packaging script that stages the three built bundles,
  checks their SoundFont lists and signatures, and emits `dist/Aoi-YUME-macOS.zip`.
- [ ] Add an install script that backs up existing Aoi YUME VST3/AU bundles into
  `~/Library/Audio/Plug-Ins/.aoi-yume-backups/<timestamp>` before replacement.
- [ ] Add shell tests for archive names, canonical SoundFonts, and backup-path
  safety; run them before implementation to establish RED.

### Task 3: Repair the local package and installation

**Files:**
- Generated: `dist/Aoi-YUME-macOS.zip`
- External installation: `~/Library/Audio/Plug-Ins/VST3/Aoi YUME.vst3`
- External installation: `~/Library/Audio/Plug-Ins/Components/Aoi YUME.component`

- [ ] Run the packaging script against the freshly built artifacts.
- [ ] Run the install script after verifying the package manifest.
- [ ] Verify installed bundles contain canonical filenames, metadata strings,
  valid signatures, and no legacy SoundFont files.

### Task 4: Final safety and validation review

**Files:**
- Verify: all changed files and generated package manifest

- [ ] Run the shell packaging test, full non-pluginval CTest, `git diff --check`,
  and source-name compatibility scan.
- [ ] Run pluginval once in documented headless mode; if AppKit aborts before
  validation, preserve the failure evidence and do not report a pass.
- [ ] Update continuity with exact build, package, install, and validator evidence.
