#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "x10/dsp/nonlinear/Adaa1.h"
#include "x10/dsp/nonlinear/Curves.h"

#include "support/Reference.h"
#include "support/Signals.h"

using namespace x10;
using namespace x10::dsp;
using namespace x10::test;

namespace
{

/**
    Drives ADAA with a real signal and compares every sample against the
    definition it is supposed to implement: the mean of f over the interval
    spanned by consecutive input samples.
*/
template <Curve C>
double worstDeviationFromDefinition (float amplitude, std::size_t bin, int steps)
{
    const auto input = binCentredSine (4096, bin, amplitude);

    Adaa1<C> adaa;
    adaa.reset();

    double worst = 0.0;
    float previous = 0.0f;

    for (float x : input)
    {
        const double produced = static_cast<double> (adaa.process (x));
        const double expected = integralMean<C> (static_cast<double> (previous),
                                                 static_cast<double> (x),
                                                 steps);
        worst    = std::max (worst, std::abs (produced - expected));
        previous = x;
    }

    return worst;
}

} // namespace

TEST_CASE ("ADAA reproduces the interval mean of the curve", "[adaa]")
{
    // Tolerances are set from measurement, not from taste. The smooth curves sit
    // near 1e-5; the wavefolder is looser only because the Simpson reference
    // itself degrades where the triangle kinks inside an interval.
    SECTION ("Tanh")
    {
        const double worst = worstDeviationFromDefinition<curves::Tanh> (4.0f, kGateToneBin, 4096);
        CAPTURE (worst);
        REQUIRE (worst < 1.0e-4);
    }

    SECTION ("Tube")
    {
        const double worst = worstDeviationFromDefinition<curves::Tube> (4.0f, kGateToneBin, 4096);
        CAPTURE (worst);
        REQUIRE (worst < 1.0e-4);
    }

    SECTION ("Transformer")
    {
        const double worst = worstDeviationFromDefinition<curves::Transformer> (4.0f, kGateToneBin, 4096);
        CAPTURE (worst);
        REQUIRE (worst < 1.0e-4);
    }

    SECTION ("Wavefolder")
    {
        const double worst = worstDeviationFromDefinition<curves::Wavefolder> (4.0f, kGateToneBin, 16384);
        CAPTURE (worst);
        REQUIRE (worst < 1.0e-3);
    }
}

TEST_CASE ("ADAA reduces to the curve evaluated half a sample late", "[adaa]")
{
    // The interval mean of a slowly varying signal is f at the interval midpoint,
    // so ADAA costs exactly half a sample of delay and nothing else. Comparing
    // against f(x[n]) instead would show a spurious error of dx/2 — measured at
    // 3.8e-4 for a 2.9 Hz tone — which is the delay, not an inaccuracy.
    //
    // Bin 32 keeps dx (~0.025) above kAdaaEps so the division path is the one
    // under test here, not the midpoint fallback.
    const auto input = binCentredSine (4096, 32, 2.0f);

    Adaa1<curves::Tanh> adaa;
    adaa.reset();

    double worst = 0.0;
    float previous = 0.0f;

    for (float x : input)
    {
        const double produced = static_cast<double> (adaa.process (x));
        const double midpoint = static_cast<double> (curves::Tanh::f (0.5f * (x + previous)));
        worst    = std::max (worst, std::abs (produced - midpoint));
        previous = x;
    }

    CAPTURE (worst);
    REQUIRE (worst < 1.0e-4);
}

TEST_CASE ("ADAA output stays bounded by the curve's own range", "[adaa]")
{
    // The interval mean of a bounded function cannot exceed that function's
    // bounds, so a sign or caching error in the quotient shows up here at once.
    //
    // The tolerance is not slack: driven to +/-12, log(cosh x) reaches ~11.3 and
    // its float ulp is ~1e-6, so differencing two such values leaves a relative
    // error of order 1e-5 in the quotient. Measured worst overshoot is 1.7e-5,
    // about -95 dBFS. That is the numerical floor of first-order ADAA at high
    // drive, and it is the reason the alias gate can never read better than
    // roughly -95 dB no matter how correct the curve is.
    const auto input = binCentredSine (8192, kGateToneBin, 12.0f);

    Adaa1<curves::Tanh> adaa;
    adaa.reset();

    double worstOvershoot = 0.0;

    for (float x : input)
    {
        const float y = adaa.process (x);
        REQUIRE (std::isfinite (y));
        worstOvershoot = std::max (worstOvershoot, static_cast<double> (std::abs (y)) - 1.0);
    }

    CAPTURE (worstOvershoot);
    REQUIRE (worstOvershoot < 1.0e-3);
}
