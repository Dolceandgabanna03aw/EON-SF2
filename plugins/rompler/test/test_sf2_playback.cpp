#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "SF2Loader.h"
#include "Sf2Builder.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 512;

juce::File testSf2File()
{
    return juce::File (X10_SF2_CROSSCHECK_TESTDATA "/Dr._Mario_64_Soundfont.sf2");
}

class GeneratedSf2Fixture
{
public:
    GeneratedSf2Fixture()
        : file_ (".sf2")
    {
        x10::sf2::test::Sf2Builder builder;
        builder.sampleFrames = 4096;
        const auto bytes = builder.build();
        REQUIRE (file_.getFile().replaceWithData (bytes.data(), bytes.size()));
    }

    [[nodiscard]] const juce::File& file() const noexcept { return file_.getFile(); }

private:
    juce::TemporaryFile file_;
};

/**
    One preset -> one instrument -> one looping sample recorded at sourceRate.
    The builder's PCM is a ramp (frame i = i * 100), so frames must stay below
    ~327 for the ramp to stay monotonic; a value read back identifies its frame.
*/
class LoopingSf2Fixture
{
public:
    LoopingSf2Fixture (std::uint32_t sourceRate, std::uint32_t frames,
                       std::uint32_t loopStart, std::uint32_t loopEnd)
        : file_ (".sf2")
    {
        using namespace x10::sf2::test;
        Sf2Builder builder;
        builder.sampleFrames = frames;

        BuilderInstrument instrument;
        instrument.name = "Looping";
        instrument.zones.push_back (BuilderZone { { { 54, 1 },     // sampleModes: loop continuously
                                                    { 53, 0 } } }); // sampleID -> 0
        builder.instruments.push_back (instrument);

        BuilderSample sample;
        sample.name       = "Ramp";
        sample.start      = 0;
        sample.end        = frames;
        sample.loopStart  = loopStart;
        sample.loopEnd    = loopEnd;
        sample.sampleRate = sourceRate;
        builder.samples.push_back (sample);

        const auto bytes = builder.build();
        REQUIRE (file_.getFile().replaceWithData (bytes.data(), bytes.size()));
    }

    [[nodiscard]] const juce::File& file() const noexcept { return file_.getFile(); }

private:
    juce::TemporaryFile file_;
};

void setParameter (aod::RomplerProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getValueTreeState().getParameter (id);
    REQUIRE (parameter != nullptr);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

double rmsOf (const juce::AudioBuffer<float>& buffer, int channel, int start, int length)
{
    double sum = 0.0;
    for (int i = start; i < start + length; ++i)
        sum += static_cast<double> (buffer.getSample (channel, i)) * static_cast<double> (buffer.getSample (channel, i));
    return std::sqrt (sum / static_cast<double> (length));
}

/** Plays one note for `seconds` through a fresh processor with the FX sends off. */
juce::AudioBuffer<float> renderNote (const juce::File& bank, float drivePercent, float velToDrivePercent,
                                     int velocity, double seconds)
{
    aod::RomplerProcessor processor;
    processor.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);
    processor.loadSoundFont (bank);
    REQUIRE (processor.getPresetCount() == 1);

    setParameter (processor, aod::ParamIDs::fxChorusMix, 0.0f);
    setParameter (processor, aod::ParamIDs::fxReverbMix, 0.0f);
    setParameter (processor, aod::ParamIDs::voiceDrive, drivePercent);
    setParameter (processor, aod::ParamIDs::voiceVelToDrive, velToDrivePercent);

    const int total = static_cast<int> (seconds * kSampleRate) / kBlockSize * kBlockSize;
    juce::AudioBuffer<float> out (2, total);
    juce::AudioBuffer<float> block (2, kBlockSize);
    for (int pos = 0; pos < total; pos += kBlockSize)
    {
        juce::MidiBuffer midi;
        if (pos == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, static_cast<juce::uint8> (velocity)), 0);
        block.clear();
        processor.processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, pos, block, ch, 0, kBlockSize);
    }
    processor.releaseResources();
    return out;
}
} // namespace

