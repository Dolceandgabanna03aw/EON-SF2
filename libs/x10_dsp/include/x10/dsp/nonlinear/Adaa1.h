#pragma once

#include <cmath>
#include <cstddef>

#include "x10/dsp/Concepts.h"
#include "x10/dsp/Config.h"

namespace x10::dsp
{

/**
    First-order antiderivative antialiasing.

        y[n] = ( F1(x[n]) - F1(x[n-1]) ) / ( x[n] - x[n-1] ),   F1' = f

    Two details carry the whole correctness of this class.

    1. The difference quotient is 0/0 as dx -> 0. The midpoint fallback below is
       the only guard against that, and it lives here rather than in each curve
       so that it cannot be omitted by a new curve. Without it, low-level signals
       produce sporadic NaN.

    2. F1(x[n]) is cached into the next sample's F1(x[n-1]). This halves the
       transcendental count to one evaluation per sample — at 48 kHz and 128
       voices that is 3.05M/s rather than 6.1M/s.

    Note the reset value: F1x1_ starts at F1(0), NOT at 0. The antiderivative has
    a free additive constant, and a curve whose F1(0) is nonzero would otherwise
    emit a spike on the first sample after a voice steal. tanh happens to satisfy
    log(cosh 0) = 0, which is exactly why this bug survives casual testing.
*/
template <Curve C>
class Adaa1
{
public:
    [[nodiscard]] float process (float x) noexcept
    {
        const float F1x = C::F1 (x);
        const float dx  = x - x1_;

        const float y = (std::abs (dx) < kAdaaEps)
                          ? C::f (0.5f * (x + x1_))
                          : (F1x - F1x1_) / dx;

        x1_   = x;
        F1x1_ = F1x;
        return y;
    }

    void processBlock (float* data, std::size_t numSamples) noexcept
    {
        for (std::size_t i = 0; i < numSamples; ++i)
            data[i] = process (data[i]);
    }

    void reset() noexcept
    {
        x1_   = 0.0f;
        F1x1_ = C::F1 (0.0f);
    }

private:
    float x1_   = 0.0f;
    float F1x1_ = C::F1 (0.0f);
};

} // namespace x10::dsp
