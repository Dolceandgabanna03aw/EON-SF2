#pragma once

#include <algorithm>

#include "x10/dsp/Concepts.h"

namespace x10::dsp::naive
{

/**
    Deliberately bad processors, kept in the shipping headers on purpose.

    test_gate_selfcheck.cpp feeds these to the alias gate and requires that the
    gate rejects them. A detector that cannot fail is not a detector, so these
    types are the fixture that keeps the numeric gate honest — including against
    the specific failure mode of relaxing a threshold until something passes.

    They are never instantiated by the audio path.
*/

/** Hard clipper. Infinite harmonic series, aliases catastrophically. */
struct HardClip
{
    [[nodiscard]] float process (float x) const noexcept
    {
        return std::clamp (x, -1.0f, 1.0f);
    }

    void reset() noexcept {}
};

/** Direct per-sample evaluation of a curve, with no antialiasing whatsoever. */
template <Curve C>
struct Direct
{
    [[nodiscard]] float process (float x) const noexcept { return C::f (x); }

    void reset() noexcept {}
};

} // namespace x10::dsp::naive