TEST_CASE ("SF2Loader loads a real bank and resolves a sample for note-on", "[sf2][m1]")
{
    if (! testSf2File().existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    aod::SF2Loader loader (static_cast<int> (kSampleRate));
    REQUIRE (loader.loadFile (testSf2File()));
    REQUIRE (loader.presetCount() > 0);

    const auto [bank, program] = loader.firstPresetProgram();
    aod::Sample* sample = loader.getSample (bank, program, 60, 100);
    REQUIRE (sample != nullptr);
    REQUIRE (! sample->data.empty());
}

TEST_CASE ("a note-on through the processor produces non-silent output", "[sf2][m1]")
{
    if (! testSf2File().existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    aod::RomplerProcessor processor;
    processor.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
    processor.prepareToPlay (kSampleRate, kBlockSize);
    processor.loadSoundFont (testSf2File());

    aod::SF2Loader loader (static_cast<int> (kSampleRate));
    REQUIRE (loader.loadFile (testSf2File()));
    const auto [bank, program] = loader.firstPresetProgram();

    // Find a midi key that the first preset actually voices, so the block is
    // non-silent regardless of which font is bundled or how regions are pinned.
    int soundingKey = -1;
    for (int key = 0; key < 128 && soundingKey < 0; ++key)
        if (loader.getSample (bank, program, key, 100) != nullptr)
            soundingKey = key;
    REQUIRE (soundingKey >= 0);
    processor.selectPreset (bank, program);

    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, soundingKey, static_cast<juce::uint8> (100)), 0);

    processor.processBlock (buffer, midi);

    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = std::max (peak, buffer.getMagnitude (ch, 0, kBlockSize));

    REQUIRE (peak > 0.0f);
}

TEST_CASE ("processor honours the MIDI sample offset", "[sf2][midi][timing]")
{
    const GeneratedSf2Fixture fixture;

    aod::SF2Loader loader (static_cast<int> (kSampleRate));
    REQUIRE (loader.loadFile (fixture.file()));
    const auto [bank, program] = loader.firstPresetProgram();

    int soundingKey = -1;
    for (int key = 0; key < 128 && soundingKey < 0; ++key)
        if (loader.getSample (bank, program, key, 100) != nullptr)
            soundingKey = key;
    REQUIRE (soundingKey >= 0);
    auto renderAt = [&] (int sampleOffset)
    {
        aod::RomplerProcessor processor;
        processor.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
        processor.prepareToPlay (kSampleRate, kBlockSize);
        processor.loadSoundFont (fixture.file());
        processor.selectPreset (bank, program);

        juce::AudioBuffer<float> rendered (2, kBlockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, soundingKey, static_cast<juce::uint8> (100)),
                       sampleOffset);
        processor.processBlock (rendered, midi);
        return rendered;
    };

    constexpr int noteOffset = 384;
    const auto immediate = renderAt (0);
    const auto delayed = renderAt (noteOffset);

    float maxDifference = 0.0f;
    for (int sample = 0; sample < kBlockSize; ++sample)
        maxDifference = std::max (maxDifference,
                                  std::abs (immediate.getSample (0, sample)
                                            - delayed.getSample (0, sample)));

    REQUIRE (immediate.getMagnitude (0, 0, kBlockSize) > 0.0f);
    REQUIRE (delayed.getMagnitude (0, 0, noteOffset) <= std::numeric_limits<float>::epsilon());
    REQUIRE (delayed.getMagnitude (0, noteOffset, kBlockSize - noteOffset) > 0.0f);
    REQUIRE (maxDifference > 0.0f);
}

