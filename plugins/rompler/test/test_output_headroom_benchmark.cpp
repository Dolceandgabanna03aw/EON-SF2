#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"
#include "SF2Loader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
constexpr int kSampleRate = 48000;
constexpr int kBlockSize = 256;

// Mirrors OutputSafetyProcessor::applySoftCeiling: content below the knee is
// passed through untouched, so a sample at or above these thresholds is proof
// that the safety stage, not the musical gain staging, decided the level.
constexpr float kKnee = 0.97f;
constexpr float kCeiling = 0.985f;

constexpr int kTimedBlocks = 32;
// Two seconds of silence between probes so a previous probe's reverb tail
// cannot inflate the next one.
constexpr int kSilenceBlocks = 375;

constexpr std::array<float, 4> kTrimOffsets { 0.0f, -3.0f, -6.0f, -12.0f };

juce::File findTestFont()
{
    const juce::File testData (X10_SF2_CROSSCHECK_TESTDATA);
    for (const auto* name : { "Crystal Legacy.sf2", "FFVIGM.sf2",
                              "Triton Strings.sf2", "Dr. Mario.sf2" })
    {
        const auto candidate = testData.getChildFile (name);
        if (candidate.existsAsFile())
            return candidate;
    }
    return {};
}

struct ProbeResult
{
    float peak = 0.0f;
    int kneeSamples = 0;
    int ceilingSamples = 0;
    int totalSamples = 0;
};

void setDenormalized (aod::RomplerProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.getValueTreeState().getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

// Keys the selected preset actually maps to a sample at this velocity. Playing
// only these keeps the probe about gain staging instead of about empty regions,
// and the velocity matters: a key can be sampled at 100 and silent at 127.
std::vector<int> soundingKeys (aod::SF2Loader& loader, int bank, int program, int velocity)
{
    std::vector<int> keys;
    for (int key = 0; key < 128; ++key)
        if (loader.getSample (bank, program, key, velocity) != nullptr)
            keys.push_back (key);
    return keys;
}

void accumulate (const juce::AudioBuffer<float>& buffer, ProbeResult& result)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float magnitude = std::abs (buffer.getSample (channel, sample));
            result.peak = std::max (result.peak, magnitude);
            result.totalSamples += 1;
            if (magnitude >= kKnee)
                ++result.kneeSamples;
            if (magnitude >= kCeiling)
                ++result.ceilingSamples;
    }
}

// The output stage promises nothing above its ceiling. Anything else means the
// buffer was written outside the documented path, so the probe reports it
// instead of folding it into the statistics.
bool scanAboveCeiling (const juce::AudioBuffer<float>& buffer, const char* phase,
                       int probe, const char* scenario, float trimDb, int block)
{
    bool reported = false;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float magnitude = std::abs (buffer.getSample (channel, sample));
            if (magnitude <= kCeiling + 1.0e-6f)   // tolerated rounding at the clamp
                continue;
            std::printf ("HEADROOM-ANOMALY phase=%s preset=%d scenario=%s trim_db=%.2f "
                         "block=%d channel=%d sample=%d value=%.9g\n",
                         phase, probe + 1, scenario, static_cast<double> (trimDb),
                         block, channel, sample, static_cast<double> (magnitude));
            reported = true;
            break;
        }
    return reported;
}
} // namespace

