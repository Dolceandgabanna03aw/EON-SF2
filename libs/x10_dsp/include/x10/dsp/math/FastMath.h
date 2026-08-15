#pragma once

#include <cmath>

namespace x10::dsp::math
{

inline constexpr float kLn2 = 0.693147180559945f;

/**
    log(cosh(x)), evaluated without overflowing.

    The direct form std::log(std::cosh(x)) overflows to +inf for |x| > ~89 in
    float, which turns a loud transient into a NaN downstream. Factoring
    cosh(x) = e^|x| (1 + e^{-2|x|}) / 2 gives

        log(cosh x) = |x| + log1p(e^{-2|x|}) - log 2

    which is exact at x = 0 and degrades gracefully into the |x| - log 2
    asymptote as e^{-2|x|} underflows. No branch and no range table needed.
*/
[[nodiscard]] inline float logCosh (float x) noexcept
{
    const float a = std::abs (x);
    return a + std::log1p (std::exp (-2.0f * a)) - kLn2;
}

// NOTE: polynomial approximations of tanh/logCosh are deliberately absent.
// This project's rule is that optimisation follows measurement, and the CPU
// measurement harness does not exist yet. Adding an approximation now would
// trade accuracy for an unquantified gain. Revisit once the benchmark lands.

} // namespace x10::dsp::math
