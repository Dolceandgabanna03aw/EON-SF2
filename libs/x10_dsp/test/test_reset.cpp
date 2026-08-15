#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <tuple>
#include <vector>

#include "x10/dsp/Concepts.h"
#include "x10/dsp/filter/DCBlocker.h"
#include "x10/dsp/filter/TptSvf.h"
#include "x10/dsp/nonlinear/Adaa1.h"
#include "x10/dsp/nonlinear/Curves.h"

#include "support/Signals.h"

using namespace x10;
using namespace x10::dsp;
using namespace x10::test;

using AllCurves = std::tuple<curves::Tanh, curves::Tube, curves::Transformer, curves::Wavefolder>;

TEMPLATE_LIST_TEST_CASE ("reset makes a voice bit-for-bit reusable", "[reset]", AllCurves)
{
    // A voice that has just been stolen must behave exactly like a fresh one.
    // Anything less and the same note sounds different depending on what the
    // voice was doing before, which is untraceable in practice.
    const auto input = binCentredSine (2048, kGateToneBin, 3.0f);

    Adaa1<TestType> stage;

    stage.reset();
    std::vector<float> first (input.begin(), input.end());
    stage.processBlock (first.data(), first.size());

    // Dirty the state with something completely different.
    const auto interference = binCentredSine (777, 4097, 9.0f);
    for (float s : interference)
        (void) stage.process (s);

    stage.reset();
    std::vector<float> second (input.begin(), input.end());
    stage.processBlock (second.data(), second.size());

    REQUIRE (std::memcmp (first.data(), second.data(), first.size() * sizeof (float)) == 0);
}

TEMPLATE_LIST_TEST_CASE ("no transient on the first sample after reset", "[reset]", AllCurves)
{
    // The failure this catches is an impulse at the start of every stolen voice,
    // which the downstream nonlinearity then smears into broadband splatter.
    Adaa1<TestType> stage;

    // Drive it hard first, so any leftover history would be large.
    const auto loud = binCentredSine (1024, kGateToneBin, 10.0f);
    for (float s : loud)
        (void) stage.process (s);

    stage.reset();

    const float firstOutput = stage.process (0.0f);
    CAPTURE (firstOutput);
    REQUIRE (std::abs (firstOutput) < 1.0e-6f);
}

namespace
{

/**
    Tanh with a deliberately shifted antiderivative.

    F1 is an antiderivative, so adding a constant changes nothing mathematically:
    ADAA only ever uses differences of F1. The one place the constant can leak in
    is the reset value of the cached F1(x[n-1]). Setting that cache to 0 instead
    of F1(0) puts a spike of size shift/dx on the first sample.

    tanh alone cannot catch that bug, because log(cosh 0) happens to be 0. This
    curve exists purely to make the bug observable.
*/
struct ShiftedTanh
{
    static constexpr float kShift = 100.0f;
    static constexpr std::array<float, 0> breakpoints {};

    [[nodiscard]] static float f (float x) noexcept { return curves::Tanh::f (x); }
    [[nodiscard]] static float F1 (float x) noexcept { return curves::Tanh::F1 (x) + kShift; }
};

} // namespace

TEST_CASE ("the reset value of the F1 cache is F1(0), not zero", "[reset]")
{
    // The first sample after reset must be a large step, and this is not a
    // detail. An earlier version of this test drove a bin-centred sine, whose
    // first sample is exactly 0: dx was then 0, the midpoint fallback ran, the
    // corrupted cache was never read, and the test passed against a knowingly
    // broken reset(). A voice steal restarts playback at an arbitrary point in
    // the sample, so a nonzero first sample is also the realistic case.
    constexpr float kFirstSample = 3.0f;

    Adaa1<curves::Tanh> plain;
    Adaa1<ShiftedTanh> shifted;

    // Dirty both, so reset() has something real to clear.
    for (float s : binCentredSine (512, kGateToneBin, 6.0f))
    {
        (void) plain.process (s);
        (void) shifted.process (s);
    }

    plain.reset();
    shifted.reset();

    const double plainFirst   = static_cast<double> (plain.process (kFirstSample));
    const double shiftedFirst = static_cast<double> (shifted.process (kFirstSample));

    // Shifting F1 by a constant cannot change the output: ADAA only ever uses
    // differences of F1. A cache reset to 0 instead of F1(0) leaks the shift in
    // as an error of kShift/dx, here about 33.
    CAPTURE (plainFirst, shiftedFirst);
    REQUIRE (std::abs (plainFirst - shiftedFirst) < 1.0e-3);

    // And the rest of the block must agree too.
    double worst = 0.0;
    for (float x : binCentredSine (256, kGateToneBin, 3.0f))
        worst = std::max (worst,
                          std::abs (static_cast<double> (plain.process (x))
                                    - static_cast<double> (shifted.process (x))));

    CAPTURE (worst);
    REQUIRE (worst < 1.0e-3);
}

TEST_CASE ("resetAll clears every member of a state tuple", "[reset]")
{
    // The point of the helper is that adding a member cannot be forgotten here.
    // If a member is added to this tuple and it is not Resettable, the code below
    // does not compile — which is the intended failure mode.
    std::tuple<Adaa1<curves::Tube>, DCBlocker, TptSvf> stage;

    std::get<1> (stage).prepare (kSampleRate);
    std::get<2> (stage).prepare (kSampleRate);
    std::get<2> (stage).setCutoff (2000.0f, 0.7071068f);

    const auto loud = binCentredSine (1024, kGateToneBin, 8.0f);
    for (float s : loud)
    {
        const float a = std::get<0> (stage).process (s);
        const float b = std::get<1> (stage).process (a);
        (void) std::get<2> (stage).process (b);
    }

    resetAll (stage);

    // With every stage cleared, silence in must give silence out immediately.
    for (int i = 0; i < 16; ++i)
    {
        const float a = std::get<0> (stage).process (0.0f);
        const float b = std::get<1> (stage).process (a);
        const float c = std::get<2> (stage).process (b);
        CAPTURE (i, c);
        REQUIRE (std::abs (c) < 1.0e-9f);
    }
}

TEST_CASE ("library state types satisfy the Resettable concept", "[reset]")
{
    STATIC_REQUIRE (Resettable<Adaa1<curves::Tanh>>);
    STATIC_REQUIRE (Resettable<DCBlocker>);
    STATIC_REQUIRE (Resettable<TptSvf>);

    STATIC_REQUIRE (Processor<Adaa1<curves::Wavefolder>>);
    STATIC_REQUIRE (Processor<DCBlocker>);
    STATIC_REQUIRE (Processor<TptSvf>);
}
