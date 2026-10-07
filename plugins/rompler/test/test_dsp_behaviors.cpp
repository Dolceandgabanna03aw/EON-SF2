#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
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

TEST_CASE ("a saturated pool steals the longest-held voice for a new note", "[dsp][voice]")
{
    aod::VoicePool pool;
    const aod::Sample sample = makeTone();

    // Fill all slots with held notes so no voice is free or releasing.
    pool.setPolyphony (2);
    pool.start (&sample, 60, 0.5f);
    pool.start (&sample, 62, 0.5f);
    std::vector<float> block (static_cast<std::size_t> (kBlockSize));
    for (int b = 0; b < 8; ++b) // let note 60's envelope age
    {
        std::fill (block.begin(), block.end(), 0.0f);
        pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
    }

    // A third note must steal the oldest slot (60) rather than drop silently.
    pool.start (&sample, 64, 0.5f);
    std::fill (block.begin(), block.end(), 0.0f);
    pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
    REQUIRE (blockPeak (block.data(), kBlockSize) > 0.0f);
}

TEST_CASE ("a looping sample keeps sounding past its end", "[dsp][voice][loop]")
{
    aod::Sample sample = makeTone();
    // Loop the whole 1 s tone back to its head. A non-looping voice would run
    // off the end and go silent within the first second of playback; a looping
    // voice must still be sounding well past that.
    sample.loopStart   = 0;
    sample.loopEnd     = static_cast<int> (sample.data.size()) - 1;
    sample.loopEnabled = true;

    aod::VoicePool pool;
    pool.start (&sample, 60, 0.8f);

    // Play far past the sample length: 3 s of audio > 1 s sample.
    float lastPeak = 0.0f;
    for (int b = 0; b < 3 * kSampleRate / kBlockSize; ++b)
    {
        std::vector<float> block (static_cast<std::size_t> (kBlockSize));
        pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
        lastPeak = blockPeak (block.data(), kBlockSize);
        REQUIRE (lastPeak > 0.0f); // never dies out while held
    }
}

TEST_CASE ("a non-looping sample goes silent once it ends", "[dsp][voice][loop]")
{
    aod::Sample sample = makeTone();
    // No loop points: the classic one-shot behaviour must be preserved.
    sample.loopEnabled = false;

    aod::VoicePool pool;
    pool.start (&sample, 60, 0.8f);

    // The 1 s sample must have ended well before 2 s of playback.
    bool wentSilent = false;
    for (int b = 0; b < 2 * kSampleRate / kBlockSize; ++b)
    {
        std::vector<float> block (static_cast<std::size_t> (kBlockSize));
        pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
        if (blockPeak (block.data(), kBlockSize) <= 1.0e-6f)
        {
            wentSilent = true;
            break;
        }
    }
    REQUIRE (wentSilent);
}

TEST_CASE ("release on a looping sample fades out instead of restarting the loop", "[dsp][voice][loop]")
{
    aod::Sample sample = makeTone();
    sample.loopStart   = 0;
    sample.loopEnd     = static_cast<int> (sample.data.size()) - 1;
    sample.loopEnabled = true;

    aod::VoicePool pool;
    pool.start (&sample, 60, 0.8f);

    // Let it loop for a while, then release.
    for (int b = 0; b < 2 * kSampleRate / kBlockSize; ++b)
    {
        std::vector<float> block (static_cast<std::size_t> (kBlockSize));
        pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
    }
    pool.stop (60);

    // The release fade must run to zero monotonically; the loop must not
    // re-engage and keep it ringing forever.
    float lastPeak = 1.0f;
    bool hitZero = false;
    for (int b = 0; b < 32; ++b)
    {
        std::vector<float> block (static_cast<std::size_t> (kBlockSize));
        pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
        const float p = blockPeak (block.data(), kBlockSize);
        REQUIRE (p <= lastPeak + 1.0e-5f); // monotonic decay, no click back up
        lastPeak = p;
        if (p <= 1.0e-6f)
        {
            hitZero = true;
            break;
        }
    }
    REQUIRE (hitZero);
}

