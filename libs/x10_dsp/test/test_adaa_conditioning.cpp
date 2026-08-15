#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "x10/dsp/Config.h"
#include "x10/dsp/nonlinear/Adaa1.h"
#include "x10/dsp/nonlinear/Curves.h"

#include "support/Reference.h"
#include "support/Signals.h"

using namespace x10;
using namespace x10::dsp;
using namespace x10::test;

namespace
{

/** Error of the raw division path — the one Adaa1 guards against. */
template <Curve C>
double divisionPathError (double x0, double dx)
{
    const float a = static_cast<float> (x0);
    const float b = static_cast<float> (x0 + dx);
    const double produced = static_cast<double> ((C::F1 (b) - C::F1 (a)) / (b - a));
    return std::abs (produced - integralMean<C> (static_cast<double> (a), static_cast<double> (b)));
}

/** Error of the midpoint fallback. */
template <Curve C>
double midpointPathError (double x0, double dx)
{
    const float a = static_cast<float> (x0);
    const float b = static_cast<float> (x0 + dx);
    const double produced = static_cast<double> (C::f (0.5f * (a + b)));
    return std::abs (produced - integralMean<C> (static_cast<double> (a), static_cast<double> (b)));
}

} // namespace

TEST_CASE ("the ill-conditioning guard is necessary", "[adaa][conditioning]")
{
    // Without the guard, small dx destroys the result. This is the failure the
    // guard exists for; if this test ever passes trivially, the measurement is
    // wrong, not the code.
    const double x0 = 0.37;

    for (double dx : { 1.0e-5, 1.0e-6, 1.0e-7 })
    {
        const double err = divisionPathError<curves::Tanh> (x0, dx);
        CAPTURE (dx, err);
        REQUIRE (err > 1.0e-3);
    }

    // At dx = 1e-8 in float the subtraction underflows to zero difference and the
    // quotient is not even finite.
    REQUIRE_FALSE (std::isfinite (divisionPathError<curves::Tanh> (x0, 1.0e-8)));
}

TEST_CASE ("the guard is not over-applied", "[adaa][conditioning]")
{
    // Above the threshold the division path must be the accurate one, otherwise
    // kAdaaEps is too large and the antialiasing is being thrown away.
    const double x0 = 0.37;

    for (double dx : { 1.0e-1, 5.0e-2, 2.0e-2 })
    {
        const double err = divisionPathError<curves::Tanh> (x0, dx);
        CAPTURE (dx, err);
        REQUIRE (err < 1.0e-5);
    }
}

TEST_CASE ("kAdaaEps sits inside the measured crossover band", "[adaa][conditioning]")
{
    // Guards the constant itself. Measured crossovers span roughly 1e-3 to 7e-2
    // across the four curves and |x| up to 8; anything outside that band means
    // one of the two paths is being used where the other is more accurate.
    REQUIRE (kAdaaEps >= 1.0e-3f);
    REQUIRE (kAdaaEps <= 1.0e-1f);

    // Below the threshold the midpoint must actually be the better choice.
    const double x0 = 0.37;
    const double dx = static_cast<double> (kAdaaEps) * 0.1;

    const double division = divisionPathError<curves::Tanh> (x0, dx);
    const double midpoint = midpointPathError<curves::Tanh> (x0, dx);

    CAPTURE (kAdaaEps, dx, division, midpoint);
    REQUIRE (midpoint < division);
}

TEST_CASE ("low-level signals never produce NaN or Inf", "[adaa][conditioning]")
{
    // The plan's M3 criterion: one million samples of very quiet noise, no
    // non-finite output. Quiet noise is the worst case because dx is small on
    // every sample, not just near peaks.
    constexpr std::size_t kNumSamples = 1'000'000;

    auto sweepAmplitude = [] (float amplitude)
    {
        const auto noise = lowLevelNoise (kNumSamples, amplitude);

        Adaa1<curves::Tanh> tanhStage;
        Adaa1<curves::Tube> tubeStage;
        Adaa1<curves::Transformer> transformerStage;
        Adaa1<curves::Wavefolder> folderStage;

        tanhStage.reset();
        tubeStage.reset();
        transformerStage.reset();
        folderStage.reset();

        std::size_t nonFinite = 0;
        for (float x : noise)
        {
            nonFinite += static_cast<std::size_t> (! std::isfinite (tanhStage.process (x)));
            nonFinite += static_cast<std::size_t> (! std::isfinite (tubeStage.process (x)));
            nonFinite += static_cast<std::size_t> (! std::isfinite (transformerStage.process (x)));
            nonFinite += static_cast<std::size_t> (! std::isfinite (folderStage.process (x)));
        }
        return nonFinite;
    };

    for (float amplitude : { 1.0e-3f, 1.0e-6f, 1.0e-12f, 1.0e-25f })
    {
        CAPTURE (amplitude);
        REQUIRE (sweepAmplitude (amplitude) == 0u);
    }
}

TEST_CASE ("a constant input produces a constant output", "[adaa][conditioning]")
{
    // dx is exactly zero here, the degenerate case. The output must settle on
    // f(x), not on 0/0.
    Adaa1<curves::Transformer> adaa;
    adaa.reset();

    constexpr float kLevel = 0.6f;
    float last = 0.0f;
    for (int i = 0; i < 512; ++i)
        last = adaa.process (kLevel);

    CAPTURE (last);
    REQUIRE (std::isfinite (last));
    REQUIRE (std::abs (last - curves::Transformer::f (kLevel)) < 1.0e-5f);
}
