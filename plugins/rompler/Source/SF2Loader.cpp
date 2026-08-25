#include "SF2Loader.h"
#include <cstring>
#include <map>

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
    // difference between converting the audio once per unique [start, end) and
    // once per region — roughly 13x less work and memory on FluidR3_GM. Scoped
    // to this load: it is discarded once every region has taken a shared_ptr
    // into it. A std::map keyed on the actual pair, rather than a hand-combined
    // hash, is deliberate — a colliding hash here would silently hand two
    // unrelated regions the same audio.
    //
    // The source rate is not part of the key: the buffer is stored unresampled,
    // so a range's contents depend only on the range. Two regions naming the
    // same bytes with different sampleRateHz would be a malformed bank, and
    // they would still get byte-identical audio here — only their playback
    // increments would differ, which is exactly right.
    std::map<std::pair<std::uint32_t, std::uint32_t>,
             std::shared_ptr<const std::vector<float>>> pcmCache;

    for (const auto& preset : presets)
    {
        for (const auto& region : preset.regions)
        {
            Sample sample;

            auto [entry, inserted] = pcmCache.try_emplace({ region.start, region.end });
            if (inserted)
                entry->second = buildSharedPcm(rawBank.sampleData, region.start, region.end);

            sample.data = entry->second;
            sample.sampleRate = static_cast<int>(region.sampleRateHz);

            // Loop points index `data` directly, and `data` is in source frames,
            // so these need no rescaling. They stay per-region rather than being
            // shared with the audio: a zone can override its loop points
            // independently of the sample it references.
            sample.loopStart = static_cast<int>(region.loopStart > region.start
                                                ? region.loopStart - region.start : 0);
            sample.loopEnd = static_cast<int>(region.loopEnd > region.start
                                              ? region.loopEnd - region.start : 0);
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

std::shared_ptr<const std::vector<float>>
    SF2Loader::buildSharedPcm(std::span<const std::byte> sampleData,
                              std::uint32_t start, std::uint32_t end)
{
    return std::make_shared<const std::vector<float>>(convertPcmRange(sampleData, start, end));
}

} // namespace eon
