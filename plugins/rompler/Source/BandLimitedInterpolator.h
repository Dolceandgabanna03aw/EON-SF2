#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace aod
{
/**
    Shared, read-only polyphase sinc table used by every voice to read a sample
    at an arbitrary playback rate.

    A linear interpolator is cheap but behaves like a poor low-pass filter: it
    dulls the passband and, more importantly, lets content above Nyquist fold
    back into the audible band whenever a sample is transposed upwards. This
    table stores a windowed sinc kernel per fractional phase, and one set of
    kernels per playback-rate bracket, so a voice reading faster than 1x also
    band-limits the source as it goes.

    Every kernel is built once, on the message thread, by the singleton
    accessor. The audio thread only indexes into immutable memory: no
    allocation or locks. Rate coordinates are cached when pitch is stationary.
*/
class BandLimitedInterpolator
{
public:
    static constexpr int kNumTaps = 48;
    /** All fractional reads use the full kernel; integer unity reads are direct. */
    static constexpr int kNumUnityTaps = kNumTaps;
    static constexpr int kNumPhases = 256;
    /** Playback-rate brackets: index 0 is <= 1x, the rest cover faster reads. */
    static constexpr int kNumRateBrackets = 33;
    /** Highest transposition the table band-limits for (4 octaves up). */
    static constexpr float kMaxSupportedRate = 16.0f;

    /** Builds the coefficient table on first use; never call from audio code. */
    static const BandLimitedInterpolator& shared();

    /** Maps a playback rate in source frames per output sample to a kernel set. */
    [[nodiscard]] int bracketForRate (double playbackRate) const noexcept;

    /** Continuous table coordinate; prepare once when a block has fixed pitch. */
    [[nodiscard]] float positionForRate (double playbackRate) const noexcept;

    template <typename ReadSample>
    [[nodiscard]] float interpolateRate (float position, float fractionalPhase, ReadSample&& readAt) const noexcept
    {
        position = std::clamp (position, 0.0f, static_cast<float> (kNumRateBrackets - 1));
        if (position <= 0.0f && fractionalPhase <= 0.0f)
            return readAt (0);

        const int lower = static_cast<int> (position);
        const int upper = std::min (lower + 1, kNumRateBrackets - 1);
        const float rateMix = position - static_cast<float> (lower);
        const float phasePosition = std::clamp (fractionalPhase, 0.0f, 1.0f) * static_cast<float> (kNumPhases);
        const int phase = std::min (static_cast<int> (phasePosition), kNumPhases - 1);
        const float phaseMix = phasePosition - static_cast<float> (phase);
        const float* a = kernelAt (lower, phase);
        const float* b = kernelAt (lower, phase + 1);
        const float* c = kernelAt (upper, phase);
        const float* d = kernelAt (upper, phase + 1);
        float sum = 0.0f;
        for (int tap = 0; tap < kNumTaps; ++tap)
        {
            const float low = a[tap] + phaseMix * (b[tap] - a[tap]);
            const float high = c[tap] + phaseMix * (d[tap] - c[tap]);
            sum += (low + rateMix * (high - low)) * readAt (tap - kCentreTap);
        }
        return sum;
    }

    /**
        Reads one output sample.

        readAt supplies the source value for a tap offset, so callers keep
        ownership of edge and loop-wrap behaviour.
    */
    template <typename ReadSample>
    [[nodiscard]] float interpolate (int bracket, float fractionalPhase, ReadSample&& readAt) const noexcept
    {
        const int phase = phaseIndex (fractionalPhase);
        const float* kernel = kernelAt (bracket, phase);

        // The integer-bracket accessor is retained for kernel diagnostics.
        const int first = bracket == 0 ? kUnityFirstTap : 0;
        const int last = bracket == 0 ? kUnityFirstTap + kNumUnityTaps : kNumTaps;

        float sum = 0.0f;
        for (int tap = first; tap < last; ++tap)
            sum += kernel[tap] * readAt (tap - kCentreTap);

        return sum;
    }

    /** Taps span [-kCentreTap, kNumTaps - kCentreTap - 1] around the read index. */
    static constexpr int kCentreTap = kNumTaps / 2 - 1;
    /** First evaluated tap of the short, unity-rate kernel. */
    static constexpr int kUnityFirstTap = kNumTaps / 2 - kNumUnityTaps / 2;

private:
    BandLimitedInterpolator();

    [[nodiscard]] static int phaseIndex (float fractionalPhase) noexcept;
    [[nodiscard]] const float* kernelAt (int bracket, int phase) const noexcept;

    std::array<float, static_cast<std::size_t> (kNumRateBrackets) * (kNumPhases + 1) * kNumTaps> table_ {};
};

} // namespace aod
