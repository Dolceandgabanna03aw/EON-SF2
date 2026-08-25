#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "PluginProcessor.h"
#include "Sampler.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 64;

/** A sine of known length, loud enough that the drive stage has something to work on. */
eon::Sample makeSine (int lengthSamples, float frequencyHz, float amplitude = 0.5f)
{
    std::vector<float> data (static_cast<std::size_t> (lengthSamples));

    for (int i = 0; i < lengthSamples; ++i)
    {
        const auto phase = 2.0 * std::numbers::pi_v<double> * static_cast<double> (frequencyHz)
                         * static_cast<double> (i) / kSampleRate;
        data[static_cast<std::size_t> (i)] = amplitude * static_cast<float> (std::sin (phase));
    }

    eon::Sample sample;
    sample.data = std::make_shared<const std::vector<float>> (std::move (data));
    sample.sampleRate = static_cast<int> (kSampleRate);
    sample.rootKey = 60.0f;

    // Wide open, so the tests below measure the drive stage rather than the filter.
    sample.filterCutoffHz = 20000.0f;

    return sample;
}

struct Scratch
{
    std::vector<float> dryLeft  = std::vector<float> (kBlockSize, 0.0f);
    std::vector<float> dryRight = std::vector<float> (kBlockSize, 0.0f);
    std::vector<float> wetLeft  = std::vector<float> (kBlockSize, 0.0f);
    std::vector<float> wetRight = std::vector<float> (kBlockSize, 0.0f);

    void clear()
    {
        std::fill (dryLeft.begin(), dryLeft.end(), 0.0f);
        std::fill (dryRight.begin(), dryRight.end(), 0.0f);
        std::fill (wetLeft.begin(), wetLeft.end(), 0.0f);
        std::fill (wetRight.begin(), wetRight.end(), 0.0f);
    }

    void render (eon::Voice& voice, const eon::VoiceSettings& settings)
    {
        clear();
        voice.render (dryLeft.data(), dryRight.data(), wetLeft.data(), wetRight.data(),
                      kBlockSize, settings);
    }

    [[nodiscard]] float wetPeak() const
    {
        float peak = 0.0f;
        for (float value : wetLeft)
            peak = std::max (peak, std::abs (value));
        return peak;
    }
};

/** Renders until the voice frees itself, returning how many samples that took. */
int samplesUntilSilent (eon::Voice& voice, const eon::VoiceSettings& settings, int limit)
{
    Scratch scratch;
    int total = 0;

    while (voice.isActive() && total < limit)
    {
        scratch.render (voice, settings);
        total += kBlockSize;
    }

    return total;
}
} // namespace

TEST_CASE ("a key above the root plays back faster", "[voice][dsp]")
{
    const auto sample = makeSine (48000, 220.0f);
    const eon::VoiceSettings settings;

    eon::Voice atRoot;
    atRoot.prepare (kSampleRate);
    atRoot.start (&sample, 60, 0.8f, 0);
    const int rootLength = samplesUntilSilent (atRoot, settings, 200000);

    eon::Voice octaveUp;
    octaveUp.prepare (kSampleRate);
    octaveUp.start (&sample, 72, 0.8f, 1);
    const int octaveLength = samplesUntilSilent (octaveUp, settings, 200000);

    CAPTURE (rootLength, octaveLength);

    // An octave up is twice the playback rate, so the sample is consumed in
    // half the time. One block of slack for the block-size quantisation.
    REQUIRE (octaveLength <= rootLength / 2 + kBlockSize);
    REQUIRE (octaveLength >= rootLength / 2 - kBlockSize);
}

TEST_CASE ("scaleTuning of zero pins every key to the root", "[voice][dsp]")
{
    auto sample = makeSine (24000, 220.0f);
    sample.scaleTuningCentsPerKey = 0.0f;

    const eon::VoiceSettings settings;

    eon::Voice atRoot;
    atRoot.prepare (kSampleRate);
    atRoot.start (&sample, 60, 0.8f, 0);

    eon::Voice twoOctavesUp;
    twoOctavesUp.prepare (kSampleRate);
    twoOctavesUp.start (&sample, 84, 0.8f, 1);

    REQUIRE (samplesUntilSilent (atRoot, settings, 100000)
             == samplesUntilSilent (twoOctavesUp, settings, 100000));
}

