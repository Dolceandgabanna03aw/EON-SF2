#include "BusProcessor.h"
#include <cmath>

namespace aod
{

void BusProcessor::prepare (double sampleRate, int maximumBlockSize, int numChannels)
{
    sampleRate_ = static_cast<float> (sampleRate);
    for (int factor = 0; factor < numFactors; ++factor)
    {
        oversamplers_[static_cast<std::size_t> (factor)] = std::make_unique<juce::dsp::Oversampling<float>> (
            static_cast<std::size_t> (numChannels), static_cast<std::size_t> (factor),
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        oversamplers_[static_cast<std::size_t> (factor)]->initProcessing (static_cast<std::size_t> (maximumBlockSize));
    }

    dcBlockers_.assign (static_cast<std::size_t> (numChannels), x10::dsp::DCBlocker{});
    for (auto& blocker : dcBlockers_)
        blocker.prepare (sampleRate);

    for (auto& state : filterStates_)
    {
        state.low.assign (static_cast<std::size_t> (numChannels), 0.0f);
        state.band.assign (static_cast<std::size_t> (numChannels), 0.0f);
    }

    smoothedCutoffHz_ = 20000.0f;
    smoothedResonance_ = 0.0f;
}

int BusProcessor::getLatencySamples (int osFactorIndex) const noexcept
{
    osFactorIndex = juce::jlimit (0, numFactors - 1, osFactorIndex);
    const auto& os = oversamplers_[static_cast<std::size_t> (osFactorIndex)];
    return os ? static_cast<int> (std::lround (os->getLatencyInSamples())) : 0;
}

void BusProcessor::reset()
{
    for (auto& os : oversamplers_)
        if (os)
            os->reset();
    for (auto& blocker : dcBlockers_)
        blocker.reset();
    for (auto& state : filterStates_)
    {
        std::fill (state.low.begin(), state.low.end(), 0.0f);
        std::fill (state.band.begin(), state.band.end(), 0.0f);
    }
    smoothedCutoffHz_ = 20000.0f;
    smoothedResonance_ = 0.0f;
}

float BusProcessor::foldSample (float x, float amount) noexcept
{
    if (amount <= 0.0f)
        return x;

    const float k = 1.0f + amount * 4.0f;
    return std::sin (x * k * juce::MathConstants<float>::halfPi);
}

float BusProcessor::processFilterSample (float x, float cutoffHz, float resonance,
                                         float sampleRate, float& low, float& band) noexcept
{
    // This is a deliberately coloured, two-pole state-variable LPF rather
    // than a transparent utility filter. The resonance-dependent bias around
    // the soft clipper creates the slight even/odd harmonic shift heard while
    // sweeping analogue filter cores, without a fixed DC offset at the output.
    const float nyquistSafeCutoff = juce::jlimit (20.0f, sampleRate * 0.225f, cutoffHz);
    const float resonanceAmount = juce::jlimit (0.0f, 1.0f, resonance);
    const float f = juce::jmin (1.82f,
                                2.0f * std::sin (juce::MathConstants<float>::pi
                                                 * nyquistSafeCutoff / sampleRate));
    const float damping = 1.90f - 1.70f * resonanceAmount;

    const float inputGain = 1.0f + 0.95f * resonanceAmount;
    const float bias = 0.13f * resonanceAmount;
    const float biasedClip = std::tanh ((x + bias) * inputGain)
                           - std::tanh (bias * inputGain);

    low += f * band;
    const float high = biasedClip - low - damping * band;
    band += f * high;

    // A softly saturating output stage stops high-resonance sweeps from
    // exploding and adds the moving harmonic contour of a driven filter core.
    return std::tanh (low * (1.0f + 0.32f * resonanceAmount));
}

void BusProcessor::process (juce::AudioBuffer<float>& buffer, float tapeDrivePercent, float foldPercent,
                             float filterCutoffHz, float filterResonancePercent, int osFactorIndex) noexcept
{
    osFactorIndex = juce::jlimit (0, numFactors - 1, osFactorIndex);
    auto& oversampler = *oversamplers_[static_cast<std::size_t> (osFactorIndex)];

    juce::dsp::AudioBlock<float> block (buffer);
    auto oversampledBlock = oversampler.processSamplesUp (block);

    const float driveAmount = tapeDrivePercent / 100.0f;
    const float driveGain = 1.0f + driveAmount * 4.0f;
    const float foldAmount = foldPercent / 100.0f;
    const float targetCutoffHz = juce::jlimit (20.0f, 20000.0f, filterCutoffHz);
    const float targetResonance = juce::jlimit (0.0f, 1.0f, filterResonancePercent / 100.0f);
    const float oversampledRate = static_cast<float> (oversampledBlock.getNumSamples())
                                / static_cast<float> (juce::jmax (1, buffer.getNumSamples()))
                                * sampleRate_;
    const float effectiveSampleRate = oversampledRate > 0.0f ? oversampledRate : sampleRate_;
    const float smoothing = 1.0f - std::exp (-1.0f / (0.015f * effectiveSampleRate));
    auto& filterState = filterStates_[static_cast<std::size_t> (osFactorIndex)];

    for (std::size_t i = 0; i < oversampledBlock.getNumSamples(); ++i)
    {
        const bool filterEnabled = targetCutoffHz < 19950.0f || targetResonance > 0.0001f;
        if (filterEnabled)
        {
            smoothedCutoffHz_ += (targetCutoffHz - smoothedCutoffHz_) * smoothing;
            smoothedResonance_ += (targetResonance - smoothedResonance_) * smoothing;
        }

        for (std::size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
        {
            auto* data = oversampledBlock.getChannelPointer (ch);
            auto& dcBlocker = dcBlockers_[ch < dcBlockers_.size() ? ch : 0];
            float sample = data[i];

            if (driveAmount > 0.0f)
            {
                sample = x10::dsp::curves::Tanh::f (sample * driveGain) / driveGain;
                sample = dcBlocker.process (sample);
            }

            sample = foldSample (sample, foldAmount);

            // Bypass is exact at the stored default so previous patches keep
            // their tone. Once engaged, parameters slew at the oversampled
            // rate, avoiding zippering while retaining the analogue-style
            // harmonic movement of the nonlinear core.
            if (filterEnabled)
            {
                sample = processFilterSample (sample, smoothedCutoffHz_, smoothedResonance_,
                                              effectiveSampleRate, filterState.low[ch], filterState.band[ch]);
            }

            data[i] = sample;
        }
    }

    oversampler.processSamplesDown (block);
}

} // namespace aod
