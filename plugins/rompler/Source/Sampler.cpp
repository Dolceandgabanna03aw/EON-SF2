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

    // Copy loop points at start(): render() must not read through a sample
    // pointer that may belong to a retired loader once this voice is retriggered
    // against a newer one. Keeping the loop state here makes the audio thread
    // self-contained for the voice's lifetime.
    loopStart_ = sample->loopStart;
    loopEnd_   = sample->loopEnd;
    loopEnabled_ = sample->loopEnabled && loopEnd_ > loopStart_ + 1;

    // Pitch: the sample is recorded at rootKey. A note played N semitones above
    // rootKey must advance N semitones faster (pitch ratio 2^(N/12)); the
    // region's per-key scale (usually 100 cents/key) and constant tune offset
    // are folded in so a scale of 0 pins every note to the root pitch.
    const double semitones = static_cast<double> (midiNote - sample->rootKey)
        * static_cast<double> (sample->scaleTuningCentsPerKey) / 100.0
        + static_cast<double> (sample->tuneCents) / 100.0;
    playRate_ = std::pow (2.0, semitones / 12.0);
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
        // Scale the release ramp duration by the level at release time: a note
        // released mid-attack (level 0.5) fades over half the nominal release
        // time, so the *slope* of the fade is the same as a full-level release.
        // A fixed-time ramp from a low level is a much sharper slope and clicks;
        // this keeps the fade audibly consistent whatever the release level.
        const float rampTime = releaseTime * std::max (releaseLevel_, 0.05f);
        const float t = (envPhase_ - releasePhase_) / rampTime;
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

    // Loop points as sample-frame indices into sampleData. While looping, phase_
    // wraps from loopEnd_ back to loopStart_ so sustained notes never run off
    // the end of the sample; releasing ignores the loop and plays the tail out
    // past loopEnd_ so the release envelope has real data to fade.
    const bool looping = loopEnabled_ && !releasing_;
    const auto loopStart = static_cast<std::int64_t>(loopStart_);
    const auto loopEnd = static_cast<std::int64_t>(loopEnd_);

    for (int i = 0; i < numSamples; ++i)
    {
        if (!looping)
        {
            const auto index = static_cast<std::int64_t>(phase_);
            if (index >= sampleCount - 1)
            {
                if (releasing_)
                {
                    // While releasing, hold the last sample position instead of
                    // falling off the end of the buffer: the fade must run to
                    // zero on its own, or the waveform is truncated mid-cycle
                    // and clicks.
                    phase_ = static_cast<double>(sampleCount - 1);
                }
                else
                {
                    active_ = false;
                    break;
                }
            }
        }

        const auto index = static_cast<std::int64_t>(phase_);
        const float frac = static_cast<float>(phase_ - static_cast<double>(index));
        const float s0 = sampleData[static_cast<std::size_t>(index)];
        // Reading s1 needs one sample of headroom; a releasing voice holds
        // phase_ at sampleCount - 1, so clamp here to stay in bounds.
        const auto s1Index = (index + 1 < sampleCount) ? index + 1 : sampleCount - 1;
        const float s1 = sampleData[static_cast<std::size_t>(s1Index)];
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

        phase_ += playRate_;

        // Wrap the loop: once the read position passes loopEnd_, continue from
        // loopStart_ keeping the fractional part, so the interpolation phase is
        // continuous across the wrap and the loop does not click.
        if (looping && phase_ >= static_cast<double>(loopEnd))
            phase_ -= static_cast<double>(loopEnd - loopStart);

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

    // Third pass: the whole pool is busy with sustained notes. Steal the
    // *oldest* active voice so the new note is never silently dropped; the
    // oldest has decayed the furthest, so it is the least audible victim.
    // (Voice::start() rewrites all state, so the steal is click-free apart
    // from the natural note cut.)
    std::size_t oldest = 0;
    for (std::size_t i = 1; i < limit; ++i)
        if (voices_[i].isActive())
            oldest = i;
    return &voices_[oldest];
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
