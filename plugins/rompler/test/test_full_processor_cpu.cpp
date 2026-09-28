#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <vector>

#include "Parameters.h"
#include "PluginProcessor.h"
#include "SF2Loader.h"

namespace
{

/** Candidate fonts, in order of preference. The shipped default comes first
    because it is the most representative bank; later entries are fallbacks so
    the diagnostic still runs on machines without the packaged font. */
const std::array<const char*, 4> kFontCandidates {{
    "Crystal Legacy.sf2",
    "FFVIGM.sf2",
    "Korg_Triton_Strings_Ensemble_Soundfont.sf2",
    "Dr._Mario_64_Soundfont.sf2",
}};

struct SustainedPatch
{
    juce::File file;
    int bank = 0;
    int program = 0;
    // Keys whose mapped sample loops forever and is actually audible.
    // Non-looping samples are unusable here (a high note plays them many
    // times faster, so one that looks long enough at rate 1 can still
    // exhaust mid-run), and a silent loop keeps a voice "active" while
    // contributing nothing to the measured render cost or output peak.
    std::vector<int> keys;
};

juce::File findTestFont()
{
    const juce::File corpus (X10_SF2_CROSSCHECK_TESTDATA);
    for (const auto* name : kFontCandidates)
        if (const juce::File file = corpus.getChildFile (name); file.existsAsFile())
            return file;
    return {};
}

SustainedPatch findSustainedPatch (const juce::File& font)
{
    aod::SF2Loader loader (48000);
    if (! loader.loadFile (font))
        return {};

    SustainedPatch best { font, 0, 0, {} };
    for (int preset = 0; preset < loader.presetCount(); ++preset)
    {
        const auto [bank, program] = loader.presetBankProgram (preset);
        std::vector<int> keys;
        for (int key = 0; key < 128; ++key)
        {
            const auto* sample = loader.getSample (bank, program, key, 100);
            if (sample == nullptr || ! sample->loopEnabled)
                continue;
            const auto peak = std::max_element (sample->data.begin(), sample->data.end(),
                                                [] (float a, float b)
                                                { return std::abs (a) < std::abs (b); });
            // `peak` is selected by absolute magnitude, so preserve that
            // meaning when checking audibility. A waveform whose largest
            // excursion is negative is still a valid sustained voice.
            if (peak != sample->data.end() && std::abs (*peak) > 0.05f)
                keys.push_back (key);
        }
        if (keys.size() > best.keys.size())
            best = { font, bank, program, std::move (keys) };
    }
    return best;
}

void setDenormalized (aod::RomplerProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.getValueTreeState().getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

struct LongRunStats
{
    std::vector<double> timings;
    std::int64_t renderedSamples = 0;
    std::size_t nonFiniteSamples = 0;
    int minActiveVoices = std::numeric_limits<int>::max();
    int maxActiveVoices = 0;
    float maxPeak = 0.0f;
    std::size_t deadlineMisses = 0;
    std::size_t midiDeadlineMisses = 0;
    std::size_t automationDeadlineMisses = 0;
    std::size_t unmarkedDeadlineMisses = 0;
    std::size_t fullVoiceDeadlineMisses = 0;
    std::size_t reducedVoiceDeadlineMisses = 0;
    std::vector<std::uint8_t> midiBlocks;
    std::vector<std::uint8_t> automationBlocks;
    std::vector<int> activeVoiceCounts;
};

double percentile (std::vector<double> sorted, double fraction)
{
    if (sorted.empty())
        return 0.0;

    std::sort (sorted.begin(), sorted.end());
    const auto index = static_cast<std::size_t> (
        std::floor (juce::jlimit (0.0, 1.0, fraction)
                    * static_cast<double> (sorted.size() - 1)));
    return sorted[index];
}

void scanFiniteSamples (const juce::AudioBuffer<float>& buffer, LongRunStats& stats)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* data = buffer.getReadPointer (channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float value = data[sample];
            if (! std::isfinite (value))
                ++stats.nonFiniteSamples;
            else
                stats.maxPeak = std::max (stats.maxPeak, std::abs (value));
        }
    }
}

void addChord (juce::MidiBuffer& midi, const std::vector<int>& keys, int count,
               int velocity, int samplePosition = 0)
{
    if (keys.empty())
        return;

    for (int n = 0; n < count; ++n)
    {
        const auto keyIndex = static_cast<std::size_t> (
            std::llround (static_cast<double> (n)
                          * static_cast<double> (keys.size() - 1)
                          / static_cast<double> (std::max (1, count - 1))));
        midi.addEvent (juce::MidiMessage::noteOn (1, keys[keyIndex],
                                                   static_cast<juce::uint8> (velocity)),
                       samplePosition);
    }
}

