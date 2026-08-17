#include "Sampler.h"
#include <algorithm>
#include <cmath>

namespace aod
{

void Voice::start(const Sample* sample, float velocity) noexcept
{
    sample_ = sample;
    velocity_ = velocity;
    phase_ = 0.0;
    envPhase_ = 0.0f;
    active_ = true;
    filterNeedsPrepare_ = true;
}

void Voice::stop() noexcept
{
    active_ = false;
}

float Voice::envelope() const noexcept
{
    constexpr float attackTime = 0.01f;
    constexpr float decayTime = 0.3f;
    constexpr float sustainLevel = 0.7f;

    if (envPhase_ < attackTime)
        return envPhase_ / attackTime;
    if (envPhase_ < attackTime + decayTime)
        return 1.0f - (envPhase_ - attackTime) / decayTime * (1.0f - sustainLevel);
    return sustainLevel;
}

void Voice::render(float* output, int numSamples, int hostSampleRate, float driveDb, int curveId,
                    int filterRouting, float filterOffsetCents) noexcept
{
    if (!active_ || sample_ == nullptr || sample_->data.empty())
        return;

    if (filterNeedsPrepare_ || filterSampleRate_ != hostSampleRate)
    {
        filter_.prepare (static_cast<double> (hostSampleRate));
        filterSampleRate_ = hostSampleRate;
        filterNeedsPrepare_ = false;
    }

    const float cutoffHz = std::clamp (
        sample_->filterCutoffHz * std::pow (2.0f, filterOffsetCents / 1200.0f),
        20.0f, static_cast<float> (hostSampleRate) * 0.49f);
    const float q = std::pow (10.0f, sample_->filterResonanceDb / 20.0f) * 0.7071068f;
    filter_.setCutoff (cutoffHz, q);

    const float* sampleData = sample_->data.data();
    const auto sampleCount = static_cast<std::int64_t>(sample_->data.size());
    const float invHostSampleRate = 1.0f / static_cast<float>(hostSampleRate);
    const float driveGain = std::pow(10.0f, driveDb / 20.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        const auto index = static_cast<std::int64_t>(phase_);
        if (index >= sampleCount - 1)
        {
            active_ = false;
            break;
        }

        const float frac = static_cast<float>(phase_ - static_cast<double>(index));
        const float s0 = sampleData[static_cast<std::size_t>(index)];
        const float s1 = sampleData[static_cast<std::size_t>(index) + 1];
        const float interpolated = s0 + frac * (s1 - s0);

        float sample = interpolated * velocity_ * envelope();

        if (filterRouting == 0) // Pre: filter before drive
            sample = filter_.process (sample);

        // Apply nonlinear drive based on curve ID
        const float driven = driveGain * sample;
        if (curveId == 1)
            sample = x10::dsp::curves::Tube::f (driven) / driveGain;
        else if (curveId == 2)
            sample = x10::dsp::curves::Transformer::f (driven) / driveGain;
        else // curveId == 0 or default
            sample = x10::dsp::curves::Tanh::f (driven) / driveGain;

        if (filterRouting != 0) // Post: filter after drive
            sample = filter_.process (sample);

        output[i] += sample;

        phase_ += 1.0;
        envPhase_ += invHostSampleRate;
    }
}

void VoicePool::start(const Sample* sample, int midiNote, float velocity) noexcept
{
    if (midiNote < 0 || midiNote >= 128)
        return;

    Voice* voice = findFreeVoice();
    if (voice == nullptr)
        return;

    voice->start(sample, velocity);
    noteToVoice_[static_cast<std::size_t>(midiNote)] =
        static_cast<int>(voice - voices_.data());
}

void VoicePool::stop(int midiNote) noexcept
{
    if (midiNote < 0 || midiNote >= 128)
        return;

    const int voiceIdx = noteToVoice_[static_cast<std::size_t>(midiNote)];
    if (voiceIdx >= 0 && static_cast<std::size_t>(voiceIdx) < voices_.size())
        voices_[static_cast<std::size_t>(voiceIdx)].stop();
}

void VoicePool::stopAll() noexcept
{
    for (auto& voice : voices_)
        voice.stop();
}

Voice* VoicePool::findFreeVoice() noexcept
{
    for (auto& voice : voices_)
        if (!voice.isActive())
            return &voice;
    return nullptr;
}

void VoicePool::render(float* output, int numSamples, int hostSampleRate, float driveDb, int curveId,
                        int filterRouting, float filterOffsetCents) noexcept
{
    std::fill(output, output + numSamples, 0.0f);

    for (auto& voice : voices_)
        if (voice.isActive())
            voice.render(output, numSamples, hostSampleRate, driveDb, curveId, filterRouting, filterOffsetCents);
}

} // namespace aod