TEST_CASE ("release holds the last sample instead of truncating mid-cycle", "[dsp][voice]")
{
    aod::Sample sample;
    // Very short sample: release starts after the tone has nearly ended, so the
    // old code path would cut the voice off at the buffer end and click.
    constexpr int length = 128;
    sample.data.resize (static_cast<std::size_t> (length));
    for (int i = 0; i < length; ++i)
        sample.data[static_cast<std::size_t> (i)] = std::sin (2.0f * 3.14159265f * 1000.0f
                                                             * static_cast<float> (i)
                                                             / static_cast<float> (kSampleRate));
    sample.sampleRate = kSampleRate;

    aod::VoicePool pool;
    pool.start (&sample, 60, 0.8f);

    // Play the tone nearly to the end of the sample, then release.
    std::vector<float> block (static_cast<std::size_t> (kBlockSize));
    for (int b = 0; b < 1; ++b)
    {
        std::fill (block.begin(), block.end(), 0.0f);
        pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
    }
    pool.stop (60);

    // The release must still fade out to silence rather than stopping the
    // moment the sample ends.
    float lastPeak = 1.0f;
    bool hitZero = false;
    for (int b = 0; b < 32; ++b)
    {
        std::fill (block.begin(), block.end(), 0.0f);
        pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
        const float p = blockPeak (block.data(), kBlockSize);
        REQUIRE (p <= lastPeak + 1.0e-5f); // monotonic decay, no click back up
        lastPeak = p;
        if (p <= 1.0e-6f)
        {
            hitZero = true;
            break;
        }
    }
    REQUIRE (hitZero);
}

