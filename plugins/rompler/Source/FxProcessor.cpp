#include "FxProcessor.h"

namespace aod
{

void FxProcessor::prepare (double sampleRate, int maximumBlockSize, int numChannels)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (maximumBlockSize);
    spec.numChannels      = static_cast<juce::uint32> (numChannels);

    chorus_.prepare (spec);
    reverb_.prepare (spec);
    reset();
    prepared_ = true;
}

void FxProcessor::reset()
{
    chorus_.reset();
    reverb_.reset();
}

void FxProcessor::process (juce::AudioBuffer<float>& buffer,
                           float chorusRateHz, float chorusDepth, float chorusMix,
                           float reverbRoom, float reverbDamp, float reverbMix) noexcept
{
    if (!prepared_ || buffer.getNumSamples() == 0)
        return;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    chorus_.setRate (chorusRateHz);
    chorus_.setDepth (chorusDepth);
    chorus_.setMix (chorusMix);
    chorus_.process (context);

    auto params = reverb_.getParameters();
    params.roomSize   = reverbRoom;
    params.damping    = reverbDamp;
    params.wetLevel   = reverbMix;
    params.dryLevel   = 1.0f - reverbMix;
    params.width      = 1.0f;
    params.freezeMode = 0.0f;
    reverb_.setParameters (params);
    reverb_.process (context);
}

} // namespace aod
