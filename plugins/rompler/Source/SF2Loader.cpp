#include "SF2Loader.h"
#include <cstring>
#include <map>
#include <tuple>

namespace eon
{

namespace
{

/** Converts a little-endian int16 PCM range [start, end) frames into float [-1, 1]. */
std::vector<float> convertPcmRange(std::span<const std::byte> sampleData,
                                    std::uint32_t start, std::uint32_t end)
{
    const std::size_t frameCount = sampleData.size() / 2;
    if (start >= frameCount || end > frameCount || start >= end)
        return {};

    std::vector<float> out(end - start);
    const auto* bytes = sampleData.data();

    for (std::uint32_t i = start; i < end; ++i)
    {
        std::int16_t sample16;
        std::memcpy(&sample16, bytes + (static_cast<std::size_t>(i) * 2), sizeof(sample16));
        out[i - start] = static_cast<float>(sample16) / 32768.0f;
    }

    return out;
}

} // namespace

bool SF2Loader::loadFile(const juce::File& file)
{
    std::vector<std::byte> fileBytes;
    x10::sf2::RawBank rawBank;

    const auto error = x10::sf2::readFile(file.getFullPathName().toStdString(), fileBytes, rawBank);
    if (error != x10::sf2::Sf2Error::ok)
        return false;

    std::vector<x10::instrument::Preset> presets = x10::sf2::flatten(rawBank);

    samples_.clear();

    // Regions split one recording by key or velocity far more often than they
    // need a private copy of it: on a General MIDI bank this cache is the
    // difference between converting the audio once per unique (start, end,
    // source rate) and once per region — roughly 13x less work and memory on
    // FluidR3_GM. Scoped to this load: it is discarded once every region has
    // taken a shared_ptr into it. A std::map keyed on the actual tuple, rather
    // than a hand-combined hash, is deliberate — a colliding hash here would
    // silently hand two unrelated regions the same audio.
    std::map<std::tuple<std::uint32_t, std::uint32_t, int>, SharedPcm> pcmCache;

    for (const auto& preset : presets)
    {
        for (const auto& region : preset.regions)
        {
            Sample sample;

            const auto sourceRate = static_cast<int>(region.sampleRateHz);
            const auto cacheKey = std::make_tuple(region.start, region.end, sourceRate);

            auto [entry, inserted] = pcmCache.try_emplace(cacheKey);
            if (inserted)
                entry->second = buildSharedPcm(rawBank.sampleData, region.start, region.end, sourceRate);

            sample.data = entry->second.data;
            sample.sampleRate = hostSampleRate_;

            // Loop points are frame indices into the *source* audio. The shared
            // buffer may already be resampled, so they still have to be rescaled
            // per region — a zone can override its loop points independently of
            // the sample it references, so this part cannot be shared even when
            // the audio is.
            const std::uint32_t rawLoopStart = region.loopStart > region.start
                                              ? region.loopStart - region.start : 0;
            const std::uint32_t rawLoopEnd = region.loopEnd > region.start
                                            ? region.loopEnd - region.start : 0;

            if (entry->second.resampled)
            {
                const auto ratio = entry->second.resampleRatio;
                sample.loopStart = static_cast<int>(static_cast<float>(rawLoopStart) * ratio);
                sample.loopEnd = static_cast<int>(static_cast<float>(rawLoopEnd) * ratio);
            }
            else
            {
                sample.loopStart = static_cast<int>(rawLoopStart);
                sample.loopEnd = static_cast<int>(rawLoopEnd);
            }
            sample.loopMode = region.loopMode;

            // Copied rather than referenced: the voice reads these on the audio
            // thread and the RegionIndex owning this Region can be swapped out
            // by a bank load while a note is still sounding.
            sample.rootKey = region.rootKey;
            sample.tuneCents = region.tuneCents;
            sample.scaleTuningCentsPerKey = region.scaleTuningCentsPerKey;
            sample.attenuationDb = region.attenuationDb;
            sample.pan = region.pan;
            sample.filterCutoffHz = region.filterCutoffHz;
            sample.filterResonanceDb = region.filterResonanceDb;
            sample.volumeEnvelope = region.volumeEnvelope;

            samples_.emplace(&region, std::move(sample));
        }
    }

    // Pointers into presets[].regions stay valid: RegionIndex moves the
    // vector<Preset> buffer, it does not relocate individual elements.
    regionIndex_ = std::make_unique<x10::instrument::RegionIndex>(std::move(presets));

    return true;
}

Sample* SF2Loader::getSample(int bank, int program, int key, int velocity) noexcept
{
    if (!regionIndex_)
        return nullptr;

    std::array<const x10::instrument::Region*, 1> matches {};
    const std::size_t matchCount = regionIndex_->match(
        static_cast<std::uint16_t>(bank), static_cast<std::uint16_t>(program),
        key, velocity, matches);

    if (matchCount == 0)
        return nullptr;

    auto it = samples_.find(matches[0]);
    return it != samples_.end() ? &it->second : nullptr;
}

std::pair<int, int> SF2Loader::firstPresetProgram() const noexcept
{
    if (!regionIndex_ || regionIndex_->presetCount() == 0)
        return { 0, 0 };

    const auto& preset = regionIndex_->presetAt(0);
    return { preset.bank, preset.program };
}

SF2Loader::SharedPcm SF2Loader::buildSharedPcm(std::span<const std::byte> sampleData,
                                               std::uint32_t start, std::uint32_t end,
                                               int sourceSampleRate) const
{
    auto raw = convertPcmRange(sampleData, start, end);

    if (sourceSampleRate == hostSampleRate_ || raw.empty())
        return { std::make_shared<const std::vector<float>>(std::move(raw)), 1.0f, false };

    const float ratio = static_cast<float>(hostSampleRate_) / static_cast<float>(sourceSampleRate);
    const auto newSize = static_cast<std::size_t>(static_cast<float>(raw.size()) * ratio);
    if (newSize == 0)
        return { std::make_shared<const std::vector<float>>(std::move(raw)), 1.0f, false };

    std::vector<float> resampled(newSize);

    for (std::size_t i = 0; i < newSize; ++i)
    {
        const float phase = static_cast<float>(i) / ratio;
        const auto index = static_cast<std::size_t>(phase);

        if (index >= raw.size() - 1)
        {
            resampled[i] = raw.back();
        }
        else
        {
            const float frac = phase - static_cast<float>(index);
            const float s0 = raw[index];
            const float s1 = raw[index + 1];
            resampled[i] = s0 + frac * (s1 - s0);
        }
    }

    return { std::make_shared<const std::vector<float>>(std::move(resampled)), ratio, true };
}

} // namespace eon