/**
    Representative-preset output headroom probe.

    The output safety stage is protection, not a mixing decision. This probe
    plays unmodified factory patches through the complete processBlock chain at
    their default settings and counts how much of the rendered signal actually
    reaches the safety knee (0.97) or its ceiling (0.985). It then repeats each
    render with progressively lower Output Trim values and reports the smallest
    reduction that keeps the whole signal below the knee.

    Hidden behind [.][benchmark]: the numbers describe this font on this
    machine. Run it directly with

        build-plugin/plugins/rompler/test/rompler_tests "representative preset output headroom probe"
*/
TEST_CASE ("representative preset output headroom probe", "[.][benchmark]")
{
    const juce::File font = findTestFont();
    if (! font.existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    aod::SF2Loader loader (kSampleRate);
    if (! loader.loadFile (font))
        SKIP ("could not load the test font");

    const int presetCount = loader.presetCount();
    REQUIRE (presetCount > 0);

    // A handful of presets spread across the font rather than the first few,
    // so one loud bank cannot stand in for the whole library.
    constexpr int maxProbes = 6;
    const int probeCount = std::min (maxProbes, presetCount);
    std::printf ("HEADROOM font=%s presets=%d probing=%d\n",
                 font.getFileName().toRawUTF8(), presetCount, probeCount);

    for (int probe = 0; probe < probeCount; ++probe)
    {
        const int preset = probeCount == 1
            ? 0
            : static_cast<int> (static_cast<long long> (probe) * (presetCount - 1) / (probeCount - 1));
        const auto [bank, program] = loader.presetBankProgram (preset);
        const auto chordKeys = soundingKeys (loader, bank, program, 100);
        const auto loudKeys = soundingKeys (loader, bank, program, 127);
        if (chordKeys.size() < 4 || loudKeys.empty())
            continue;

        aod::RomplerProcessor processor;
        processor.setPlayConfigDetails (0, 2, kSampleRate, kBlockSize);
        processor.prepareToPlay (kSampleRate, kBlockSize);
        processor.loadSoundFont (font);
        processor.selectPreset (bank, program);

        // One spread three-note chord and one top-of-range fortissimo note.
        const auto chord = std::array<int, 3> { chordKeys[chordKeys.size() / 4],
                                                chordKeys[chordKeys.size() / 2],
                                                chordKeys[(chordKeys.size() * 3) / 4] };
        struct Scenario
        {
            const char* name;
            std::vector<int> notes;
            int velocity;
        };
        const std::array<Scenario, 2> scenarios {
            Scenario { "chord", { chord[0], chord[1], chord[2] }, 100 },
            Scenario { "loud-top", { loudKeys.back() }, 127 }
        };

        const float defaultTrim = processor.getValueTreeState()
                                      .getParameter (aod::ParamIDs::outTrim)
                                      ->convertFrom0to1 (processor.getValueTreeState()
                                                             .getParameter (aod::ParamIDs::outTrim)
                                                             ->getValue());

        for (const auto& scenario : scenarios)
        {
            float cleanExtraDb = 0.0f;
            bool foundCleanTrim = false;

            for (const float offset : kTrimOffsets)
            {
                setDenormalized (processor, aod::ParamIDs::outTrim, defaultTrim + offset);
                processor.drainDeferredWorkForTesting();

                juce::AudioBuffer<float> buffer (2, kBlockSize);
                juce::MidiBuffer silence;
                // Release whatever the previous probe left sounding before the
                // decay gap, so this scenario is measured on its own voices.
                juce::MidiBuffer flush;
                flush.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                for (int block = 0; block < kSilenceBlocks; ++block)
                {
                    processor.processBlock (buffer, block == 0 ? flush : silence);
                    scanAboveCeiling (buffer, "gap", probe, scenario.name,
                                      defaultTrim + offset, block);
                }

                // Whatever is still sounding at the end of the gap is the
                // previous scenario's tail; the probe only counts if its own
                // trigger rises clearly above it.
                // Careful: the two-argument overload is (startSample, numSamples)
                // over every channel. There is no (channel, numSamples) overload,
                // so a second call with `1` would read one sample past the end of
                // the allocation instead of the right channel.
                const float residualPeak = buffer.getMagnitude (0, kBlockSize);

                juce::MidiBuffer trigger;
                trigger.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                for (const int note : scenario.notes)
                    trigger.addEvent (juce::MidiMessage::noteOn (
                                          1, note, static_cast<juce::uint8> (scenario.velocity)), 0);

                ProbeResult result;
                juce::MidiBuffer empty;
                for (int block = 0; block < kTimedBlocks; ++block)
                {
                    processor.processBlock (buffer, block == 0 ? trigger : empty);
                    scanAboveCeiling (buffer, "timed", probe, scenario.name,
                                      defaultTrim + offset, block);
                    accumulate (buffer, result);
                }

                const int activeVoices = processor.getActiveVoiceCountForTesting();
                std::printf ("HEADROOM preset=%d/%d name=\"%s\" bank=%d program=%d scenario=%s "
                             "trim_db=%.2f voices=%d peak=%.6f residual=%.6f knee=%d/%d ceiling=%d\n",
                             probe + 1, probeCount, loader.presetName (preset).toRawUTF8(),
                             bank, program, scenario.name, defaultTrim + offset,
                             activeVoices, static_cast<double> (result.peak),
                             static_cast<double> (residualPeak),
                             result.kneeSamples, result.totalSamples, result.ceilingSamples);

                CHECK (std::isfinite (result.peak));
                CHECK (result.totalSamples == kTimedBlocks * kBlockSize * 2);
                // A short, non-looping sample can finish inside the measured
                // window, so `activeVoices` is reported rather than asserted;
                // the guarantee is that the window is this probe's own sound.
                CHECK (result.peak > residualPeak * 2.0f);

                if (result.kneeSamples == 0)
                {
                    foundCleanTrim = true;
                    cleanExtraDb = offset;
                    break;
                }
            }

            // If even a 12 dB cut still drives the safety stage, the probe is
            // not measuring what it thinks it is.
            CHECK (foundCleanTrim);
            std::printf ("HEADROOM-MIN preset=%d scenario=%s extra_trim_db=%.0f\n",
                         probe + 1, scenario.name, static_cast<double> (cleanExtraDb));
        }

        processor.releaseResources();
    }
}
