#include "BandLimitedInterpolator.h"

#include <algorithm>
#include <cmath>

namespace aod
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

double sinc (double x) noexcept
{
    if (std::abs (x) < 1.0e-9)
        return 1.0;

    const double px = kPi * x;
    return std::sin (px) / px;
}

/** Modified Bessel function of the first kind, order zero. */
double besselI0 (double x) noexcept
{
    double sum = 1.0;
    double term = 1.0;

    for (int k = 1; k < 32; ++k)
    {
        term *= (x * x) / (4.0 * static_cast<double> (k) * static_cast<double> (k));
        sum += term;
        if (term < sum * 1.0e-14)
            break;
    }

    return sum;
}

/**
    Kaiser window over a normalised [0, 1] position.

    Beta trades transition width against stopband depth. At this tap count the
    chosen value puts the stopband far enough down that a transposed sample's
    folded image stays inaudible, while the wanted partials remain flat.
*/
double kaiser (double position) noexcept
{
    constexpr double beta = 9.0;
    static const double normalisation = besselI0 (beta);

    const double centred = 2.0 * position - 1.0;
    const double radicand = std::max (0.0, 1.0 - centred * centred);
    return besselI0 (beta * std::sqrt (radicand)) / normalisation;
}

/** Playback rate represented by a bracket, used to pick the kernel cutoff. */
double rateForBracket (int bracket) noexcept
{
    if (bracket <= 0)
        return 1.0;

    const double normalised = static_cast<double> (bracket)
                            / static_cast<double> (BandLimitedInterpolator::kNumRateBrackets - 1);
    return std::pow (static_cast<double> (BandLimitedInterpolator::kMaxSupportedRate), normalised);
}
} // namespace

BandLimitedInterpolator::BandLimitedInterpolator()
{
    for (int bracket = 0; bracket < kNumRateBrackets; ++bracket)
    {
        // Reading the source faster than 1x moves its spectrum up, so the
        // kernel cutoff has to come down by the same factor to keep the result
        // below the output Nyquist. A small margin leaves room for the
        // transition band of a finite-length kernel.
        const double rate = rateForBracket (bracket);
        const double cutoff = bracket == 0 ? 1.0 : 0.9 / rate;

        // Keep a common window length for continuous rate interpolation.
        const int firstTap = bracket == 0 ? kUnityFirstTap : 0;
        const int tapCount = bracket == 0 ? kNumUnityTaps : kNumTaps;
        const double windowSpan = static_cast<double> (tapCount);

        for (int phase = 0; phase <= kNumPhases; ++phase)
        {
            const double fractional = static_cast<double> (phase) / static_cast<double> (kNumPhases);
            double sum = 0.0;
            std::array<double, kNumTaps> kernel {};

            for (int tap = 0; tap < kNumTaps; ++tap)
            {
                if (tap < firstTap || tap >= firstTap + tapCount)
                {
                    kernel[static_cast<std::size_t> (tap)] = 0.0;
                    continue;
                }

                const double offset = static_cast<double> (tap - kCentreTap) - fractional;
                const double windowPosition = (offset + windowSpan * 0.5) / windowSpan;
                const double value = cutoff * sinc (cutoff * offset) * kaiser (windowPosition);
                kernel[static_cast<std::size_t> (tap)] = value;
                sum += value;
            }

            // Normalising to unity DC gain keeps a steady signal at its
            // original level regardless of which kernel is selected.
            const double scale = std::abs (sum) > 1.0e-9 ? 1.0 / sum : 1.0;
            for (int tap = 0; tap < kNumTaps; ++tap)
            {
                const auto index = static_cast<std::size_t> ((bracket * (kNumPhases + 1) + phase) * kNumTaps + tap);
                table_[index] = static_cast<float> (kernel[static_cast<std::size_t> (tap)] * scale);
            }
        }
    }
}

const BandLimitedInterpolator& BandLimitedInterpolator::shared()
{
    // Function-local static: built once, then read concurrently by all voices.
    static const BandLimitedInterpolator instance;
    return instance;
}

int BandLimitedInterpolator::bracketForRate (double playbackRate) const noexcept
{
    if (!(playbackRate > 1.0))
        return 0;

    const double clamped = std::min (playbackRate, static_cast<double> (kMaxSupportedRate));
    const double normalised = std::log (clamped) / std::log (static_cast<double> (kMaxSupportedRate));
    // Round up so the selected kernel is never less band-limited than the rate
    // actually being played.
    const int bracket = static_cast<int> (std::ceil (normalised * static_cast<double> (kNumRateBrackets - 1)));
    return std::clamp (bracket, 0, kNumRateBrackets - 1);
}

float BandLimitedInterpolator::positionForRate (double playbackRate) const noexcept
{
    if (!(playbackRate > 1.0))
        return 0.0f;
    return static_cast<float> (std::log2 (std::min (playbackRate, static_cast<double> (kMaxSupportedRate)))
        * static_cast<double> (kNumRateBrackets - 1) / 4.0);
}

int BandLimitedInterpolator::phaseIndex (float fractionalPhase) noexcept
{
    const int phase = static_cast<int> (fractionalPhase * static_cast<float> (kNumPhases));
    return std::clamp (phase, 0, kNumPhases - 1);
}

const float* BandLimitedInterpolator::kernelAt (int bracket, int phase) const noexcept
{
    const int safeBracket = std::clamp (bracket, 0, kNumRateBrackets - 1);
    return table_.data() + static_cast<std::size_t> ((safeBracket * (kNumPhases + 1) + phase) * kNumTaps);
}

} // namespace aod