TEST_CASE ("pitch tracks the played MIDI note", "[sf2][pitch]")
{
    if (! testSf2File().existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    aod::SF2Loader loader (static_cast<int> (kSampleRate));
    REQUIRE (loader.loadFile (testSf2File()));
    const auto [bank, program] = loader.firstPresetProgram();

    // Find a key that resolves a sample so we can measure its playback pitch.
    aod::Sample* sample = nullptr;
    for (int key = 0; key < 128; ++key)
    {
        sample = loader.getSample (bank, program, key, 100);
        if (sample != nullptr)
            break;
    }
    REQUIRE (sample != nullptr);
    REQUIRE (! sample->data.empty());

    // A note 12 semitones above the root must advance exactly twice as fast
    // (2^(12/12) = 2) when the region uses normal chromatic tuning.
    const double expectedRatio = std::pow (2.0, sample->scaleTuningCentsPerKey / 100.0);
    REQUIRE (expectedRatio > 1.5);

    const int noteA = juce::jlimit (0, 115, static_cast<int> (std::lround (sample->rootKey)));
    const int noteB = noteA + 12;

    auto zeroCrossings = [] (aod::Sample* s, int note, int blockSize)
    {
        aod::VoicePool pool (1);
        pool.start (s, note, 100.0f / 127.0f);
        juce::AudioBuffer<float> buf (1, blockSize);
        pool.render (buf.getWritePointer (0), blockSize, static_cast<int> (kSampleRate),
                     0.0f, 0.0f, 0, 0, 0.0f);

        int crossings = 0;
        for (int i = 1; i < blockSize; ++i)
        {
            const float a = buf.getSample (0, i - 1);
            const float b = buf.getSample (0, i);
            if ((a < 0.0f && b >= 0.0f) || (a >= 0.0f && b < 0.0f))
                ++crossings;
        }
        return crossings;
    };

    constexpr int kBigBlock = 8192;
    const int zcA = zeroCrossings (sample, noteA, kBigBlock);
    const int zcB = zeroCrossings (sample, noteB, kBigBlock);

    REQUIRE (zcA > 20); // the sample must actually oscillate
    REQUIRE (zcB > 20);
    const double ratio = static_cast<double> (zcB) / static_cast<double> (zcA);
    REQUIRE (std::abs (ratio - expectedRatio) < 0.2);
}

TEST_CASE ("the Drive knob saturates louder instead of collapsing to silence", "[sf2][drive]")
{
    const LoopingSf2Fixture bank (48000, 300, 20, 280);

    // 0.4 s: past attack and decay; measure the last 100 ms of sustain.
    constexpr double seconds = 0.4;
    const int window = static_cast<int> (kSampleRate) / 10;

    double previous = 0.0;
    for (const float drive : { 0.0f, 20.0f, 50.0f, 100.0f })
    {
        CAPTURE (drive);
        const auto out = renderNote (bank.file(), drive, 0.0f, 80, seconds);
        const int start = out.getNumSamples() - window;
        const double rms = rmsOf (out, 0, start, window);
        CAPTURE (rms, previous);

        CHECK (rms > 0.1);                 // never collapses toward silence
        CHECK (rms >= previous);           // more drive is never quieter
        // Loose bound: the voice-level drive tests pin the peak precisely.
        // Here the FX stage still sits after the voice, and juce::Reverb
        // applies its internal 2x dry scale even with the reverb mix at 0.
        CHECK (out.getMagnitude (0, 0, out.getNumSamples()) < 2.0f);
        previous = rms;
    }
}

TEST_CASE ("loop points follow the sample when it is resampled to the host rate", "[sf2][loop][resample]")
{
    constexpr std::uint32_t frames = 300, loopStart = 60, loopEnd = 240;
    constexpr int hostRate = 48000;
    const float sourceStep = 100.0f / 32768.0f; // ramp increment per source frame

    for (const std::uint32_t sourceRate : { 22050u, 44100u, 48000u, 96000u })
    {
        CAPTURE (sourceRate);
        const LoopingSf2Fixture bank (sourceRate, frames, loopStart, loopEnd);

        aod::SF2Loader loader (hostRate);
        REQUIRE (loader.loadFile (bank.file()));
        const auto [b, p] = loader.firstPresetProgram();
        const aod::Sample* sample = loader.getSample (b, p, 60, 100);
        REQUIRE (sample != nullptr);
        REQUIRE (sample->loopEnabled);

        const double ratio = static_cast<double> (hostRate) / static_cast<double> (sourceRate);
        CHECK (std::abs (sample->loopStart - static_cast<double> (loopStart) * ratio) <= 1.0);
        CHECK (std::abs (sample->loopEnd - static_cast<double> (loopEnd) * ratio) <= 1.0);

        // The frames at the loop points still carry the source frames' values:
        // the loop covers the same audio, not a different stretch of it.
        REQUIRE (sample->loopEnd < static_cast<int> (sample->data.size()));
        CHECK (std::abs (sample->data[static_cast<std::size_t> (sample->loopStart)]
                         - static_cast<float> (loopStart) * sourceStep) <= sourceStep);
        CHECK (std::abs (sample->data[static_cast<std::size_t> (sample->loopEnd)]
                         - static_cast<float> (loopEnd) * sourceStep) <= sourceStep);
    }
}

TEST_CASE ("a resampled loop repeats at the source loop's rate", "[sf2][loop][resample]")
{
    // 180-frame loop at 22.05 kHz: 122.5 repetitions per second at the root key.
    const LoopingSf2Fixture bank (22050, 300, 60, 240);
    aod::SF2Loader loader (48000);
    REQUIRE (loader.loadFile (bank.file()));
    const auto [b, p] = loader.firstPresetProgram();
    const aod::Sample* sample = loader.getSample (b, p, 60, 100);
    REQUIRE (sample != nullptr);

    aod::VoicePool pool (1);
    pool.start (sample, 60, 1.0f);
    std::vector<float> out (48000);
    pool.render (out.data(), static_cast<int> (out.size()), 48000, 0.0f, 0.0f, 0, 0, 0.0f);

    // The ramp drops sharply only where the loop wraps back to its start.
    // The voice filter rings for a few samples after each drop, so drops
    // closer together than 32 samples count as one wrap.
    int wraps = 0;
    std::size_t lastWrap = 0;
    for (std::size_t i = 4800; i < out.size(); ++i) // skip the attack
        if (out[i] < out[i - 1] - 0.05f && i - lastWrap > 32)
        {
            ++wraps;
            lastWrap = i;
        }
    const double perSecond = wraps / 0.9;
    CAPTURE (wraps, perSecond);
    CHECK (std::abs (perSecond - 22050.0 / 180.0) < 2.0);
}
