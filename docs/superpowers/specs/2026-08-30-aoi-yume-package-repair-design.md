# Aoi YUME Package Repair Design

## Goal

Make the host-visible installation and distributable macOS archive match the
current Aoi YUME build and canonical bundled SoundFont names, while keeping
validation honest when the local GUI session cannot start pluginval.

## Scope

- Build VST3, AU, Standalone, and the test target from the current checkout.
- Create a clean `Aoi-YUME-macOS` archive from those exact artifacts, without
  `__MACOSX` metadata or legacy EON-DS50 filenames.
- Install VST3 and AU through a recoverable backup-and-replace operation.
- Verify vendor/product metadata, canonical SoundFont contents, and code
  signatures after both packaging and installation.
- Keep the legacy filename restoration mapping in the processor.
- Report pluginval as an environment-blocked check if AppKit aborts before the
  validator parses the plugin; never convert that abort into a pass.

## Non-goals

- Do not redistribute the ignored multi-gigabyte `testdata/` corpus through Git.
- Do not delete the previous archive or installed bundles; move replaced
  installed bundles to a timestamped backup directory.
- Do not claim DAW audition or notarization from local build evidence.

## Acceptance criteria

1. The fresh archive contains only `Aoi YUME.vst3`, `Aoi YUME.component`, and
   `Aoi YUME.app`, with exactly the three canonical SoundFonts in each bundle.
2. Installed VST3/AU bundles contain the same canonical filenames and pass
   `codesign --verify --deep --strict`.
3. `ctest -E '^pluginval_vst3_strictness10$' --output-on-failure` passes all
   remaining tests.
4. pluginval behavior is recorded as either a real exit-0 validation or an
   explicit environment-blocked result with its diagnostic report.
5. No source-level old filename remains outside compatibility tests/mapping.
