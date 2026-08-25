#include "Sampler.h"

#include <algorithm>
#include <cmath>

namespace eon
{

namespace
{

/** Constant-power pan, -1 hard left to +1 hard right. */
void panGains (float pan, float& left, float& right) noexcept
{
    const float angle = (juce::jlimit (-1.0f, 1.0f, pan) * 0.5f + 0.5f)
                      * juce::MathConstants<float>::halfPi;
    left  = std::cos (angle);
    right = std::sin (angle);
}

} // namespace

// ---------------------------------------------------------------------------
// AmpEnvelope
// ---------------------------------------------------------------------------

void AmpEnvelope::prepare (double sampleRate) noexcept
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    reset();
}

void AmpEnvelope::reset() noexcept
{
    stage_ = Stage::idle;
    samplesRemaining_ = 0;
    level_ = 0.0f;
    levelIncrement_ = 0.0f;
    decibels_ = kSilenceDb;
    decibelIncrement_ = 0.0f;
    sustainDb_ = 0.0f;
}

int AmpEnvelope::samplesFor (float seconds) const noexcept
{
    return juce::jmax (1, juce::roundToInt (static_cast<double> (seconds) * sampleRate_));
}

void AmpEnvelope::noteOn (const x10::instrument::Envelope& shape) noexcept
{
    shape_ = shape;

    // A sustain of zero is -inf dB. The floor keeps the decay slope finite; a
    // voice that reaches it is finished either way.
    sustainDb_ = shape_.sustainLevel > 0.0f
                   ? juce::jmax (kSilenceDb, juce::Decibels::gainToDecibels (shape_.sustainLevel, kSilenceDb))
                   : kSilenceDb;

    level_ = 0.0f;
    decibels_ = kSilenceDb;
    stage_ = Stage::delay;
    samplesRemaining_ = samplesFor (shape_.delaySeconds);
}

void AmpEnvelope::noteOff() noexcept
{
    if (stage_ == Stage::idle || stage_ == Stage::release)
        return;

    // Release runs in dB from wherever the envelope currently is, so a note
    // released during attack does not jump to full level first.
    decibels_ = juce::jmax (kSilenceDb, juce::Decibels::gainToDecibels (level_, kSilenceDb));
    stage_ = Stage::release;
    samplesRemaining_ = samplesFor (shape_.releaseSeconds);
    decibelIncrement_ = (kSilenceDb - decibels_) / static_cast<float> (samplesRemaining_);
}

void AmpEnvelope::advanceStage() noexcept
{
    switch (stage_)
    {
        case Stage::delay:
            stage_ = Stage::attack;
            samplesRemaining_ = samplesFor (shape_.attackSeconds);
            levelIncrement_ = 1.0f / static_cast<float> (samplesRemaining_);
            level_ = 0.0f;
            break;

        case Stage::attack:
            level_ = 1.0f;
            stage_ = Stage::hold;
            samplesRemaining_ = samplesFor (shape_.holdSeconds);
            break;

        case Stage::hold:
            stage_ = Stage::decay;
            samplesRemaining_ = samplesFor (shape_.decaySeconds);
            decibels_ = 0.0f;
            decibelIncrement_ = (sustainDb_ - decibels_) / static_cast<float> (samplesRemaining_);
            break;

        case Stage::decay:
            stage_ = Stage::sustain;
            decibels_ = sustainDb_;
            level_ = juce::Decibels::decibelsToGain (sustainDb_, kSilenceDb);
            samplesRemaining_ = 0;
            break;

        case Stage::release:
        case Stage::sustain:
        case Stage::idle:
            stage_ = Stage::idle;
            level_ = 0.0f;
            break;
    }
}

float AmpEnvelope::nextValue() noexcept
{
    switch (stage_)
    {
        case Stage::idle:
            return 0.0f;

        case Stage::delay:
            if (--samplesRemaining_ <= 0)
                advanceStage();
            return 0.0f;

        case Stage::attack:
            level_ += levelIncrement_;
            if (--samplesRemaining_ <= 0)
                advanceStage();
            return juce::jlimit (0.0f, 1.0f, level_);

        case Stage::hold:
            if (--samplesRemaining_ <= 0)
                advanceStage();
            return 1.0f;

        case Stage::decay:
            decibels_ += decibelIncrement_;
            level_ = juce::Decibels::decibelsToGain (decibels_, kSilenceDb);
            if (--samplesRemaining_ <= 0)
                advanceStage();
            return level_;

        case Stage::sustain:
            return level_;

        case Stage::release:
            decibels_ += decibelIncrement_;
            level_ = juce::Decibels::decibelsToGain (decibels_, kSilenceDb);
            if (--samplesRemaining_ <= 0)
            {
                stage_ = Stage::idle;
                level_ = 0.0f;
            }
            return level_;
    }

    return 0.0f;
}

// ---------------------------------------------------------------------------
// Voice
// ---------------------------------------------------------------------------

