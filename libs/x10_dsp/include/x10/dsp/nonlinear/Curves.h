#pragma once

#include <array>
#include <cmath>

#include "x10/dsp/math/FastMath.h"

namespace x10::dsp::curves
{

/**
    Symmetric hyperbolic tangent — the default voice curve.

        f(x)  = tanh(x)
        F1(x) = log(cosh(x))
*/
struct Tanh
{
    static constexpr std::array<float, 0> breakpoints {};

    [[nodiscard]] static float f (float x) noexcept { return std::tanh (x); }
    [[nodiscard]] static float F1 (float x) noexcept { return math::logCosh (x); }
};

/**
    Asymmetric tube-flavoured soft clip.

    The negative half saturates sooner than the positive half, imitating grid
    cutoff in a triode stage. Both halves reuse tanh with a different gain:

        f(x)  = tanh(k x) / k          k = kPos for x >= 0, kNeg otherwise
        F1(x) = log(cosh(k x)) / k^2

    The join at x = 0 is well behaved by construction: f, f' and F1 all evaluate
    to the same value from either side (0, 1 and 0 respectively), so no integration
    constant has to be carried. test_curves.cpp asserts this rather than assuming it.
*/
struct Tube
{
    static constexpr float kPos = 0.8f;
    static constexpr float kNeg = 1.6f;
    static constexpr std::array<float, 1> breakpoints { 0.0f };

    [[nodiscard]] static float f (float x) noexcept
    {
        const float k = (x >= 0.0f) ? kPos : kNeg;
        return std::tanh (k * x) / k;
    }

    [[nodiscard]] static float F1 (float x) noexcept
    {
        const float k = (x >= 0.0f) ? kPos : kNeg;
        return math::logCosh (k * x) / (k * k);
    }
};

/**
    Asymmetric algebraic soft clip standing in for transformer core saturation.

        f(x)  = x / (1 + k|x|)
        F1(x) = ( k|x| - log(1 + k|x|) ) / k^2

    Only the memoryless core lives here. The low-frequency harmonic emphasis that
    completes the transformer character is a filter with memory and cannot be
    expressed as a static curve, so the bus stage composes it around this (plan D6).
*/
struct Transformer
{
    static constexpr float kPos = 1.0f;
    static constexpr float kNeg = 1.7f;
    static constexpr std::array<float, 1> breakpoints { 0.0f };

    [[nodiscard]] static float f (float x) noexcept
    {
        const float k = (x >= 0.0f) ? kPos : kNeg;
        return x / (1.0f + k * std::abs (x));
    }

    [[nodiscard]] static float F1 (float x) noexcept
    {
        const float k = (x >= 0.0f) ? kPos : kNeg;
        const float a = k * std::abs (x);
        return (a - std::log1p (a)) / (k * k);
    }
};

/**
    Triangular wavefolder, period 4, folding at every odd integer.

        f(x)  = triangle wave through (0,0), (1,1), (3,-1)
        F1(x) = piecewise quadratic, period 4

    F1 is genuinely periodic here because the triangle has zero mean over a
    period, so both f and F1 can be evaluated from a single wrapped coordinate.

    This is the harshest test the ADAA framework gets: the derivative is
    discontinuous at every fold, which is why naive folding aliases so badly.
    The plan forbids shipping it without ADAA.
*/
struct Wavefolder
{
    // Fold points inside the [-8, 8] test range used by test_curves.cpp.
    static constexpr std::array<float, 8> breakpoints {
        -7.0f, -5.0f, -3.0f, -1.0f, 1.0f, 3.0f, 5.0f, 7.0f
    };

    [[nodiscard]] static float f (float x) noexcept
    {
        const float u = wrap (x);
        return (u <= 1.0f) ? u : (2.0f - u);
    }

    [[nodiscard]] static float F1 (float x) noexcept
    {
        const float u = wrap (x);
        return (u <= 1.0f) ? (0.5f * u * u)
                           : (2.0f * u - 0.5f * u * u - 1.0f);
    }

private:
    /** Maps x into the fundamental interval [-1, 3). */
    [[nodiscard]] static float wrap (float x) noexcept
    {
        return x - 4.0f * std::floor ((x + 1.0f) * 0.25f);
    }
};

} // namespace x10::dsp::curves
