# x10_dsp — provenance and measured constants

Two purposes. First, to record where each algorithm came from, so the licence
boundary stays defensible: everything here is implemented from published
equations, and no code is derived from a GPL source. Second, to record the
measurements behind the numeric constants, so nobody has to re-derive them or,
worse, adjust them until a test passes.

Measurements taken on Apple M1 Ultra, Apple clang 16.0.0, `-O2`, float32,
48 kHz, 16384-point analysis.

---

## 1. Algorithm sources

| Component | Source | Licence exposure |
|---|---|---|
| First-order ADAA | Parker, Zavalishin, Le Bivic, *Reducing the Aliasing of Nonlinear Waveshaping Using Continuous-Time Convolution*, DAFx-16 | None — published equation, implemented from scratch |
| `logCosh` stable form | `cosh x = e^{\|x\|}(1 + e^{-2\|x\|})/2` identity | None — standard identity |
| TPT state variable filter | Zavalishin, *The Art of VA Filter Design*; Simper's SVF formulation | None — published topology |
| One-pole DC blocker | Standard `y[n] = x[n] - x[n-1] + R·y[n-1]` | None |
| Goertzel (test only) | Standard single-bin DFT recurrence | None |
| Tube / Transformer / Wavefolder curves | Original, derived here; antiderivatives worked out by hand and verified against numerical integration by `test_curves.cpp` | None |

No source from Surge, or from any other GPL-licensed plugin, was read or adapted
while writing this library. That matters for the RAG corpus policy: GPL-tagged
material must stay out of anything that feeds generation for this target.

---

## 2. `kAdaaEps` = 1e-2

### Why the intuitive value is wrong

The planning document proposed roughly 1e-5, reasoning about the denominator
going to zero. The dominant error is in the **numerator**: `F1(x) - F1(x1)`
loses significant digits by cancellation, and the loss grows as dx shrinks.

Error of each path against a double-precision Simpson reference, Tanh at
x0 = 0.37:

| dx | division path | midpoint path |
|---|---|---|
| 1e-01 | 2.66e-07 | 2.79e-04 |
| 1e-02 | 5.78e-07 | 2.60e-06 |
| 1e-03 | 1.52e-05 | 2.43e-08 |
| 1e-04 | 6.29e-05 | 2.28e-08 |
| 1e-05 | **4.21e-03** | 1.53e-08 |
| 1e-06 | 9.64e-03 | 1.22e-08 |
| 1e-07 | 3.54e-01 | 1.49e-08 |
| 1e-08 | **NaN** | 0.00e+00 |

At the proposed 1e-5 the division path carries 4.2e-3 of error — about
-47 dBFS — and the guard would not have fired. The NaN the document predicted
does appear, but three orders of magnitude further down than expected.

### Crossover, per curve and operating point

dx below which the midpoint is the more accurate path, located by bisection:

| \|x\| | Tanh | Tube | Transformer | Wavefolder |
|---|---|---|---|---|
| 0.001 | 2.1e-02 | 3.7e-02 | 1.0e-03 | 1.8e-03 |
| 0.01 | 2.5e-02 | 2.9e-02 | 1.0e-03 | 8.3e-02 |
| 0.1 | 1.8e-02 | 2.2e-02 | 1.9e-03 | 4.4e-01 |
| 0.37 | 1.0e-02 | 1.2e-02 | 5.2e-03 | 2.6e-01 |
| 1.0 | 1.3e-02 | 5.0e-03 | 1.3e-02 | 9.6e-01 |
| 2.0 | 2.2e-02 | 2.1e-02 | 2.0e-02 | 4.2e-01 |
| 4.0 | 7.5e-02 | 6.3e-02 | 3.9e-02 | degenerate |
| 8.0 | 1.0e+00 | 4.2e-01 | 1.1e-01 | degenerate |

The wavefolder is degenerate at large \|x\| because its segments are linear
there: f'' is zero, so the midpoint is exact and the comparison is decided by
rounding alone.

### Cost of choosing too large a value

Percentage of samples taking the midpoint path, full-scale sine:

| eps | 1245 Hz | 3999 Hz | 8001 Hz |
|---|---|---|---|
| 1e-03 | 0.10% | 0.02% | 0.02% |
| 3e-03 | 0.29% | 0.10% | 0.05% |
| **1e-02** | **0.98%** | **0.32%** | **0.17%** |
| 3e-02 | 2.93% | 0.93% | 0.49% |
| 1e-01 | 9.81% | 3.08% | 1.59% |