void Voice::prepare (double sampleRate) noexcept
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
    envelope_.prepare (sampleRate);
    filter_.prepare (sampleRate);
    dcBlocker_.prepare (sampleRate);
    reset();
}

void Voice::reset() noexcept
{
    sample_ = nullptr;
    phase_ = 0.0;
    pitchRatio_ = 1.0;
    velocity_ = 0.0f;
    note_ = -1;
    active_ = false;

    envelope_.reset();
    filter_.reset();
    dcBlocker_.reset();
    x10::dsp::resetAll (shapers_);
}

void Voice::start (const Sample* sample, int midiNote, float velocity, std::uint32_t age) noexcept
{
    if (sample == nullptr || sample->data == nullptr || sample->data->empty())
        return;

    sample_ = sample;
    note_ = midiNote;
    velocity_ = velocity;
    age_ = age;
    phase_ = 0.0;
    active_ = true;

    const float semitones = static_cast<float> (midiNote) - sample->rootKey;
    const float cents = semitones * sample->scaleTuningCentsPerKey + sample->tuneCents;

    // The sample is stored at whatever rate it was recorded at, so the phase
    // increment carries the rate conversion as well as the transposition. The
    // loader used to resample every sample to the host rate up front; doing it
    // here costs nothing extra — the interpolating read below was already a
    // resampler — and avoids interpolating the audio twice.
    const double rateRatio = static_cast<double> (sample->sampleRate) / sampleRate_;
    pitchRatio_ = std::pow (2.0, static_cast<double> (cents) / 1200.0) * rateRatio;

    panGains (sample->pan, gainLeft_, gainRight_);
    staticGain_ = juce::Decibels::decibelsToGain (-sample->attenuationDb);

    // Every piece of per-voice state is cleared before reuse. Leaving the ADAA
    // history or the filter's integrators loaded puts an impulse on the first
    // sample after a steal.
    filter_.reset();
    dcBlocker_.reset();
    x10::dsp::resetAll (shapers_);

    const float resonanceQ = juce::jmax (0.5f, juce::Decibels::decibelsToGain (sample->filterResonanceDb) * 0.7071068f);
    filter_.setCutoff (sample->filterCutoffHz, resonanceQ);

    envelope_.noteOn (sample->volumeEnvelope);
}

void Voice::release() noexcept
{
    envelope_.noteOff();
}

void Voice::kill() noexcept
{
    active_ = false;
    envelope_.reset();
}

float Voice::nextSample() noexcept
{
    const auto& data = *sample_->data;
    const auto sampleCount = data.size();

    if (sampleCount < 2)
    {
        active_ = false;
        return 0.0f;
    }

    const bool loops = sample_->loopMode == x10::instrument::LoopMode::continuous
                    || (sample_->loopMode == x10::instrument::LoopMode::sustainThenRelease
                        && ! envelope_.isReleasing());

    // The wrap has to happen before the end-of-data check, not after the read.
    // Checking for the end first stops a sample whose loop ends on its last
    // frame — which is exactly how a one-shot-length loop is written.
    if (loops && sample_->loopEnd > sample_->loopStart)
    {
        // The last frame is reserved as the interpolator's right-hand
        // neighbour, so the loop cannot be allowed to land on it.
        const auto loopEnd = juce::jmin (static_cast<double> (sample_->loopEnd),
                                         static_cast<double> (sampleCount - 1));
        // Clamping can collapse an out-of-range loop down to loopStart or
        // below; there is nothing left to wrap in that case, and a zero or
        // negative length would spin the wrap forever. Fall through so the
        // end-of-data check below deactivates the voice instead.
        if (loopEnd > static_cast<double> (sample_->loopStart))
        {
            // The length must be measured from the same clamped end the wrap
            // uses. Using the raw loopEnd here lets a loop that ends on the
            // last frame wrap phase_ past loopStart and negative, which turns
            // into a huge index on the way into the interpolator below.
            const auto loopLength = loopEnd - static_cast<double> (sample_->loopStart);

            while (phase_ >= loopEnd)
                phase_ -= loopLength;
        }
    }

    if (phase_ >= static_cast<double> (sampleCount - 1))
    {
        active_ = false;
        return 0.0f;
    }

    const auto index = static_cast<std::size_t> (phase_);
    const float frac = static_cast<float> (phase_ - static_cast<double> (index));
    const float s0 = data[index];
    const float s1 = data[index + 1];

    phase_ += pitchRatio_;

    return s0 + frac * (s1 - s0);
}

