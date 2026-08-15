#pragma once

#include <cmath>

#include "x10/dsp/Concepts.h"

namespace x10::test
{

/**
    Mean value of f over [a, b], by composite Simpson in double precision.

    This is the definition first-order ADAA approximates:

        y[n] = (1/dx) * integral from x[n-1] to x[n] of f(u) du

    Testing against this rather than against a second copy of the
    (F1(b) - F1(a)) / (b - a) formula is deliberate. It validates two things at
    once — that F1 really is the antiderivative of f, and that the quotient is
    assembled correctly — and a transcription error in F1 cannot cancel out.
*/
template <dsp::Curve C>
[[nodiscard]] inline double integralMean (double a, double b, int steps = 4096)
{
    if (std::abs (b - a) < 1e-14)
        return static_cast<double> (C::f (static_cast<float> (0.5 * (a + b))));

    const double h = (b - a) / steps;

    double sum = static_cast<double> (C::f (static_cast<float> (a)))
               + static_cast<double> (C::f (static_cast<float> (b)));

    for (int i = 1; i < steps; ++i)
    {
        const double x = a + h * static_cast<double> (i);
        sum += (i % 2 ? 4.0 : 2.0) * static_cast<double> (C::f (static_cast<float> (x)));
    }

    return (sum * h / 3.0) / (b - a);
}

/** Definite integral of f over [a, b], for checking F1 differences. */
template <dsp::Curve C>
[[nodiscard]] inline double definiteIntegral (double a, double b, int steps = 4096)
{
    return integralMean<C> (a, b, steps) * (b - a);
}

} // namespace x10::test