TEST_CASE ("a looping region keeps sounding past the end of its data", "[voice][dsp]")
{
    auto sample = makeSine (2000, 440.0f);
    sample.loopMode = x10::instrument::LoopMode::continuous;
    sample.loopStart = 0;
    sample.loopEnd = 2000; // Ends on the last frame: the case the wrap order got wrong.

    const eon::VoiceSettings settings;

    eon::Voice voice;
    voice.prepare (kSampleRate);
    voice.start (&sample, 60, 0.8f, 0);

    Scratch scratch;

    for (int block = 0; block < 200; ++block) // ~12800 samples, six times the data
        scratch.render (voice, settings);

    REQUIRE (voice.isActive());
    REQUIRE (scratch.wetPeak() > 0.0f);
}

TEST_CASE ("note-off releases rather than cutting the voice off", "[voice][dsp]")
{
    auto sample = makeSine (48000, 220.0f);
    sample.volumeEnvelope.releaseSeconds = 0.05f; // 2400 samples

    const eon::VoiceSettings settings;

    eon::Voice voice;
    voice.prepare (kSampleRate);
    voice.start (&sample, 60, 0.8f, 0);

    Scratch scratch;
    scratch.render (voice, settings);

    voice.release();
    REQUIRE (voice.isActive());

    // Output has to survive the block straight after the note-off; a voice that
    // is simply switched off produces the click this test exists to catch.
    scratch.render (voice, settings);
    REQUIRE (scratch.wetPeak() > 0.0f);

    const int tail = samplesUntilSilent (voice, settings, 48000);

    CAPTURE (tail);
    REQUIRE (tail > 0);
    REQUIRE (tail < 6000); // Well short of the sample's own 48000 frames.
}

TEST_CASE ("drive changes the waveform without running away in level", "[voice][dsp]")
{
    const auto sample = makeSine (4800, 220.0f);

    const auto peakAt = [&sample] (float drive01)
    {
        eon::VoiceSettings settings;
        settings.drive01 = drive01;
        settings.velocityToDrive = 0.0f;

        eon::Voice voice;
        voice.prepare (kSampleRate);
        voice.start (&sample, 60, 1.0f, 0);

        Scratch scratch;
        float peak = 0.0f;

        for (int block = 0; block < 20; ++block)
        {
            scratch.render (voice, settings);
            peak = std::max (peak, scratch.wetPeak());
        }

        return peak;
    };

    const float clean = peakAt (0.0f);
    const float driven = peakAt (1.0f);

    CAPTURE (clean, driven);

    REQUIRE (clean > 0.0f);
    REQUIRE (driven > 0.0f);

    // The post-gain normalises the curve's response to full scale, so heavy
    // drive lifts a half-scale sine towards unity but must not blow past it.
    REQUIRE (driven > clean);
    REQUIRE (driven < 1.5f);
}