void Voice::render (float* dryL, float* dryR, float* wetL, float* wetR,
                    int numSamples, const VoiceSettings& settings) noexcept
{
    if (! active_ || sample_ == nullptr || sample_->data == nullptr || sample_->data->empty())
        return;

    // Velocity shifts drive around the knob position rather than scaling it, so
    // a negative velocityToDrive makes hard playing cleaner.
    const float driveAmount = juce::jlimit (0.0f, 1.0f,
        settings.drive01 + settings.velocityToDrive * (velocity_ - 0.5f));

    const float preGain = juce::Decibels::decibelsToGain (driveAmount * kMaxDriveDb);

    const float cutoff = sample_->filterCutoffHz
                       * std::pow (2.0f, settings.filterOffsetCents / 1200.0f);
    const float resonanceQ = juce::jmax (0.5f,
        juce::Decibels::decibelsToGain (sample_->filterResonanceDb) * 0.7071068f);
    filter_.setCutoff (cutoff, resonanceQ);

    switch (settings.curve)
    {
        case VoiceCurve::tanh:
        {
            using C = x10::dsp::curves::Tanh;
            renderCurve<C> (dryL, dryR, wetL, wetR, numSamples, settings,
                            preGain, 1.0f / C::f (preGain));
            break;
        }

        case VoiceCurve::tube:
        {
            using C = x10::dsp::curves::Tube;
            renderCurve<C> (dryL, dryR, wetL, wetR, numSamples, settings,
                            preGain, 1.0f / C::f (preGain));
            break;
        }

        case VoiceCurve::transformer:
        {
            using C = x10::dsp::curves::Transformer;
            renderCurve<C> (dryL, dryR, wetL, wetR, numSamples, settings,
                            preGain, 1.0f / C::f (preGain));
            break;
        }
    }
}

template <class C>
void Voice::renderCurve (float* dryL, float* dryR, float* wetL, float* wetR,
                         int numSamples, const VoiceSettings& settings,
                         float preGain, float postGain) noexcept
{
    auto& shaper = std::get<x10::dsp::Adaa1<C>> (shapers_);

    const float amplitude = velocity_ * staticGain_;

    for (int i = 0; i < numSamples; ++i)
    {
        const float envelopeLevel = envelope_.nextValue();

        if (envelope_.isFinished())
        {
            active_ = false;
            return;
        }

        const float raw = nextSample();

        if (! active_)
            return;

        const float dry = raw * amplitude * envelopeLevel;

        float wet = dry;

        if (settings.filterBeforeDrive)
        {
            wet = filter_.process (wet);
            wet = dcBlocker_.process (shaper.process (wet * preGain) * postGain);
        }
        else
        {
            wet = dcBlocker_.process (shaper.process (wet * preGain) * postGain);
            wet = filter_.process (wet);
        }

        dryL[i] += dry * gainLeft_;
        dryR[i] += dry * gainRight_;
        wetL[i] += wet * gainLeft_;
        wetR[i] += wet * gainRight_;
    }
}

// ---------------------------------------------------------------------------
// VoicePool
// ---------------------------------------------------------------------------

void VoicePool::prepare (double sampleRate) noexcept
{
    for (auto& voice : voices_)
        voice.prepare (sampleRate);

    nextAge_ = 0;
}

void VoicePool::setPolyphony (int numVoices) noexcept
{
    const int limited = juce::jlimit (1, maxVoices, numVoices);

    // Voices outside the new limit have to stop, or a shrink would leave them
    // sounding with no way to reach them again.
    for (int i = limited; i < polyphony_ && i < maxVoices; ++i)
        voices_[static_cast<std::size_t> (i)].kill();

    polyphony_ = limited;
}

Voice* VoicePool::allocateVoice() noexcept
{
    Voice* oldest = nullptr;

    for (int i = 0; i < polyphony_; ++i)
    {
        auto& voice = voices_[static_cast<std::size_t> (i)];

        if (! voice.isActive())
            return &voice;

        if (oldest == nullptr || voice.age() < oldest->age())
            oldest = &voice;
    }

    return oldest;
}

void VoicePool::noteOn (const Sample* sample, int midiNote, float velocity) noexcept
{
    if (midiNote < 0 || midiNote > 127)
        return;

    if (auto* voice = allocateVoice())
        voice->start (sample, midiNote, velocity, nextAge_++);
}

void VoicePool::noteOff (int midiNote) noexcept
{
    for (int i = 0; i < polyphony_; ++i)
    {
        auto& voice = voices_[static_cast<std::size_t> (i)];

        if (voice.isActive() && voice.note() == midiNote && ! voice.isReleasing())
            voice.release();
    }
}

void VoicePool::stopAll() noexcept
{
    for (auto& voice : voices_)
        voice.kill();
}

int VoicePool::activeVoiceCount() const noexcept
{
    int count = 0;

    for (const auto& voice : voices_)
        count += voice.isActive() ? 1 : 0;

    return count;
}

void VoicePool::render (float* dryL, float* dryR, float* wetL, float* wetR,
                        int numSamples, const VoiceSettings& settings) noexcept
{
    for (int i = 0; i < polyphony_; ++i)
        voices_[static_cast<std::size_t> (i)].render (dryL, dryR, wetL, wetR, numSamples, settings);
}

} // namespace eon