**1e-2** sits inside the crossover band for every curve at realistic levels
while diverting at most 1% of samples. Where it is below the crossover — Tanh
around \|x\| = 4, crossover 7.5e-2 — the division path's error there is about
2e-5, roughly -94 dBFS, which is not worth spending 10% of samples to avoid.

The NMR measurement is insensitive to this constant (identical to two decimals
from eps = 0 to 1e-1 at full scale), so it cannot be used to choose it. The
per-sample tables above are the only usable instrument.

`test_adaa_conditioning.cpp` pins the window and fails if the constant moves
outside it.

---

## 3. Alias gate calibration

### Operating point

3999 Hz (bin 1365), amplitude 4.0. Alias energy is separated from harmonic
energy by Parseval subtraction; bin 1365 is odd and the window is 2^14, so the
two are coprime and no folded alias can land on a harmonic bin.

### Measured ADAA improvement over naive, amplitude 4.0

| tone | Tanh | Tube | Transformer | Wavefolder |
|---|---|---|---|---|
| 1245 Hz | 5.20 | 5.66 | 6.77 | 9.16 |
| 2001 Hz | 5.37 | 5.65 | 6.60 | 8.59 |
| **3999 Hz** | **6.84** | **7.15** | **7.67** | **8.62** |
| 8001 Hz | 5.14 | 5.34 | 5.28 | 12.33 |
| 12003 Hz | 14.37 | 10.64 | 11.92 | 9.28 |

3999 Hz gives the tightest cluster (6.8–8.6 dB), so the threshold is set to
**5.0 dB** — under every measured value with margin, and far above the 0 dB a
naive substitution scores.

### Two findings that contradict the planning document

**First-order ADAA does not deliver 15 dB.** The M3 acceptance criterion asks
for ≥ 15 dB improvement at 1 kHz full drive. Measured improvement at 1245 Hz is
5.2 dB, and the best figure anywhere in the matrix is 14.4 dB at 12 kHz. As
written, that criterion would block a correct implementation. Either the
criterion drops to about 5 dB, or the design moves to second-order ADAA or real
oversampling — which is the polyphony trade-off the plan defers to M3 anyway.

**Below roughly 2 kHz the metric inverts.** At 501 Hz and amplitude 4, ADAA
measures -104.5 dB against naive's -131.0 dB: ADAA is 26 dB *worse*. Nothing is
broken. A naive curve barely aliases at that frequency, so the ratio is
measuring ADAA's own numerical floor rather than any aliasing. Any gate placed
at 1 kHz, as the planning document suggests, would fail a correct build.

### Why the gate is relative, not absolute

At the operating point, correct ADAA wavefolding measures -9.8 dB while naive
hard clipping measures -38.4 dB, because a triangle folder is harmonically
richer than a clipper. No single absolute threshold can both accept the former
and reject the latter. The gate therefore compares each candidate against naive
evaluation of the same curve.

---

## 4. Known numerical floor

Driven to ±12, `log(cosh x)` reaches about 11.3, whose float ulp is ~1e-6.
Differencing two such values leaves a relative error of order 1e-5 in the
quotient. Measured worst overshoot beyond the curve's own bound is 1.7e-5,
about -95 dBFS. This is the floor of first-order ADAA in float32 at high drive;
no alias measurement on this library can read better than roughly -95 dB.

---

## 5. Gate self-verification

The suite is checked against deliberate regressions rather than trusted. Each
of these was applied to the working tree, built, and confirmed to fail:

| Injected fault | Caught by |
|---|---|
| `kAdaaEps` set to 1e-5 | interval-mean, half-sample-delay, and eps-window tests |
| `reset()` zeroing the F1 cache instead of `F1(0)` | the F1-cache reset test |
| ADAA replaced by naive direct evaluation | interval-mean, half-sample-delay, and gate-accept tests |

The second of these initially passed against the broken build. The test drove a
bin-centred sine, whose first sample is exactly zero, so dx was zero, the
midpoint fallback ran, and the corrupted cache was never read. The test now
forces a large first sample after reset. Recorded here because it is the exact
failure the gate self-check exists to prevent, and it occurred in this
library's own fixtures on the first attempt.
