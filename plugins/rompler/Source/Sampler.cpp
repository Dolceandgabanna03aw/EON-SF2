#include "Sampler.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace aod
{

namespace
{
    // Fixed musical vibrato rate applied by CC1 (mod wheel). Not tempo-synced
    // or user-configurable per the spec; a gentle ~5.5 Hz reads as natural
    // vocal/string-style vibrato without sounding like a tremolo effect.
    constexpr float kVibratoRateHz = 5.5f;
}

double Voice::computePlayRate(const Sample* sample, int midiNote) const noexcept
{
    const double semitones = static_cast<double> (midiNote - sample->rootKey)
        * static_cast<double> (sample->scaleTuningCentsPerKey) / 100.0
        + static_cast<double> (sample->tuneCents) / 100.0;
    return std::pow (2.0, semitones / 12.0);
}

void Voice::start(const Sample* sample, int midiNote, float velocity) noexcept
{
    sample_ = sample;
    velocity_ = velocity;
    midiNote_ = midiNote;
    phase_ = 0.0;
    envPhase_ = 0.0f;
    envelopeLevel_ = 0.0f;
    vibratoPhase_ = 0.0;
    active_ = true;
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
    playRate_ = computePlayRate (sample, midiNote);

    adsr_.noteOn();
}

void Voice::retarget(const Sample* sample, int midiNote) noexcept
{
    if (!active_)
        return;

    // Same-sample legato preserves the current read position and envelope.
    // A different sample has different bounds and loop metadata, so retaining
    // the old phase/loop cache could index beyond the replacement buffer.
    if (sample_ != sample)
    {
        phase_ = 0.0;
        loopStart_ = sample->loopStart;
        loopEnd_ = sample->loopEnd;
        loopEnabled_ = sample->loopEnabled && loopEnd_ > loopStart_ + 1;
    }

    sample_ = sample;
    midiNote_ = midiNote;
    playRate_ = computePlayRate (sample, midiNote);
}

void Voice::retire() noexcept
{
    active_ = false;
    midiNote_ = -1;
    sample_ = nullptr;
    envelopeLevel_ = 0.0f;
}

void Voice::stop() noexcept
{
    if (!active_)
        return;
    adsr_.noteOff();
}

bool Voice::isReleasing() const noexcept
{
    return active_ && adsr_.stage() == x10::dsp::Adsr::Stage::Release;
}

void Voice::render(float* output, int numSamples, int hostSampleRate, float driveDb, float velToDriveDb,
                    int curveId, int filterRouting, float filterOffsetCents,
                    float attackMs, float decayMs, float sustainLevel, float releaseMs,
                    float pitchBendSemitones, float vibratoDepthCents) noexcept
{
    if (!active_)
    {
        envelopeLevel_ = 0.0f;
        return;
    }

    if (sample_ == nullptr || sample_->data.empty())
    {
        active_ = false;
        envelopeLevel_ = 0.0f;
        return;
    }

    if (filterNeedsPrepare_ || filterSampleRate_ != hostSampleRate)
    {
        filter_.prepare (static_cast<double> (hostSampleRate));
        filterSampleRate_ = hostSampleRate;
        filterNeedsPrepare_ = false;
        // Prepare the envelope only on rate changes, not per block: prepare()
        // recomputes the current stage's increment, and doing that mid-Decay or
        // mid-Release every block would restart the ramp from the current
        // level, stretching what should be a fixed-time fade indefinitely.
        adsr_.prepare (static_cast<double> (hostSampleRate));
    }

    // Push ADSR parameters only on change: the setters recompute the current
    // stage's ramp even for identical values, which would restart a Decay or
    // Release fade from the current level every block.
    const float envParams[] = { attackMs, decayMs, sustainLevel, releaseMs };
    std::uint32_t hash = 2166136261u;
    for (float v : envParams)
    {
        std::uint32_t bits = 0;
        static_assert (sizeof (bits) == sizeof (v), "expected 32-bit float");
        std::memcpy (&bits, &v, sizeof (bits));
        hash = (hash ^ bits) * 16777619u;
    }
    if (hash != envParamHash_)
    {
        envParamHash_ = hash;
        adsr_.setAttackSec (attackMs * 0.001f);
        adsr_.setDecaySec (decayMs * 0.001f);
        adsr_.setSustainLevel (sustainLevel);
        adsr_.setReleaseSec (releaseMs * 0.001f);
    }

    const float cutoffHz = std::clamp (
        sample_->filterCutoffHz * std::pow (2.0f, filterOffsetCents / 1200.0f),
        20.0f, static_cast<float> (hostSampleRate) * 0.49f);
    const float q = std::pow (10.0f, sample_->filterResonanceDb / 20.0f) * 0.7071068f;
    filter_.setCutoff (cutoffHz, q);

    const float* sampleData = sample_->data.data();
    const auto sampleCount = static_cast<std::int64_t>(sample_->data.size());

    // Velocity shapes the drive amount: velToDriveDb at 0% is neutral, +100%
    // makes hard hits drive harder and -100% does the inverse. This is an
    // additional dB offset centred so a velocity of 127 (1.0) is the reference.
    const float velDriveDb = driveDb + velToDriveDb * (velocity_ - 1.0f);
    const float driveGain = std::pow (10.0f, velDriveDb / 20.0f);

    // Loop points as sample-frame indices into sampleData. While looping, phase_
    // wraps from loopEnd_ back to loopStart_ so sustained notes never run off
    // the end of the sample; during Release the loop is ignored and the tail
    // plays out so the ADSR release has real data to fade.
    const bool inRelease = adsr_.stage() == x10::dsp::Adsr::Stage::Release;
    const bool looping = loopEnabled_ && !inRelease;
    const auto loopStart = static_cast<std::int64_t>(loopStart_);
    const auto loopEnd = static_cast<std::int64_t>(loopEnd_);

    // Vibrato LFO angular increment per sample at the fixed musical rate.
    const double vibratoIncrement =
        2.0 * juce::MathConstants<double>::pi * static_cast<double> (kVibratoRateHz)
        / static_cast<double> (hostSampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        const float env = adsr_.tick();
        envelopeLevel_ = env;
        if (env <= 0.0f && !adsr_.isActive())
        {
            active_ = false;
            envelopeLevel_ = 0.0f;
            break;
        }

        // Pitch bend and vibrato are combined as an additional semitone
        // offset applied per sample, on top of the cached playRate_. This
        // keeps a live wheel/CC1 change instantaneous without recomputing or
        // resetting the cached rate (which would otherwise jump the read
        // phase).
        const float vibratoSemitones =
            (vibratoDepthCents * static_cast<float> (std::sin (vibratoPhase_))) / 100.0f;
        const double pitchRatio = std::pow (2.0,
            (static_cast<double> (pitchBendSemitones) + static_cast<double> (vibratoSemitones)) / 12.0);
        const double effectiveRate = playRate_ * pitchRatio;

        if (!looping)
        {
            const auto index = static_cast<std::int64_t>(phase_);
            if (index >= sampleCount - 1)
            {
                if (inRelease)
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
                    envelopeLevel_ = 0.0f;
                    break;
                }
            }
        }

        const auto index = static_cast<std::int64_t>(phase_);
        const float frac = static_cast<float>(phase_ - static_cast<double>(index));
        const float s0 = sampleData[static_cast<std::size_t>(index)];
        const auto s1Index = (index + 1 < sampleCount) ? index + 1 : sampleCount - 1;
        const float s1 = sampleData[static_cast<std::size_t>(s1Index)];
        const float interpolated = s0 + frac * (s1 - s0);

        float sample = interpolated * velocity_ * env;

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

        phase_ += effectiveRate;
        vibratoPhase_ += vibratoIncrement;
        if (vibratoPhase_ >= 2.0 * juce::MathConstants<double>::pi)
            vibratoPhase_ -= 2.0 * juce::MathConstants<double>::pi;

        // Wrap the loop: once the read position passes loopEnd_, continue from
        // loopStart_ keeping the fractional part, so the interpolation phase is
        // continuous across the wrap and the loop does not click.
        if (looping && phase_ >= static_cast<double>(loopEnd))
            phase_ -= static_cast<double>(loopEnd - loopStart);

        envPhase_ += 1.0f / static_cast<float>(hostSampleRate);
    }
}

