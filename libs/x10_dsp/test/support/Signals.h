#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace x10::test
{

// ---------------------------------------------------------------------------
// Test tone selection
//
// Alias energy must never land on a harmonic bin, or the measurement scores
// aliasing as harmonic content and reports success. Two properties give that:
//
//   1. The tone sits exactly on a transform bin, so it is periodic in the
//      analysis window and leaks nothing. No window function needed.
//
//   2. Every bin used here is odd, and 16384 = 2^14, so bin and window size are
//      coprime. Therefore h1*bin == +/- h2*bin (mod 16384) forces h1 == h2, and
//      no folded alias can ever coincide with a harmonic bin.
//
// Harmonics below Nyquist are real output; everything else is folded energy and
// is counted as alias.
// ---------------------------------------------------------------------------

inline constexpr std::size_t kFftSize    = 16384;
inline constexpr double      kSampleRate = 48000.0;

/**
    Gate operating point, chosen by measurement (see PROVENANCE.md).

    3999 Hz at amplitude 4.0 is where all four curves show their tightest cluster
    of ADAA improvement (6.8-8.6 dB). Lower tones are unusable as a gate: below
    roughly 2 kHz a naive curve barely aliases at all, so ADAA's own error floor
    dominates the ratio and a correct implementation scores *worse* than naive.
*/
inline constexpr std::size_t kGateToneBin   = 1365; // 3999.0 Hz
inline constexpr float       kGateAmplitude = 4.0f;

[[nodiscard]] inline constexpr double toneFrequencyHz (std::size_t bin) noexcept
{
    return kSampleRate * static_cast<double> (bin) / static_cast<double> (kFftSize);
}

/** Highest harmonic of `bin` that stays below Nyquist. */
[[nodiscard]] inline constexpr std::size_t maxHarmonic (std::size_t bin) noexcept
{
    return (kFftSize / 2 - 1) / bin;
}

/** Sine locked to a transform bin, so that it is exactly window-periodic. */
[[nodiscard]] inline std::vector<float> binCentredSine (std::size_t numSamples,
                                                        std::size_t bin,
                                                        float amplitude)
{
    std::vector<float> out (numSamples);
    const double w = 2.0 * std::numbers::pi_v<double> * static_cast<double> (bin)
                       / static_cast<double> (kFftSize);

    for (std::size_t n = 0; n < numSamples; ++n)
        out[n] = amplitude * static_cast<float> (std::sin (w * static_cast<double> (n)));

    return out;
}

/** Deterministic PRNG — reproducible across runs and platforms. */
class Xorshift32
{
public:
    explicit Xorshift32 (std::uint32_t seed = 0x9E3779B9u) noexcept : state_ (seed) {}

    [[nodiscard]] std::uint32_t next() noexcept
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

    /** Uniform in [-1, 1). */
    [[nodiscard]] float bipolar() noexcept
    {
        return static_cast<float> (next()) * (2.0f / 4294967296.0f) - 1.0f;
    }

private:
    std::uint32_t state_;
};

/** Very quiet noise — the regime where the ADAA difference quotient degenerates. */
[[nodiscard]] inline std::vector<float> lowLevelNoise (std::size_t numSamples, float amplitude)
{
    Xorshift32 rng;
    std::vector<float> out (numSamples);

    for (auto& s : out)
        s = amplitude * rng.bipolar();

    return out;
}

/** Logarithmic sine sweep, for broadband alias inspection. */
[[nodiscard]] inline std::vector<float> logSweep (std::size_t numSamples,
                                                  double startHz,
                                                  double endHz,
                                                  float amplitude)
{
    std::vector<float> out (numSamples);
    const double duration = static_cast<double> (numSamples) / kSampleRate;
    const double k        = std::log (endHz / startHz);

    for (std::size_t n = 0; n < numSamples; ++n)
    {
        const double t     = static_cast<double> (n) / kSampleRate;
        const double phase = 2.0 * std::numbers::pi_v<double> * startHz * duration / k
                               * (std::exp (t * k / duration) - 1.0);
        out[n] = amplitude * static_cast<float> (std::sin (phase));
    }

    return out;
}

} // namespace x10::test