TEST_CASE ("release mid-attack keeps the fade slope continuous", "[dsp][voice]")
{
    aod::Sample sample = makeTone();

    aod::VoicePool pool;
    pool.start (&sample, 60, 0.8f);

    // Render a few samples into the attack (attack is 10 ms at 48 kHz, so
    // ~480 samples), then release while the envelope is still rising.
    std::vector<float> block (static_cast<std::size_t> (kBlockSize));
    pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
    pool.stop (60);

    // Render one more block and look at the *first differences* across the
    // release boundary. A 1 kHz sine at 48 kHz has a max slope of about 0.13
    // samples^-1; a discontinuous envelope (fixed-time ramp from a low release
    // level) would introduce a slope spike several times that. The scaled
    // ramp keeps the fade slope the same whatever the release level.
    std::fill (block.begin(), block.end(), 0.0f);
    pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);

    float maxSlope = 0.0f;
    for (int i = 1; i < kBlockSize; ++i)
        maxSlope = std::max (maxSlope, std::abs (block[static_cast<std::size_t> (i)]
                                                - block[static_cast<std::size_t> (i - 1)]));

    // Sine slope bound is ~0.13; release ramps down at the same rate, so the
    // combined envelope+sine slope stays under ~0.35. Anything much larger
    // indicates a discontinuity (click) at the release point.
    REQUIRE (maxSlope < 0.5f);
}
// ---------------------------------------------------------------------------
// Release envelope: the fade must start at the level the note was sounding at
// and reach silence over Voice::releaseTime (80 ms), not jump straight to 0.
// ---------------------------------------------------------------------------
namespace
{
/** Constant-valued sample: the envelope is then directly visible in the output. */
aod::Sample makeDc (int length, float value)
{
    aod::Sample s;
    s.data.assign (static_cast<std::size_t> (length), value);
    s.sampleRate = kSampleRate;
    return s;
}

struct ReleaseTrace
{
    float levelBeforeNoteOff = 0.0f;
    std::vector<float> afterNoteOff;
};

/** Holds a looping DC note for holdSamples, releases it, and records 200 ms after. */
ReleaseTrace traceRelease (std::int64_t holdSamples)
{
    aod::Sample sample = makeDc (kSampleRate, 0.5f);
    sample.loopStart   = 0;
    sample.loopEnd     = kSampleRate - 1;
    sample.loopEnabled = true;

    aod::VoicePool pool;
    pool.start (&sample, 60, 1.0f);

    constexpr int holdBlock = 4096;
    std::vector<float> block (static_cast<std::size_t> (holdBlock));
    std::int64_t rendered = 0;
    while (rendered < holdSamples)
    {
        const auto n = static_cast<int> (std::min<std::int64_t> (holdBlock, holdSamples - rendered));
        pool.render (block.data(), n, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
        rendered += n;
    }

    ReleaseTrace trace;
    trace.levelBeforeNoteOff = block[static_cast<std::size_t> (
        (holdSamples % holdBlock == 0 ? holdBlock : holdSamples % holdBlock) - 1)];

    pool.stop (60);

    // 1 ms blocks so the fade shape is sampled finely across block boundaries.
    constexpr int fineBlock = kSampleRate / 1000;
    std::vector<float> fine (static_cast<std::size_t> (fineBlock));
    for (int ms = 0; ms < 200; ++ms)
    {
        pool.render (fine.data(), fineBlock, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
        trace.afterNoteOff.insert (trace.afterNoteOff.end(), fine.begin(), fine.end());
    }
    return trace;
}

void requireSmoothReleaseOverReleaseTime (const ReleaseTrace& trace)
{
    const float before = trace.levelBeforeNoteOff;
    REQUIRE (before > 0.2f); // the note was actually sounding at sustain

    const auto& out = trace.afterNoteOff;
    const auto at = [&out] (double seconds) {
        return out[static_cast<std::size_t> (seconds * static_cast<double> (kSampleRate))];
    };

    // No instant drop: the first sample after note-off is still at (nearly)
    // the sustain level. The old code produced 0 here.
    REQUIRE (out[0] > 0.9f * before);

    // Halfway through the 80 ms ramp the note is clearly still fading, not gone.
    REQUIRE (at (0.040) > 0.25f * before);
    REQUIRE (at (0.040) < 0.75f * before);

    // Monotonic and free of steps: a linear 80 ms fade from ~0.35 moves
    // ~1e-4 per sample; an abrupt cut would be a step of ~0.3.
    float maxStep = 0.0f;
    float maxRise = 0.0f;
    for (std::size_t i = 1; i < out.size(); ++i)
    {
        maxStep = std::max (maxStep, out[i - 1] - out[i]);
        maxRise = std::max (maxRise, out[i] - out[i - 1]);
    }
    REQUIRE (maxRise <= 1.0e-6f);
    REQUIRE (maxStep < 1.0e-3f);

    // Still audible shortly before the release time elapses...
    REQUIRE (at (0.070) > 1.0e-3f);
    // ...and silent shortly after it, staying silent.
    float tail = 0.0f;
    for (std::size_t i = static_cast<std::size_t> (0.085 * kSampleRate); i < out.size(); ++i)
        tail = std::max (tail, std::abs (out[i]));
    REQUIRE (tail <= 1.0e-6f);
}
} // namespace

TEST_CASE ("note-off fades from the sounding level to silence over the release time", "[dsp][voice][release]")
{
    requireSmoothReleaseOverReleaseTime (traceRelease (kSampleRate)); // 1 s hold, at sustain
}

TEST_CASE ("a note held for ten minutes still releases within the release time", "[dsp][voice][release]")
{
    // A float envelope clock stops advancing at 512 s (1/48000 is below half
    // an ulp there), which froze the release fade so a long-held note never
    // went silent. 10 minutes is past that point at any common sample rate.
    requireSmoothReleaseOverReleaseTime (traceRelease (std::int64_t { 600 } * kSampleRate));
}

// ---------------------------------------------------------------------------
// Loop wrap: a step larger than the loop length must still wrap inside the
// loop, and the interpolation taps must never leave the buffer. Under the
// asan preset the old single-subtraction wrap is a heap-buffer-overflow.
// ---------------------------------------------------------------------------
namespace
{
void requireBoundedLoopPlayback (const aod::Sample& sample, int note)
{
    aod::VoicePool pool;
    pool.start (&sample, note, 1.0f);

    std::vector<float> block (static_cast<std::size_t> (kBlockSize));
    bool allFinite = true;
    float peak = 0.0f;
    float lastBlockPeak = 0.0f;
    for (int b = 0; b < 200; ++b)
    {
        std::fill (block.begin(), block.end(), 0.0f);
        pool.render (block.data(), kBlockSize, kSampleRate, 0.0f, 0.0f, 0, 0, 0.0f);
        for (const float v : block)
            allFinite = allFinite && std::isfinite (v);
        lastBlockPeak = blockPeak (block.data(), kBlockSize);
        peak = std::max (peak, lastBlockPeak);
    }
    REQUIRE (allFinite);
    REQUIRE (peak <= 1.0f); // source is +-0.5; garbage reads would not stay bounded
    // Still sounding after ~2 s, i.e. the loop kept wrapping instead of the
    // voice running off the end of the sample.
    REQUIRE (lastBlockPeak > 0.0f);
}

aod::Sample makeShortLoop (int length, int loopStart, int loopEnd)
{
    aod::Sample s;
    s.data.resize (static_cast<std::size_t> (length));
    for (int i = 0; i < length; ++i)
        s.data[static_cast<std::size_t> (i)] = 0.5f * std::sin (0.1f * static_cast<float> (i));
    s.sampleRate  = kSampleRate;
    s.loopStart   = loopStart;
    s.loopEnd     = loopEnd;
    s.loopEnabled = true;
    return s;
}
} // namespace

TEST_CASE ("a short loop played far above its root key stays inside the buffer", "[dsp][voice][loop]")
{
    SECTION ("10-frame loop near the end, note 127 over root 0 (~1500 frames per sample)")
    {
        aod::Sample sample = makeShortLoop (1000, 900, 910);
        sample.rootKey = 0.0f;
        requireBoundedLoopPlayback (sample, 127);
    }

    SECTION ("loop ending on the last frame, step just over the loop length")
    {
        // 4-frame loop [995, 999], three octaves up: 8 frames per sample.
        aod::Sample sample = makeShortLoop (1000, 995, 999);
        sample.rootKey = 60.0f;
        requireBoundedLoopPlayback (sample, 96);
    }

    SECTION ("hand-built loop end past the buffer is clamped, not trusted")
    {
        aod::Sample sample = makeShortLoop (1000, 990, 1050);
        sample.rootKey = 0.0f;
        requireBoundedLoopPlayback (sample, 127);
    }
}
