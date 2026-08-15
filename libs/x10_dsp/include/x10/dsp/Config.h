#pragma once

#include <cstddef>

namespace x10::dsp
{

/** Audio sample type used throughout the core. */
using Sample = float;

/**
    Ill-conditioning threshold for the first-order ADAA difference quotient.

    When |x[n] - x[n-1]| falls below this value, Adaa1 evaluates the curve at the
    midpoint instead of dividing by dx. The value is measured, not guessed: it is
    the crossover between

      - the division path's error, dominated by catastrophic cancellation in
        F1(x) - F1(x1), which grows as dx shrinks, and
      - the midpoint path's error, which is O(dx^2) and shrinks with dx.

    Measured crossover across the four curves and |x| up to 8 spans 1e-3 to 7e-2;
    1e-2 sits inside that band while sending only 0.2-1% of samples down the
    midpoint path at full scale.

    Note this is three orders of magnitude ABOVE the 1e-5 that intuition (and the
    planning document) suggests. At dx = 1e-5 the measured division-path error is
    4.2e-3, roughly -47 dBFS of noise; by dx = 1e-8 it is NaN. Shrinking this
    constant to "be safer" does the opposite. test_adaa_conditioning.cpp pins the
    window and will fail if it is moved; PROVENANCE.md records the sweep.
*/
inline constexpr float kAdaaEps = 1.0e-2f;

/** Reference rate for the library's own regression fixtures. */
inline constexpr double kReferenceSampleRate = 48000.0;

} // namespace x10::dsp
