#include "BusStage.h"

namespace eon
{

void BusStage::prepare (double sampleRate, int maximumBlockSize, int numChannels)
{
    juce::ignoreUnused (sampleRate);

    preparedChannels_ = juce::jlimit (1, maxChannels, numChannels);

    for (std::size_t i = 0; i < factors.size(); ++i)
    {
        // Stage count is log2 of the factor: 1x is a pass-through with no
        // stages, which JUCE supports and reports zero latency for.
        const int numStages = i == 0 ? 0 : static_cast<int> (i);

        oversamplers_[i] = std::make_unique<juce::dsp::Oversampling<float>> (
            static_cast<std::size_t> (preparedChannels_),
            static_cast<std::size_t> (numStages),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true);

        oversamplers_[i]->initProcessing (static_cast<std::size_t> (maximumBlockSize));
    }

    for (auto& channel : channels_)
        channel.dc.prepare (sampleRate);

    reset();
}

void BusStage::reset() noexcept
{
    for (auto& oversampler : oversamplers_)
        if (oversampler != nullptr)
            oversampler->reset();

    for (auto& channel : channels_)
    {
        channel.tape.reset();
        channel.fold.reset();
        channel.dc.reset();
    }
}

int BusStage::latencySamples (int factorIndex) const
{
    const auto index = static_cast<std::size_t> (juce::jlimit (0, static_cast<int> (factors.size()) - 1, factorIndex));

    if (oversamplers_[index] == nullptr)
        return 0;

    return juce::roundToInt (oversamplers_[index]->getLatencyInSamples());
}

void BusStage::process (juce::AudioBuffer<float>& buffer, float tape01, float fold01, int factorIndex)
{
    const auto index = static_cast<std::size_t> (juce::jlimit (0, static_cast<int> (factors.size()) - 1, factorIndex));
    auto* oversampler = oversamplers_[index].get();

    if (oversampler == nullptr)
        return;

    const float tapePre  = juce::Decibels::decibelsToGain (tape01 * kMaxTapeDb);
    const float tapePost = 1.0f / x10::dsp::curves::Tanh::f (tapePre);
    const float foldPre  = juce::Decibels::decibelsToGain (fold01 * kMaxFoldDb);

    juce::dsp::AudioBlock<float> block (buffer);
    auto upsampled = oversampler->processSamplesUp (block);

    const auto numChannels = juce::jmin (static_cast<std::size_t> (preparedChannels_),
                                         upsampled.getNumChannels());

    for (std::size_t channel = 0; channel < numChannels; ++channel)
    {
        auto& state = channels_[channel];
        auto* data = upsampled.getChannelPointer (channel);

        for (std::size_t i = 0; i < upsampled.getNumSamples(); ++i)
        {
            float x = state.tape.process (data[i] * tapePre) * tapePost;

            // The wavefolder is the identity on [-1, 1], so a fold of zero
            // passes the signal through untouched and needs no bypass branch.
            // Its output is bounded by construction, which is why there is no
            // makeup gain on this one.
            x = state.fold.process (x * foldPre);

            data[i] = x;
        }
    }

    oversampler->processSamplesDown (block);

    for (int channel = 0; channel < juce::jmin (preparedChannels_, buffer.getNumChannels()); ++channel)
    {
        auto& state = channels_[static_cast<std::size_t> (channel)];
        auto* data = buffer.getWritePointer (channel);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
            data[i] = state.dc.process (data[i]);
    }
}

} // namespace eon