int VoicePool::activeVoiceCount() const noexcept
{
    return static_cast<int> (std::count_if (voices_.begin(), voices_.end(), [] (const Voice& voice)
    {
        return voice.isActive();
    }));
}

int VoicePool::voiceIndexForNote(int midiNote) const noexcept
{
    if (midiNote < 0 || midiNote >= 128)
        return -1;

    const int voiceIndex = noteToVoice_[static_cast<std::size_t>(midiNote)];
    if (voiceIndex < 0 || static_cast<std::size_t>(voiceIndex) >= voices_.size())
        return -1;

    const Voice& voice = voices_[static_cast<std::size_t>(voiceIndex)];
    return voice.isActive() && voice.note() == midiNote ? voiceIndex : -1;
}

void VoicePool::setPolyphony(int numVoices) noexcept
{
    const int newLimit = juce::jlimit (1, static_cast<int>(voices_.size()), numVoices);
    if (newLimit >= polyphony_)
    {
        polyphony_ = newLimit;
        return;
    }

    // Slots outside a newly reduced limit are not rendered. Retire them now
    // so they cannot remain frozen, revive later, or retain MIDI state.
    for (std::size_t i = static_cast<std::size_t>(newLimit); i < voices_.size(); ++i)
    {
        const int note = voices_[i].note();
        if (note >= 0 && note < 128 && noteToVoice_[static_cast<std::size_t>(note)] == static_cast<int>(i))
        {
            noteToVoice_[static_cast<std::size_t>(note)] = -1;
            pendingRelease_[static_cast<std::size_t>(note)] = false;
            if (keyHeld_[static_cast<std::size_t>(note)])
            {
                keyHeld_[static_cast<std::size_t>(note)] = false;
                if (heldKeyCount_ > 0)
                    --heldKeyCount_;
            }
        }
        voices_[i].retire();
    }
    for (int note = 0; note < 128; ++note)
    {
        const auto noteIndex = static_cast<std::size_t>(note);
        if (noteToVoice_[noteIndex] < newLimit)
            continue;

        noteToVoice_[noteIndex] = -1;
        pendingRelease_[noteIndex] = false;
    }
    if (leadVoiceIndex_ >= newLimit)
        leadVoiceIndex_ = -1;
    polyphony_ = newLimit;
}

