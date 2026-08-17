#pragma once

#include <juce_dsp/juce_dsp.h>

namespace aod
{

/**
    Master FX stage: a stereo chorus feeding a stereo reverb, run after the
    bus nonlinear stage and before the output trim.

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

        chorusRateHz: LFO rate in Hz.
        chorusDepth, chorusMix, reverbRoom, reverbDamp, reverbMix: 0-1,
        straight from APVTS (the percent parameters are divided by 100 by the
        caller). A mix of 0 passes the block through untouched.
    */
    void process (juce::AudioBuffer<float>& buffer,
                  float chorusRateHz, float chorusDepth, float chorusMix,
                  float reverbRoom, float reverbDamp, float reverbMix) noexcept;

private:
    juce::dsp::Chorus<float> chorus_;
    juce::dsp::Reverb reverb_;
    bool prepared_ = false;
};

} // namespace aod
