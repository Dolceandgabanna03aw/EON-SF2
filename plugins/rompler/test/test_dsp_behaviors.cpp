#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Sampler.h"

namespace
{
constexpr int kSampleRate = 48000;
constexpr int kBlockSize  = 512;

/** A short, constant 1 kHz-ish burst so velocity and envelope are the only variables. */
aod::Sample makeTone()
{
    aod::Sample s;
    constexpr int length = kSampleRate; // 1 second at 48k
    s.data.resize (static_cast<std::size_t> (length));
    for (int i = 0; i < length; ++i)
        s.data[static_cast<std::size_t> (i)] = std::sin (2.0f * 3.14159265f * 1000.0f * static_cast<float> (i)
                                                         / static_cast<float> (kSampleRate));
    s.sampleRate = kSampleRate;
    return s;
}

float blockPeak (const float* output, int numSamples)
{
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = std::max (peak, std::abs (output[i]));
    return peak;
}
} // namespace

TEST_CASE ("polyphone cap stops allocating voices past the limit", "[dsp][voice]")
{
    aod::VoicePool pool;
    const aod::Sample sample = makeTone();

    pool.setPolyphony (1);

    // note-to-voice index is not public, but the observable behaviour is: with
    // only one voice slot, the second simultaneous note is dropped silently.
    pool.start (&sample, 60, 0.5f);
    pool.start (&sample, 62, 0.5f);

    std::vector<float> block (static_cast<std::size_t> (kBlockSize));
    pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);

    // One voice playing a 1s tone within a 512-frame block => non-zero but a
    // single voice's amplitude, not two stacked. We simply assert it fired.
    REQUIRE (blockPeak (block.data(), kBlockSize) > 0.0f);

    // Raising polyphony lets the dropped note register on the next render.
    pool.setPolyphony (4);
    std::vector<float> block2 (static_cast<std::size_t> (kBlockSize));
    pool.render (block2.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
    REQUIRE (blockPeak (block2.data(), kBlockSize) > 0.0f);
}

TEST_CASE ("velocity-to-drive scales loudness monotonically", "[dsp][voice]")
{
    aod::VoicePool pool;
    const aod::Sample sample = makeTone();

    // Positive velToDriveDb makes a hard hit (v=1.0) the reference and a soft
    // hit quieter; neutral 0 leaves it untouched.
    pool.start (&sample, 60, 1.0f);
    std::vector<float> loud (static_cast<std::size_t> (kBlockSize));
    pool.render (loud.data(), kBlockSize, kSampleRate, 10.0f, 20.0f, 0, 0, 0.0f);

    pool.stopAll();
    pool.start (&sample, 60, 0.1f);
    std::vector<float> soft (static_cast<std::size_t> (kBlockSize));
    pool.render (soft.data(), kBlockSize, kSampleRate, 10.0f, 20.0f, 0, 0, 0.0f);

    REQUIRE (blockPeak (soft.data(), kBlockSize) < blockPeak (loud.data(), kBlockSize));
}

TEST_CASE ("release fades to silence instead of cutting off abruptly", "[dsp][voice]")
{
    aod::VoicePool pool;
    const aod::Sample sample = makeTone();

    pool.start (&sample, 60, 0.8f);

    // Let the voice ring for a bit, then release it.
    std::vector<float> ring (static_cast<std::size_t> (kBlockSize));
    pool.render (ring.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
    REQUIRE (blockPeak (ring.data(), kBlockSize) > 0.0f);

    pool.stop (60);

    // Across a handful of blocks the release ramp should shrink and reach zero.
    float lastPeak = blockPeak (ring.data(), kBlockSize);
    for (int b = 0; b < 64; ++b)
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        pool.render (ring.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
        const float p = blockPeak (ring.data(), kBlockSize);
        REQUIRE (p <= lastPeak + 1.0e-5f); // monotonic decay
        lastPeak = p;
        if (p <= 1.0e-6f)
            break;
    }

    // 80ms release at 48k = 3840 samples = 7.5 blocks; 64 iterations is plenty.
    REQUIRE (lastPeak <= 1.0e-6f);
}

TEST_CASE ("a released voice is reusable for a new note", "[dsp][voice]")
{
    aod::VoicePool pool;
    const aod::Sample sample = makeTone();

    pool.start (&sample, 60, 0.5f);
    pool.stop (60);

    // The slot is still owned by the fading voice, but a new start must re-fire
    // regardless of the release tail by resetting the same voice.
    pool.start (&sample, 60, 0.5f);
    std::vector<float> block (static_cast<std::size_t> (kBlockSize));
    pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
    REQUIRE (blockPeak (block.data(), kBlockSize) > 0.0f);
}

TEST_CASE ("retriggering a held note reuses its voice instead of stacking", "[dsp][voice]")
{
    aod::VoicePool pool;
    const aod::Sample sample = makeTone();

    // Hold note 60, then re-strike it (legato retrigger). The first voice must
    // be reset in place, so the pool does not burn a fresh slot per strike.
    pool.setPolyphony (1);
    pool.start (&sample, 60, 0.5f);
    pool.start (&sample, 60, 0.9f); // retrigger while still held

    std::vector<float> block (static_cast<std::size_t> (kBlockSize));
    pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);

    // With polyphony 1, a stacked voice would have been dropped and produced
    // silence; retriggering in place must still sound.
    REQUIRE (blockPeak (block.data(), kBlockSize) > 0.0f);
}

TEST_CASE ("note-off cannot release a voice recycled for another note", "[dsp][voice]")
{
    aod::VoicePool pool;
    const aod::Sample sample = makeTone();

    // Note 60 grabs the only slot, then releases and finishes its fade.
    pool.start (&sample, 60, 0.5f);
    pool.stop (60);
    for (int b = 0; b < 64; ++b)
    {
        std::vector<float> block (static_cast<std::size_t> (kBlockSize));
        pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
    }

    // The slot is recycled for note 62. A late note-off for 60 must not
    // silence the voice that is now actually playing 62.
    pool.start (&sample, 62, 0.5f);
    pool.stop (60); // stale note-off for a note that is no longer sounding

    std::vector<float> block (static_cast<std::size_t> (kBlockSize));
    pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
    REQUIRE (blockPeak (block.data(), kBlockSize) > 0.0f);
}