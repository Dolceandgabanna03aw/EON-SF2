# M0 — plugin skeleton, status and constraints

What exists: a synth that loads in a host, negotiates buses, exposes its
parameters, round-trips its state and outputs silence. No DSP is connected yet.

## Gate results

| Gate | Result |
|---|---|
| Build, `-Werror` | clean, 0 warnings |
| Catch2 smoke (4 tests) | pass |
| pluginval VST3, strictness 10 | **SUCCESS** |
| Full suite (`ctest`) | 56/56, twice in a row |
| Ad-hoc signature, all 3 formats | `codesign --verify --deep --strict` passes |

## Constraints found on this machine

**Notarised distribution signing is not possible yet.** The only identity
present is `Apple Development`; there is no `Developer ID Application`
certificate, which is what notarisation requires. The build therefore ad-hoc
signs instead, which is sufficient for local hosts and for pluginval but not for
distribution. The M0 acceptance criterion in the planning document asks for a
working notarisation pipeline — that part is deferred to M6 and needs a paid
Apple Developer Program membership first.

**AU was built and signed but not validated.** `auval` requires the component to
be installed into `~/Library/Audio/Plug-Ins/Components`, which writes into the
user's plugin folder. `COPY_PLUGIN_AFTER_BUILD` is off and installation was not
performed. VST3 carries the strictness-10 gate; AU validation is a one-command
follow-up once installation is agreed.

**JUCE splash screen is left on.** `JUCE_DISPLAY_SPLASH_SCREEN` is deliberately
not set to 0: turning it off requires a paid JUCE licence and the tier for this
project is still unresolved (planning document §10). Setting it would be
asserting a licence we may not hold.

## Three traps worth remembering

**`JUCE_PLUGIN_ARTEFACT_FILE` holds a generator expression as its value.**
Reading it with `$<TARGET_PROPERTY:...>` yields another unevaluated genex, so it
must be wrapped in `$<GENEX_EVAL:...>`. Without that, `codesign` and `pluginval`
both received the literal string `$<TARGET_BUNDLE_DIR:...>`. The pluginval case
was the nastier one: ctest reported a failure in 0.35 s while running the tool
by hand succeeded, which looks like a plugin defect and is not.

**Signing has to happen last.** JUCE runs its own signature check and *then*
writes `moduleinfo.json` into the VST3 bundle, invalidating whatever signature
was there. The POST_BUILD signing step is appended after JUCE's own steps for
this reason.

**JUCE's recommended warning flags are PUBLIC on the shared-code target.** They
include `-Wfloat-equal`, which propagates into anything linking the plugin — so
`REQUIRE (value == 0.0f)` in a test fails to compile under `-Werror`. The smoke
test asserts silence through `getMagnitude(...) <= 0.0f` instead.

## One real bug the suite caught

`rompler_tests` aborted with SIGABRT after all tests had already passed, roughly
one run in three. The cause was holding `ScopedJuceInitialiser_GUI` in a
function-local static: it is destroyed during `exit()` in an order unspecified
relative to JUCE's own statics. Ownership moved into `main()`, which makes both
ends deterministic and on the main thread. Verified over 20 consecutive runs.

Worth noting because the failure only appeared through ctest at first and passed
when the test was run alone — the kind of flake that gets dismissed as
environmental.