void addNoteOffs (juce::MidiBuffer& midi, const std::vector<int>& keys,
                  int samplePosition = 0)
{
    for (const int key : keys)
        midi.addEvent (juce::MidiMessage::noteOff (1, key), samplePosition);
}

void setLongRunAutomation (aod::RomplerProcessor& processor, int phase)
{
    const float toggle = phase % 2 == 0 ? 0.0f : 12.0f;
    setDenormalized (processor, aod::ParamIDs::voiceDrive, toggle);
    setDenormalized (processor, aod::ParamIDs::busTapeDrive, phase % 2 == 0 ? 0.0f : 18.0f);
    setDenormalized (processor, aod::ParamIDs::busFold, phase % 2 == 0 ? 0.0f : 6.0f);
    setDenormalized (processor, aod::ParamIDs::outTrim, phase % 2 == 0 ? -3.0f : -6.0f);
    setDenormalized (processor, aod::ParamIDs::fxChorusMix, phase % 2 == 0 ? 8.0f : 22.0f);
    setDenormalized (processor, aod::ParamIDs::compMix, phase % 2 == 0 ? 0.0f : 24.0f);
}

LongRunStats renderLongRun (aod::RomplerProcessor& processor,
                            const SustainedPatch& patch,
                            int rate, int blockSize, int targetVoices,
                            int durationSeconds, bool exerciseMidi)
{
    LongRunStats stats;
    const auto blockCount = static_cast<std::size_t> (
        (static_cast<std::int64_t> (rate) * durationSeconds + blockSize - 1) / blockSize);
    stats.timings.reserve (blockCount);
    stats.midiBlocks.reserve (blockCount);
    stats.automationBlocks.reserve (blockCount);
    stats.activeVoiceCounts.reserve (blockCount);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    const std::int64_t cycleSamples = static_cast<std::int64_t> (rate) * 4;
    const std::int64_t automationSamples = std::max<std::int64_t> (1, rate / 4);
    std::int64_t nextCycle = 0;
    std::int64_t nextAutomation = 0;
    int cycle = -1;
    int automationPhase = 0;

    for (std::size_t block = 0; block < blockCount; ++block)
    {
        midi.clear();
        const auto blockStart = stats.renderedSamples;
        bool hasAutomation = false;
        if (blockStart >= nextAutomation)
        {
            hasAutomation = true;
            setLongRunAutomation (processor, automationPhase++);
            nextAutomation += automationSamples;
        }

        if (exerciseMidi && blockStart >= nextCycle)
        {
            cycle = (cycle + 1) % 4;
            nextCycle += cycleSamples;
            const int eventPosition = blockSize > 1 ? blockSize / 2 : 0;

            if (cycle == 0)
            {
                setDenormalized (processor, aod::ParamIDs::polyLimit,
                                 static_cast<float> (targetVoices));
                midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                addChord (midi, patch.keys, targetVoices, 100);
            }
            else if (cycle == 1)
            {
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
                addNoteOffs (midi, patch.keys, eventPosition);
            }
            else if (cycle == 2)
            {
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 96), eventPosition);
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 10, 32), eventPosition);
                midi.addEvent (juce::MidiMessage::pitchWheel (1, 12288), eventPosition);
            }
            else
            {
                // Lowering the live limit and filling the complete mapped
                // key range guarantees a real steal path even at a 128-voice
                // target, where the SoundFont has at most 128 unique keys.
                setDenormalized (processor, aod::ParamIDs::polyLimit,
                                 static_cast<float> (std::max (8, targetVoices / 2)));
                midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                addChord (midi, patch.keys, static_cast<int> (patch.keys.size()), 127);
            }
        }

        const auto begin = std::chrono::steady_clock::now();
        processor.processBlock (buffer, midi);
        const auto end = std::chrono::steady_clock::now();
        stats.timings.push_back (
            std::chrono::duration<double, std::micro> (end - begin).count());
        stats.midiBlocks.push_back (static_cast<std::uint8_t> (! midi.isEmpty()));
        stats.automationBlocks.push_back (static_cast<std::uint8_t> (hasAutomation));
        scanFiniteSamples (buffer, stats);
        const int activeVoices = processor.getActiveVoiceCountForTesting();
        stats.activeVoiceCounts.push_back (activeVoices);
        stats.maxActiveVoices = std::max (stats.maxActiveVoices, activeVoices);
        stats.minActiveVoices = std::min (stats.minActiveVoices, activeVoices);
        stats.renderedSamples += blockSize;
    }

    const double deadline = 1.0e6 * static_cast<double> (blockSize)
                            / static_cast<double> (rate);
    for (std::size_t block = 0; block < stats.timings.size(); ++block)
    {
        if (stats.timings[block] <= deadline)
            continue;

        ++stats.deadlineMisses;
        if (stats.midiBlocks[block] != 0)
            ++stats.midiDeadlineMisses;
        else if (stats.automationBlocks[block] != 0)
            ++stats.automationDeadlineMisses;
        else
            ++stats.unmarkedDeadlineMisses;

        if (stats.activeVoiceCounts[block] >= targetVoices)
            ++stats.fullVoiceDeadlineMisses;
        else
            ++stats.reducedVoiceDeadlineMisses;
    }
    return stats;
}

