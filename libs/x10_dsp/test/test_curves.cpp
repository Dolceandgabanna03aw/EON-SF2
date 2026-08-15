#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "x10/dsp/nonlinear/Curves.h"

#include "support/Reference.h"

using namespace x10;
using namespace x10::dsp;
using namespace x10::test;

using AllCurves = std::tuple<curves::Tanh, curves::Tube, curves::Transformer, curves::Wavefolder>;

TEMPLATE_LIST_TEST_CASE ("curve passes through the origin", "[curves]", AllCurves)
{
    REQUIRE (std::abs (TestType::f (0.0f)) < 1.0e-7f);
}

TEMPLATE_LIST_TEST_CASE ("curve and antiderivative are finite over the working range",
                         "[curves]", AllCurves)
{
    for (int i = -800; i <= 800; ++i)
    {
        const float x = static_cast<float> (i) * 0.01f;
        CAPTURE (x);
        REQUIRE (std::isfinite (TestType::f (x)));
        REQUIRE (std::isfinite (TestType::F1 (x)));
    }
}

TEMPLATE_LIST_TEST_CASE ("F1 is continuous across every piecewise join", "[curves]", AllCurves)
{
    // A jump here does not detune anything subtly — it puts a spike in the ADAA
    // output the moment the signal crosses the join, because the numerator of
    // the difference quotient inherits the discontinuity. Piecewise curves that
    // forget to match their integration constants fail exactly here.
    constexpr float h = 1.0e-3f;

    for (float bp : TestType::breakpoints)
    {
        const float below = TestType::F1 (bp - h);
        const float above = TestType::F1 (bp + h);

        // Over a 2h window the antiderivative can legitimately move by at most
        // 2h * max|f|; anything beyond that is a discontinuity, not a slope.
        const float slopeBound = 2.0f * h * 2.0f + 1.0e-5f;

        CAPTURE (bp, below, above);
        REQUIRE (std::abs (above - below) < slopeBound);
    }
}

TEMPLATE_LIST_TEST_CASE ("F1 really is the antiderivative of f", "[curves]", AllCurves)
{
    // Checked by integration rather than differentiation: a numerical derivative
    // of a float-valued F1 is dominated by rounding, while the integral of f is
    // stable. Intervals deliberately straddle the piecewise joins.
    double worst = 0.0;

    for (int i = -16; i < 16; ++i)
    {
        const double a = static_cast<double> (i) * 0.5;
        const double b = a + 0.5;

        const double fromF1 = static_cast<double> (TestType::F1 (static_cast<float> (b)))
                            - static_cast<double> (TestType::F1 (static_cast<float> (a)));
        const double fromIntegral = definiteIntegral<TestType> (a, b, 8192);

        worst = std::max (worst, std::abs (fromF1 - fromIntegral));
    }

    CAPTURE (worst);
    REQUIRE (worst < 1.0e-4);
}

TEST_CASE ("Tanh is odd-symmetric", "[curves]")
{
    for (int i = 1; i <= 400; ++i)
    {
        const float x = static_cast<float> (i) * 0.02f;
        CAPTURE (x);
        REQUIRE (std::abs (curves::Tanh::f (x) + curves::Tanh::f (-x)) < 1.0e-6f);
    }
}

TEST_CASE ("asymmetric curves are actually asymmetric", "[curves]")
{
    // Guards against a copy-paste that leaves both halves using the same gain,
    // which would silently turn Tube and Transformer into duplicates of a
    // symmetric curve and remove the even harmonics they exist to produce.
    SECTION ("Tube")
    {
        const float x = 1.0f;
        REQUIRE (std::abs (curves::Tube::f (x) + curves::Tube::f (-x)) > 0.05f);
    }

    SECTION ("Transformer")
    {
        const float x = 1.0f;
        REQUIRE (std::abs (curves::Transformer::f (x) + curves::Transformer::f (-x)) > 0.05f);
    }
}

TEST_CASE ("saturating curves are monotonic and bounded", "[curves]")
{
    auto checkMonotonic = [] (auto curve, float bound)
    {
        using C = decltype (curve);
        float previous = C::f (-8.0f);

        for (int i = -799; i <= 800; ++i)
        {
            const float x = static_cast<float> (i) * 0.01f;
            const float y = C::f (x);
            CAPTURE (x, y, previous);
            REQUIRE (y >= previous - 1.0e-7f);
            REQUIRE (std::abs (y) <= bound);
            previous = y;
        }
    };

    checkMonotonic (curves::Tanh{}, 1.0f);
    checkMonotonic (curves::Tube{}, 1.0f / curves::Tube::kPos + 1.0e-5f);
    checkMonotonic (curves::Transformer{}, 1.0f / curves::Transformer::kPos + 1.0e-5f);
}

TEST_CASE ("Wavefolder folds with the expected period and turning points", "[curves]")
{
    REQUIRE (std::abs (curves::Wavefolder::f (0.0f) - 0.0f) < 1.0e-6f);
    REQUIRE (std::abs (curves::Wavefolder::f (1.0f) - 1.0f) < 1.0e-6f);
    REQUIRE (std::abs (curves::Wavefolder::f (2.0f) - 0.0f) < 1.0e-6f);
    REQUIRE (std::abs (curves::Wavefolder::f (3.0f) + 1.0f) < 1.0e-6f);
    REQUIRE (std::abs (curves::Wavefolder::f (-1.0f) + 1.0f) < 1.0e-6f);

    // Period 4.
    for (int i = -20; i <= 20; ++i)
    {
        const float x = static_cast<float> (i) * 0.37f;
        CAPTURE (x);
        REQUIRE (std::abs (curves::Wavefolder::f (x) - curves::Wavefolder::f (x + 4.0f)) < 1.0e-5f);
    }

    // Never exceeds the fold limits, however hard it is driven.
    for (int i = -2000; i <= 2000; ++i)
    {
        const float x = static_cast<float> (i) * 0.05f;
        REQUIRE (std::abs (curves::Wavefolder::f (x)) <= 1.0f + 1.0e-5f);
    }
}
