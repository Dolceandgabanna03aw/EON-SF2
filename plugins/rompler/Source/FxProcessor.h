#pragma once

#include <vector>

#include <juce_dsp/juce_dsp.h>

namespace aod
{

/**
    Master FX stage: a stereo chorus feeding a stereo reverb and a tempo-synced
    stereo ping-pong delay, run after the bus nonlinear stage and before the
    output trim.

    Both effects are JUCE's own dsp implementations. They run at the host
    sample rate on the decimated stereo mix, so the oversampled bus stage
    never sees them. Each effect carries its own dry/wet mix: the chorus uses
    setMix() and the reverb balances wetLevel against dryLevel, which keeps
    the gain maths inside JUCE and gives each a soft crossfade on parameter
    changes.
*/
class FxProcessor
{
public:
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    /**
        Process one block in place.

        chorusRateHz: LFO rate in Hz. chorusDepth, chorusMix, reverbRoom,
        reverbDamp, reverbMix and delayMix/delayFeedback are 0-1, straight from
        APVTS (the percent parameters are divided by 100 by the caller).
        delay is fixed at a dotted eighth (1/8D) and follows bpm. A mix of 0
        passes the corresponding effect through dry.
    */
    void process (juce::AudioBuffer<float>& buffer,
                  float chorusRateHz, float chorusDepth, float chorusMix,
                  float reverbRoom, float reverbDamp, float reverbMix,
                  float delayMix, float delayFeedback, float bpm) noexcept;

private:
    juce::dsp::Chorus<float> chorus_;
    juce::dsp::Reverb reverb_;
    std::vector<float> delayLeft_, delayRight_;
    double sampleRate_ = 44100.0;
    int delayWriteIndex_ = 0;
    int delaySamples_ = 1;
    bool prepared_ = false;
};

} // namespace aod
