#include "Sampler.h"
#include <algorithm>
#include <cmath>

namespace aod
{

void Voice::start(const Sample* sample, int midiNote, float velocity) noexcept
{
    sample_ = sample;
    velocity_ = velocity;
    midiNote_ = midiNote;
    phase_ = 0.0;
    envPhase_ = 0.0f;
    active_ = true;
    releasing_ = false;
    releaseLevel_ = 0.0f;
    filterNeedsPrepare_ = true;
}

void Voice::stop() noexcept
{
    if (!active_ || releasing_)
        return;
    releasing_ = true;
    releasePhase_ = envPhase_;
    releaseLevel_ = envelope();
}

float Voice::envelope() const noexcept
{
    constexpr float attackTime = 0.01f;
    constexpr float decayTime = 0.3f;
    constexpr float sustainLevel = 0.7f;

    float level;
    if (envPhase_ < attackTime)
        level = envPhase_ / attackTime;
    else if (envPhase_ < attackTime + decayTime)
        level = 1.0f - (envPhase_ - attackTime) / decayTime * (1.0f - sustainLevel);
    else
        level = sustainLevel;

    if (releasing_)
    {
        const float t = (envPhase_ - releasePhase_) / releaseTime;
        if (t >= 1.0f)
            return 0.0f;
        return releaseLevel_ * (1.0f - t);
    }

    return level;
}

void Voice::render(float* output, int numSamples, int hostSampleRate, float driveDb, float velToDriveDb,
                    int curveId, int filterRouting, float filterOffsetCents) noexcept
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

    // Velocity shapes the drive amount: velToDriveDb at 0% is neutral, +100%
    // makes hard hits drive harder and -100% does the inverse. This is an
    // additional dB offset centred so a velocity of 127 (1.0) is the reference.
    const float velDriveDb = driveDb + velToDriveDb * (velocity_ - 1.0f);
    const float driveGain = std::pow (10.0f, velDriveDb / 20.0f);

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

        // Deactivate once the release fade has fully ramped to zero; the
        // envelope becomes 0.0 at that point, so stop burning samples early.
        if (releasing_ && envPhase_ - releasePhase_ >= releaseTime)
        {
            active_ = false;
            break;
        }

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

void VoicePool::setPolyphony(int numVoices) noexcept
{
    polyphony_ = juce::jlimit (1, static_cast<int>(voices_.size()), numVoices);
}

void VoicePool::start(const Sample* sample, int midiNote, float velocity) noexcept
{
    if (midiNote < 0 || midiNote >= 128)
        return;

    // Retrigger: if this note already owns a voice, reset it in place instead
    // of allocating a fresh slot. Without this, mashing one key consumes a new
    // voice per press and the old voice keeps ringing underneath.
    const int existing = noteToVoice_[static_cast<std::size_t>(midiNote)];
    if (existing >= 0 && static_cast<std::size_t>(existing) < voices_.size()
        && voices_[static_cast<std::size_t>(existing)].note() == midiNote
        && voices_[static_cast<std::size_t>(existing)].isActive())
    {
        voices_[static_cast<std::size_t>(existing)].start (sample, midiNote, velocity);
        return;
    }

    Voice* voice = findFreeVoice();
    if (voice == nullptr)
        return;

    voice->start (sample, midiNote, velocity);
    noteToVoice_[static_cast<std::size_t>(midiNote)] =
        static_cast<int>(voice - voices_.data());
}

void VoicePool::stop(int midiNote) noexcept
{
    if (midiNote < 0 || midiNote >= 128)
        return;

    const int voiceIdx = noteToVoice_[static_cast<std::size_t>(midiNote)];
    // Guard against a stale index: the slot may have been recycled for a
    // different note since this note's note-off, so only release it if it is
    // still actually sounding this note.
    if (voiceIdx >= 0 && static_cast<std::size_t>(voiceIdx) < voices_.size()
        && voices_[static_cast<std::size_t>(voiceIdx)].note() == midiNote)
        voices_[static_cast<std::size_t>(voiceIdx)].stop();
}

void VoicePool::stopAll() noexcept
{
    for (auto& voice : voices_)
        voice.stop();
}

Voice* VoicePool::findFreeVoice() noexcept
{
    const auto limit = std::min (static_cast<std::size_t>(polyphony_), voices_.size());

    // First pass: an entirely idle slot.
    for (std::size_t i = 0; i < limit; ++i)
        if (!voices_[i].isActive())
            return &voices_[i];

    // Second pass: a slot still rendering its release tail. Reallocating it is
    // preferable to silently dropping the new note, and re-triggering merely
    // overrides the fade with the fresh attack.
    for (std::size_t i = 0; i < limit; ++i)
        if (voices_[i].isReleasing())
            return &voices_[i];

    return nullptr;
}

void VoicePool::render(float* output, int numSamples, int hostSampleRate, float driveDb, float velToDriveDb,
                        int curveId, int filterRouting, float filterOffsetCents) noexcept
{
    std::fill(output, output + numSamples, 0.0f);

    const auto limit = std::min (static_cast<std::size_t>(polyphony_), voices_.size());
    for (std::size_t i = 0; i < limit; ++i)
        if (voices_[i].isActive())
            voices_[i].render(output, numSamples, hostSampleRate, driveDb, velToDriveDb, curveId, filterRouting, filterOffsetCents);
}

} // namespace aod
