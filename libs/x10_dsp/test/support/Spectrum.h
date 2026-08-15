#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "support/Signals.h"

namespace x10::test
{

/**
    Power in a single DFT bin, via Goertzel.

    Goertzel rather than an FFT dependency: only a handful of bins are ever
    needed (DC plus the harmonics), and the project must stay free of
    third-party headers. Accumulation is in double regardless of sample type.
*/
[[nodiscard]] inline double goertzelPower (const float* x, std::size_t numSamples, std::size_t bin) noexcept
{
    const double w     = 2.0 * std::numbers::pi_v<double> * static_cast<double> (bin)
                           / static_cast<double> (numSamples);
    const double coeff = 2.0 * std::cos (w);

    double s1 = 0.0;
    double s2 = 0.0;

    for (std::size_t n = 0; n < numSamples; ++n)
    {
        const double s0 = static_cast<double> (x[n]) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

/** Energy of a real harmonic, summing the conjugate pair at bins j and N-j. */
[[nodiscard]] inline double harmonicPower (const float* x, std::size_t numSamples, std::size_t bin) noexcept
{
    return 2.0 * goertzelPower (x, numSamples, bin);
}

struct AliasReport
{
    double total       = 0.0;
    double dc          = 0.0;
    double fundamental = 0.0;
    double harmonics   = 0.0; // includes the fundamental
    double alias       = 0.0;

    [[nodiscard]] double nmrDb() const noexcept
    {
        return 10.0 * std::log10 (std::max (alias, 1e-30) / std::max (fundamental, 1e-30));
    }

    [[nodiscard]] double dcDb() const noexcept
    {
        return 10.0 * std::log10 (std::max (dc, 1e-30) / std::max (fundamental, 1e-30));
    }
};

/**
    Splits one analysis window into DC, harmonics and everything else.

    "Everything else" is alias energy, obtained by Parseval subtraction rather
    than by scanning for peaks. A peak search would miss a dense alias floor,
    which is exactly what a naive nonlinearity produces.
*/
[[nodiscard]] inline AliasReport analyseHarmonics (const std::vector<float>& block, std::size_t toneBin)
{
    const std::size_t n = block.size();
    const float* data   = block.data();

    AliasReport r;

    double sumOfSquares = 0.0;
    for (float s : block)
        sumOfSquares += static_cast<double> (s) * static_cast<double> (s);

    r.total       = sumOfSquares * static_cast<double> (n); // Parseval
    r.dc          = goertzelPower (data, n, 0);
    r.fundamental = harmonicPower (data, n, toneBin);

    for (std::size_t h = 1; h <= maxHarmonic (toneBin); ++h)
        r.harmonics += harmonicPower (data, n, h * toneBin);

    r.alias = std::max (r.total - r.dc - r.harmonics, 0.0);
    return r;
}

/**
    Drives a processor to steady state, then analyses one window of its output.
    The first window is discarded so the measurement never contains start-up
    transient energy, which would otherwise read as alias.
*/
template <class P>
[[nodiscard]] AliasReport measureAlias (P& processor, std::size_t toneBin, float amplitude)
{
    processor.reset();

    const auto warmUp = binCentredSine (kFftSize, toneBin, amplitude);
    for (float s : warmUp)
        (void) processor.process (s);

    auto block = binCentredSine (kFftSize, toneBin, amplitude);
    for (float& s : block)
        s = processor.process (s);

    return analyseHarmonics (block, toneBin);
}

} // namespace x10::test
