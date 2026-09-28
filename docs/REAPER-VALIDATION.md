# REAPER host validation — Aoi YUME

Date: 2026-09-28. This is the real-DAW evidence record; it is separate from
build, ctest, pluginval, eonqc offline-render, and listening evidence.

## Evidence binding

| Field | Value |
|---|---|
| Plugin binary SHA-256 | `b37888bccb95db6b289d31c548f2def80ca7a62c85c6afc65c9084646d52c0e4` |
| Loaded file (via lsof on the live REAPER process) | `~/Library/Audio/Plug-Ins/VST3/Aoi YUME.vst3/Contents/MacOS/Aoi YUME` |
| Build artefact | `build-plugin/plugins/rompler/EONDS50_artefacts/RelWithDebInfo/VST3/Aoi YUME.vst3` (identical hash) |
| Host | REAPER v7.79, macOS arm64, isolated profile (evaluation mode — license file is profile-scoped) |
| Sample rate / block size | 48000 Hz / 512 |
| Commit under test | `be0765a` (codex/mkii) |

The installed bundle was refreshed to the build artefact before this run;
the previous copy is preserved as `Aoi YUME.vst3.old-20260928` next to it.

## Method

Isolated REAPER profile at `/tmp/aoi-yume-reaper-host` launched as
`REAPER -nosplash -newinst -cfgfile <profile>/reaper.ini <profile>/smoke.lua`.
The script inserts `VST3i: Aoi YUME (EON LAB)` on a fresh track, places a
one-second C4 MIDI note, renders the master mix over a 0–2 s time selection,
and writes `evidence.txt` before quitting.

The first launch hit the documented Waves WaveShell scan stall; killing the
`-__vst_scan__` helper children let the scan finish, as recorded in
`audio-knowledge/validation/reaper-reascript-smoke-pattern.md`.

## Result — pass

```
status=rendered
fx_index=0
fx_name=VST3i: Aoi YUME (EON LAB)
param_count=2116
render_bytes=576690
```

`render/smoke.wav`: 2 s, 48 kHz, stereo, 24-bit PCM; peak −1.3 dBFS,
RMS −18.6 dBFS — the note actually sounded, so REAPER loaded the plugin,
the bundled SoundFont became available through the deferred message-thread
drain inside a real host, and audio flowed.

## Artefacts (ephemeral /tmp paths, re-runnable)

- `/tmp/aoi-yume-reaper-host/smoke.lua` — driver script
- `/tmp/aoi-yume-reaper-host/evidence.txt` — script log
- `/tmp/aoi-yume-reaper-host/render/smoke.wav` — rendered audio
- `/tmp/aoi-yume-reaper-host/smoke.rpp` — saved project showing the FX slot

## Not covered here

Ableton Live load/automation/recall remains a separate manual checklist —
there is no Live automation driver yet (`soundcraft host-smoke` only records
manually observed results). Listening judgement remains open.