TEST_CASE ("velocity moves drive in the direction the parameter asks for", "[voice][dsp]")
{
    const auto sample = makeSine (48000, 220.0f);

    // Peak level is the wrong thing to measure here: the post-gain normalises
    // the curve's response, so a hard-driven voice and a clean one both peak
    // near full scale. What drive actually changes is the shape, so this
    // measures crest factor — 1.41 for a sine, falling towards 1.0 as the curve
    // squares the waveform off.
    const auto crestAtVelocity = [&sample] (float velocity, float velocityToDrive)
    {
        eon::VoiceSettings settings;
        settings.drive01 = 0.5f;
        settings.velocityToDrive = velocityToDrive;

        eon::Voice voice;
        voice.prepare (kSampleRate);
        voice.start (&sample, 60, velocity, 0);

        Scratch scratch;
        float peak = 0.0f;
        double sumOfSquares = 0.0;
        int counted = 0;

        for (int block = 0; block < 24; ++block)
        {
            scratch.render (voice, settings);

            // The first blocks cover the envelope's attack, which would skew
            // both statistics.
            if (block < 4)
                continue;

            for (float value : scratch.wetLeft)
            {
                peak = std::max (peak, std::abs (value));
                sumOfSquares += static_cast<double> (value) * static_cast<double> (value);
                ++counted;
            }
        }

        const auto rms = std::sqrt (sumOfSquares / static_cast<double> (counted));
        return rms > 1.0e-9 ? peak / static_cast<float> (rms) : 0.0f;
    };

    const float hardPositive = crestAtVelocity (1.0f, 1.0f);
    const float softPositive = crestAtVelocity (0.2f, 1.0f);
    const float hardNegative = crestAtVelocity (1.0f, -1.0f);
    const float softNegative = crestAtVelocity (0.2f, -1.0f);

    CAPTURE (hardPositive, softPositive, hardNegative, softNegative);

    REQUIRE (hardPositive < softPositive);
    REQUIRE (hardNegative > softNegative);
}

TEST_CASE ("the polyphony limit caps how many voices sound", "[voice][dsp]")
{
    const auto sample = makeSine (48000, 220.0f);

    eon::VoicePool pool;
    pool.prepare (kSampleRate);
    pool.setPolyphony (2);

    for (int note = 60; note < 68; ++note)
        pool.noteOn (&sample, note, 0.8f);

    REQUIRE (pool.activeVoiceCount() == 2);

    // Stealing is oldest-first, so the two most recent notes are the survivors.
    pool.noteOff (60);
    REQUIRE (pool.activeVoiceCount() == 2);
}

TEST_CASE ("shrinking the polyphony limit silences the voices outside it", "[voice][dsp]")
{
    const auto sample = makeSine (48000, 220.0f);

    eon::VoicePool pool;
    pool.prepare (kSampleRate);
    pool.setPolyphony (8);

    for (int note = 60; note < 68; ++note)
        pool.noteOn (&sample, note, 0.8f);

    REQUIRE (pool.activeVoiceCount() == 8);

    pool.setPolyphony (3);
    REQUIRE (pool.activeVoiceCount() == 3);
}

TEST_CASE ("oversampling reports its latency to the host", "[bus][dsp]")
{
    eon::RomplerProcessor processor;
    processor.setPlayConfigDetails (0, 2, kSampleRate, 512);
    processor.prepareToPlay (kSampleRate, 512);

    auto* factor = processor.getValueTreeState().getParameter (eon::ParamIDs::busOsFactor);
    REQUIRE (factor != nullptr);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    // 1x is a pass-through and must cost nothing.
    factor->setValueNotifyingHost (0.0f);
    processor.processBlock (buffer, midi);
    processor.flushPendingLatencyUpdate();
    REQUIRE (processor.getLatencySamples() == 0);

    factor->setValueNotifyingHost (1.0f); // 8x
    processor.processBlock (buffer, midi);
    processor.flushPendingLatencyUpdate();
    REQUIRE (processor.getLatencySamples() > 0);
}

TEST_CASE ("mix at zero leaves the bus stage out of the signal", "[bus][dsp]")
{
    eon::RomplerProcessor processor;
    processor.setPlayConfigDetails (0, 2, kSampleRate, 512);
    processor.prepareToPlay (kSampleRate, 512);

    auto& state = processor.getValueTreeState();

    // Fold hard enough that any wet leakage would be plainly audible.
    state.getParameter (eon::ParamIDs::busFold)->setValueNotifyingHost (1.0f);
    state.getParameter (eon::ParamIDs::busOsFactor)->setValueNotifyingHost (0.0f);
    state.getParameter (eon::ParamIDs::outMix)->setValueNotifyingHost (0.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    // Nothing is loaded, so dry is silence and the output must be silence too
    // rather than whatever the fold stage does with a zero input.
    REQUIRE (buffer.getMagnitude (0, 512) <= 0.0f);
}