LongRunStats renderNoLoadBaseline (aod::RomplerProcessor& processor, int rate,
                                   int blockSize, int durationSeconds)
{
    return renderLongRun (processor, SustainedPatch {}, rate, blockSize, 0,
                          durationSeconds, false);
}

} // namespace

/**
    Sustained full-processor CPU baseline.

    The stage-1 diagnostic measures VoicePool::render in isolation; this one
    measures the complete processBlock chain — parameter sync, MIDI dispatch,
    voice rendering, stereo spread, the oversampled bus stage, dynamics, FX,
    output gain and the safety ceiling — while a chord of real SoundFont voices
    sustains. Triggering happens inside the first warmup block so the timed
    window is pure steady-state render.

    Hidden behind [.][benchmark]: it answers "does the whole chain fit the
    block deadline" for the same rate/block/voice corners where the voice-pool
    baseline missed, and prints rather than asserts timing because the numbers
    describe this machine's load, not a portable contract.
*/
TEST_CASE ("full processor sustained CPU deadline baseline", "[.][benchmark]")
{
    const juce::File font = findTestFont();
    if (! font.existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    const SustainedPatch patch = findSustainedPatch (font);
    if (patch.keys.size() < 32)
        SKIP ("no preset with enough sustained keys in the test font");

    for (int rate : { 48000, 96000 })
    {
        aod::RomplerProcessor processor;
        processor.setPlayConfigDetails (0, 2, rate, 512);
        processor.prepareToPlay (rate, 512);
        processor.loadSoundFont (font);
        processor.selectPreset (patch.bank, patch.program);

        for (int blockSize : { 64, 512 })
            for (int targetVoices : { 32, 128 })
                for (int osIndex : { 0, 2 })
                    for (float drive : { 0.0f, 12.0f })
                    {
                        const int voices = std::min (targetVoices,
                                                     static_cast<int> (patch.keys.size()));

                        setDenormalized (processor, aod::ParamIDs::polyLimit,
                                         static_cast<float> (voices));
                        setDenormalized (processor, aod::ParamIDs::busOsFactor,
                                         static_cast<float> (osIndex));
                        setDenormalized (processor, aod::ParamIDs::voiceDrive, drive);

                        juce::AudioBuffer<float> buffer (2, blockSize);
                        juce::MidiBuffer trigger;
                        trigger.addEvent (juce::MidiMessage::allNotesOff (1), 0);
                        for (int n = 0; n < voices; ++n)
                        {
                            // Spread reduced-polyphony runs across the
                            // selected patch instead of always taking its
                            // lowest keys. Some real banks have a silent
                            // attack region at a subset of pitches, which can
                            // make a short 96 kHz block look silent while the
                            // voices are still active.
                            const auto keyIndex = static_cast<std::size_t> (
                                std::llround (static_cast<double> (n)
                                              * static_cast<double> (patch.keys.size() - 1)
                                              / static_cast<double> (std::max (1, voices - 1))));
                            trigger.addEvent (juce::MidiMessage::noteOn (
                                                  1, patch.keys[keyIndex],
                                                  static_cast<juce::uint8> (100)),
                                              0);
                        }
                        juce::MidiBuffer empty;

                        std::array<double, 80> timings {};
                        float maxPeak = 0.0f;
                        for (int pass = -8; pass < 80; ++pass)
                        {
                            const auto begin = std::chrono::steady_clock::now();
                            processor.processBlock (buffer,
                                                    pass == -8 ? trigger : empty);
                            const auto end = std::chrono::steady_clock::now();
                            if (pass >= 0)
                            {
                                timings[static_cast<std::size_t> (pass)] =
                                    std::chrono::duration<double, std::micro> (end - begin).count();
                                // getMagnitude (startSample, numSamples) already
                                // spans every channel; passing a channel index as
                                // the first argument would read past the buffer.
                                maxPeak = std::max (maxPeak, buffer.getMagnitude (0, blockSize));
                            }
                        }

                        const double deadline = 1.0e6 * static_cast<double> (blockSize)
                                                / static_cast<double> (rate);
                        const auto missed = std::count_if (timings.begin(), timings.end(),
                                                           [&] (double value) { return value > deadline; });
                        std::sort (timings.begin(), timings.end());
                        const int activeVoices = processor.getActiveVoiceCountForTesting();
                        const float peak = maxPeak;
                        std::printf ("FULLCPU rate=%d block=%d voices=%d active=%d os=%s drive=%.0f "
                                     "median_us=%.2f p95_us=%.2f max_us=%.2f deadline_us=%.2f "
                                     "missed=%ld/80 peak=%.4f\n",
                                     rate, blockSize, voices, activeVoices,
                                     aod::Choices::osFactor[osIndex].toStdString().c_str(),
                                     static_cast<double> (drive),
                                     timings[40], timings[75], timings.back(), deadline,
                                     static_cast<long> (missed), static_cast<double> (peak));

                        // A silent or shrunken tail means the patch selection
                        // failed to sustain the load, not that the chain is
                        // fast. CHECK rather than REQUIRE so one bad config
                        // still lets the rest of the matrix report.
                        CHECK (activeVoices == voices);
                        CHECK (std::isfinite (peak));
                        CHECK (peak > 0.0f);
                    }

        processor.releaseResources();
    }
}

/**
    Long-running full-processor scenario matrix.

    This is intentionally a separate hidden benchmark from the short steady
    state baseline above. It runs a representative patch for 30 seconds at
    each rate/block/polyphony corner while mixing note-off, sustain, stealing,
    MIDI performance messages and host-style parameter automation. Every
    rendered sample is checked for finiteness. Timing is reported against the
    same processor with no active voices; the zero-miss value is a target, not
    a portable assertion because other processes can pre-empt this machine.
*/
TEST_CASE ("full processor sustained scenario matrix", "[.][benchmark]")
{
    const juce::File font = findTestFont();
    if (! font.existsAsFile())
        SKIP ("test SF2 corpus not present on this machine");

    const SustainedPatch patch = findSustainedPatch (font);
    if (patch.keys.size() < 32)
        SKIP ("no preset with enough sustained keys in the test font");

    constexpr int durationSeconds = 30;
    for (int rate : { 48000, 96000 })
    {
        for (int blockSize : { 64, 128, 512 })
        {
            for (int requestedVoices : { 32, 128 })
            {
                const int voices = std::min (requestedVoices,
                                             static_cast<int> (patch.keys.size()));
                aod::RomplerProcessor processor;
                processor.setPlayConfigDetails (0, 2, rate, 512);
                processor.prepareToPlay (rate, 512);
                processor.loadSoundFont (font);
                processor.selectPreset (patch.bank, patch.program);
                setDenormalized (processor, aod::ParamIDs::polyLimit,
                                 static_cast<float> (voices));

                const auto noLoad = renderNoLoadBaseline (processor, rate, blockSize, 1);
                const auto run = renderLongRun (processor, patch, rate, blockSize,
                                                voices, durationSeconds, true);
                const double deadline = 1.0e6 * static_cast<double> (blockSize)
                                        / static_cast<double> (rate);

                std::printf (
                    "FULLCPU_LONG rate=%d block=%d voices=%d duration_s=%d "
                    "no_load_p95_us=%.2f no_load_p99_us=%.2f no_load_max_us=%.2f "
                    "no_load_missed=%zu/%zu p95_us=%.2f p99_us=%.2f max_us=%.2f "
                    "deadline_us=%.2f missed=%zu/%zu active_min=%d active_max=%d "
                    "miss_midi=%zu miss_automation=%zu miss_unmarked=%zu "
                    "miss_full_voices=%zu miss_reduced_voices=%zu "
                    "peak=%.4f nonfinite=%zu target_misses=0\n",
                    rate, blockSize, voices, durationSeconds,
                    percentile (noLoad.timings, 0.95), percentile (noLoad.timings, 0.99),
                    percentile (noLoad.timings, 1.0), noLoad.deadlineMisses,
                    noLoad.timings.size(), percentile (run.timings, 0.95),
                    percentile (run.timings, 0.99), percentile (run.timings, 1.0),
                    deadline, run.deadlineMisses, run.timings.size(), run.minActiveVoices,
                    run.maxActiveVoices, run.midiDeadlineMisses,
                    run.automationDeadlineMisses, run.unmarkedDeadlineMisses,
                    run.fullVoiceDeadlineMisses, run.reducedVoiceDeadlineMisses,
                    static_cast<double> (run.maxPeak),
                    run.nonFiniteSamples);

                CHECK (run.timings.size() > 0);
                CHECK (run.nonFiniteSamples == 0);
                CHECK (run.maxActiveVoices > 0);
                CHECK (run.maxActiveVoices <= voices);
                CHECK (std::isfinite (run.maxPeak));
                processor.releaseResources();
            }
        }
    }
}
