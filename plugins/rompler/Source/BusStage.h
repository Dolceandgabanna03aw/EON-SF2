#pragma once

#include <juce_dsp/juce_dsp.h>

#include <x10/dsp/filter/DCBlocker.h>
#include <x10/dsp/nonlinear/Adaa1.h>
#include <x10/dsp/nonlinear/Curves.h>

#include <array>
#include <memory>

namespace eon
{

/**
    The summed-output stage: tape saturation into a wavefolder, both oversampled.

    ADAA already suppresses most of the aliasing these curves would produce, but
    the wavefolder's derivative is discontinuous at every fold and first-order
    ADAA alone does not clear it at high fold amounts — hence the oversampling
    parameter in front of both.

    An oversampler is prepared for every factor at prepare() time rather than
    rebuilt when the parameter moves, because rebuilding allocates and the
    parameter can change from the audio thread. Switching factors then costs an
    array index; only the reported latency has to be pushed to the host, which
    the processor does from the message thread.
*/
class BusStage
{
public:
    /** Factor for each parameter index, matching Choices::osFactor. */
    static constexpr std::array<int, 4> factors { 1, 2, 4, 8 };

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset() noexcept;

    /** Latency of one factor index, in samples at the host rate. */
    [[nodiscard]] int latencySamples (int factorIndex) const;

    /** Processes in place. tape01 and fold01 are 0..1. */
    void process (juce::AudioBuffer<float>& buffer, float tape01, float fold01, int factorIndex);

private:
    /** Drive at the top of each control, in dB into the respective curve. */
    static constexpr float kMaxTapeDb = 24.0f;
    static constexpr float kMaxFoldDb = 18.0f;

    static constexpr int maxChannels = 2;

    struct ChannelState
    {
        x10::dsp::Adaa1<x10::dsp::curves::Tanh> tape;
        x10::dsp::Adaa1<x10::dsp::curves::Wavefolder> fold;
        x10::dsp::DCBlocker dc;
    };

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, factors.size()> oversamplers_;
    std::array<ChannelState, maxChannels> channels_;
    int preparedChannels_ = 0;
};

} // namespace eon