void VoicePool::setSustainHeld(bool held) noexcept
{
    if (sustainHeld_ == held)
        return;
    sustainHeld_ = held;

    if (!sustainHeld_)
    {
        // Pedal released: flush every note whose note-off was deferred while
        // it was held.
        for (int note = 0; note < 128; ++note)
        {
            if (pendingRelease_[static_cast<std::size_t>(note)])
            {
                pendingRelease_[static_cast<std::size_t>(note)] = false;
                releaseNote (note);
            }
        }
    }
}

void VoicePool::setLegatoEnabled(bool enabled) noexcept
{
    legatoEnabled_ = enabled;
}

void VoicePool::startVoice(Voice& voice, const Sample* sample, int midiNote, float velocity) noexcept
{
    const int voiceIndex = static_cast<int> (&voice - voices_.data());
    const int victimNote = voice.note();
    if (victimNote >= 0 && victimNote < 128
        && noteToVoice_[static_cast<std::size_t>(victimNote)] == voiceIndex)
        noteToVoice_[static_cast<std::size_t>(victimNote)] = -1;

    voice.start (sample, midiNote, velocity);
    voice.setStartSequence (nextStartSequence_++);
    noteToVoice_[static_cast<std::size_t>(midiNote)] = voiceIndex;
    leadVoiceIndex_ = voiceIndex;
}

void VoicePool::start(const Sample* sample, int midiNote, float velocity) noexcept
{
    if (midiNote < 0 || midiNote >= 128)
        return;

    // A note-on always clears any pending deferred release for that same
    // note number: if it was released and re-pressed while the pedal was
    // still held, the pedal's earlier note-off must not later steal the
    // fresh attack out from under the new press.
    pendingRelease_[static_cast<std::size_t>(midiNote)] = false;

    const bool alreadyHeld = keyHeld_[static_cast<std::size_t>(midiNote)];
    if (!alreadyHeld)
    {
        keyHeld_[static_cast<std::size_t>(midiNote)] = true;
        ++heldKeyCount_;
    }

    // Legato: with the mode on and at least one other key already held
    // (i.e. this is not the first note of a new phrase) retarget the current
    // lead voice in place instead of triggering a fresh envelope, so the
    // pitch glides on the same voice.
    if (legatoEnabled_ && !alreadyHeld && heldKeyCount_ > 1
        && leadVoiceIndex_ >= 0 && static_cast<std::size_t>(leadVoiceIndex_) < voices_.size()
        && voices_[static_cast<std::size_t>(leadVoiceIndex_)].isActive())
    {
        Voice& lead = voices_[static_cast<std::size_t>(leadVoiceIndex_)];
        const int previousNote = lead.note();
        lead.retarget (sample, midiNote);
        if (previousNote >= 0 && previousNote < 128 && previousNote != midiNote
            && noteToVoice_[static_cast<std::size_t>(previousNote)] == leadVoiceIndex_)
            noteToVoice_[static_cast<std::size_t>(previousNote)] = -1;
        noteToVoice_[static_cast<std::size_t>(midiNote)] = leadVoiceIndex_;
        return;
    }

    // Retrigger: if this note already owns a voice, reset it in place instead
    // of allocating a fresh slot. Without this, mashing one key consumes a new
    // voice per press and the old voice keeps ringing underneath.
    const int existing = noteToVoice_[static_cast<std::size_t>(midiNote)];
    if (existing >= 0 && static_cast<std::size_t>(existing) < voices_.size()
        && voices_[static_cast<std::size_t>(existing)].note() == midiNote
        && voices_[static_cast<std::size_t>(existing)].isActive())
    {
        startVoice (voices_[static_cast<std::size_t>(existing)], sample, midiNote, velocity);
        return;
    }

    Voice* voice = findFreeVoice();
    if (voice == nullptr)
        return;

    startVoice (*voice, sample, midiNote, velocity);
}

void VoicePool::stop(int midiNote) noexcept
{
    if (midiNote < 0 || midiNote >= 128)
        return;

    if (midiNote >= 0 && midiNote < 128)
    {
        if (keyHeld_[static_cast<std::size_t>(midiNote)])
        {
            keyHeld_[static_cast<std::size_t>(midiNote)] = false;
            if (heldKeyCount_ > 0)
                --heldKeyCount_;
        }
    }

    if (sustainHeld_)
    {
        // Defer the actual release until the pedal comes up; the voice keeps
        // sounding (and, per spec, remains eligible for the note's own
        // eventual release, not a hard cut).
        pendingRelease_[static_cast<std::size_t>(midiNote)] = true;
        return;
    }

    releaseNote (midiNote);
}

void VoicePool::releaseNote(int midiNote) noexcept
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
    keyHeld_.fill (false);
    heldKeyCount_ = 0;
    pendingRelease_.fill (false);
    leadVoiceIndex_ = -1;
}

Voice* VoicePool::findFreeVoice() noexcept
{
    const auto limit = std::min (static_cast<std::size_t>(polyphony_), voices_.size());

    // First pass: an entirely idle slot.
    for (std::size_t i = 0; i < limit; ++i)
        if (!voices_[i].isActive())
            return &voices_[i];

    const auto quieterThenOlder = [this] (std::size_t candidate, std::size_t incumbent)
    {
        const Voice& candidateVoice = voices_[candidate];
        const Voice& incumbentVoice = voices_[incumbent];
        const float candidateLevel = candidateVoice.envelopeLevel();
        const float incumbentLevel = incumbentVoice.envelopeLevel();
        return candidateLevel < incumbentLevel
            || (!(incumbentLevel < candidateLevel)
                && candidateVoice.startSequence() < incumbentVoice.startSequence());
    };

    // Second pass: sacrifice the quietest release tail first, breaking equal
    // envelope levels by attack age to keep behavior reproducible.
    std::size_t releaseVictim = limit;
    for (std::size_t i = 0; i < limit; ++i)
        if (voices_[i].isReleasing()
            && (releaseVictim == limit || quieterThenOlder (i, releaseVictim)))
            releaseVictim = i;
    if (releaseVictim != limit)
        return &voices_[releaseVictim];

    // Finally choose the quietest active voice. Prefer an equally quiet
    // unprotected voice over the legato lead or a physically held key, while
    // still allowing a materially quieter protected voice to be selected.
    std::size_t protectedVictim = limit;
    std::size_t unprotectedVictim = limit;
    for (std::size_t i = 0; i < limit; ++i)
    {
        if (!voices_[i].isActive())
            continue;
        std::size_t& victim = isProtectedFromStealing (i) ? protectedVictim : unprotectedVictim;
        if (victim == limit || quieterThenOlder (i, victim))
            victim = i;
    }

    if (unprotectedVictim == limit)
        return protectedVictim == limit ? nullptr : &voices_[protectedVictim];
    if (protectedVictim == limit
        || !(voices_[protectedVictim].envelopeLevel() < voices_[unprotectedVictim].envelopeLevel()))
        return &voices_[unprotectedVictim];
    return &voices_[protectedVictim];
}

bool VoicePool::isProtectedFromStealing(std::size_t voiceIndex) const noexcept
{
    if (static_cast<int>(voiceIndex) == leadVoiceIndex_)
        return true;

    const int note = voices_[voiceIndex].note();
    return note >= 0 && note < 128 && keyHeld_[static_cast<std::size_t>(note)];
}

void VoicePool::render(float* output, int numSamples, int hostSampleRate, float driveDb, float velToDriveDb,
                        int curveId, int filterRouting, float filterOffsetCents,
                        float attackMs, float decayMs, float sustainLevel, float releaseMs,
                        float pitchBendSemitones, float vibratoDepthCents) noexcept
{
    std::fill(output, output + numSamples, 0.0f);

    const auto limit = std::min (static_cast<std::size_t>(polyphony_), voices_.size());
    for (std::size_t i = 0; i < limit; ++i)
        if (voices_[i].isActive())
        {
            voices_[i].render(output, numSamples, hostSampleRate, driveDb, velToDriveDb,
                              curveId, filterRouting, filterOffsetCents,
                              attackMs, decayMs, sustainLevel, releaseMs,
                              pitchBendSemitones, vibratoDepthCents);
            if (!voices_[i].isActive())
            {
                const int note = voices_[i].note();
                if (note >= 0 && note < 128
                    && noteToVoice_[static_cast<std::size_t>(note)] == static_cast<int>(i))
                    noteToVoice_[static_cast<std::size_t>(note)] = -1;
            }
        }
}

} // namespace aod
