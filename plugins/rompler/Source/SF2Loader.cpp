#include "SF2Loader.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <tuple>

namespace aod
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

    // Keyed by the resolved PCM range plus its source rate. A std::map over a
    // tuple, not a hand-combined hash: a collision here would silently hand two
    // unrelated regions the same audio, which is a correctness bug wearing a
    // performance optimization's clothes. At ~1,400 entries the lookup cost is
    // noise next to what it replaces.
    std::map<std::tuple<std::uint32_t, std::uint32_t, int>, SharedPcm> pcmCache;

    for (const auto& preset : presets)
    {
        for (const auto& region : preset.regions)
        {
            const int sourceRate = static_cast<int>(region.sampleRateHz);

            const auto key = std::make_tuple(region.start, region.end, sourceRate);
            auto cached = pcmCache.find(key);
            if (cached == pcmCache.end())
                cached = pcmCache
                             .emplace(key, buildSharedPcm(rawBank.sampleData, region.start,
                                                          region.end, sourceRate))
                             .first;
            const SharedPcm& pcm = cached->second;

            Sample sample;
            sample.data = pcm.data;
            sample.sampleRate = pcm.resampled ? hostSampleRate_ : sourceRate;
            sample.loopStart = static_cast<int>(region.loopStart > region.start
                                                     ? region.loopStart - region.start : 0);
            sample.loopEnd = static_cast<int>(region.loopEnd > region.start
                                                   ? region.loopEnd - region.start : 0);
            // Loop points arrive as source-rate frames, so when the shared
            // buffer was resampled they have to move with it. Left unscaled, a
            // 22.05 kHz sample on a 48 kHz host looped over the first ~46 % of
            // the intended region. Only the loop points are per-region here;
            // the audio itself is shared.
            if (pcm.resampled)
            {
                const auto rescale = [ratio = pcm.resampleRatio] (int frame) noexcept
                {
                    return static_cast<std::int64_t>(
                        std::llround(static_cast<double>(frame) * ratio));
                };
                sample.loopEnd = static_cast<int>(rescale(sample.loopEnd));
                sample.loopStart = static_cast<int>(rescale(sample.loopStart));
            }
            // Loop points may exceed the loaded playback range in malformed
            // files (endLoop past end). Clamp them so the loop wraps inside the
            // buffer and a release tail never reads out of bounds.
            if (pcm.data != nullptr && pcm.data->size() >= 2)
            {
                const auto maxFrame = static_cast<int>(pcm.data->size() - 1);
                sample.loopEnd = std::min(sample.loopEnd, maxFrame);
                sample.loopStart = std::min(sample.loopStart, maxFrame - 1);
            }
            sample.loopEnabled = region.loopMode != x10::instrument::LoopMode::none;
            sample.filterCutoffHz = region.filterCutoffHz;
            sample.filterResonanceDb = region.filterResonanceDb;
            sample.rootKey = region.rootKey;
            sample.tuneCents = region.tuneCents;
            sample.scaleTuningCentsPerKey = region.scaleTuningCentsPerKey;

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

int SF2Loader::presetCount() const noexcept
{
    return regionIndex_ ? static_cast<int>(regionIndex_->presetCount()) : 0;
}

juce::String SF2Loader::presetName(int presetIndex) const noexcept
{
    if (!regionIndex_ || presetIndex < 0 || presetIndex >= presetCount())
        return {};

    return regionIndex_->presetAt(static_cast<std::size_t>(presetIndex)).name;
}

std::pair<int, int> SF2Loader::presetBankProgram(int presetIndex) const noexcept
{
    if (!regionIndex_ || presetIndex < 0 || presetIndex >= presetCount())
        return { 0, 0 };

    const auto& preset = regionIndex_->presetAt(static_cast<std::size_t>(presetIndex));
    return { preset.bank, preset.program };
}

SF2Loader::SharedPcm SF2Loader::buildSharedPcm(std::span<const std::byte> sampleData,
                                               std::uint32_t start, std::uint32_t end,
                                               int sourceSampleRate) const
{
    SharedPcm pcm;

    std::vector<float> converted = convertPcmRange(sampleData, start, end);

    if (converted.empty() || sourceSampleRate <= 0 || sourceSampleRate == hostSampleRate_)
    {
        pcm.data = std::make_shared<const std::vector<float>>(std::move(converted));
        return pcm;
    }

    // double, not float: the same ratio also rescales each region's loop
    // points, and float phase accumulates audible error over long samples.
    const double ratio = static_cast<double>(hostSampleRate_) / static_cast<double>(sourceSampleRate);
    const auto newSize = static_cast<std::size_t>(static_cast<double>(converted.size()) * ratio);
    if (newSize == 0)
    {
        pcm.data = std::make_shared<const std::vector<float>>(std::move(converted));
        return pcm;
    }

    std::vector<float> resampled(newSize);

    for (std::size_t i = 0; i < newSize; ++i)
    {
        const double phase = static_cast<double>(i) / ratio;
        const auto index = static_cast<std::size_t>(phase);

        if (index >= converted.size() - 1)
        {
            resampled[i] = converted.back();
        }
        else
        {
            const auto frac = static_cast<float>(phase - static_cast<double>(index));
            const float s0 = converted[index];
            const float s1 = converted[index + 1];
            resampled[i] = s0 + frac * (s1 - s0);
        }
    }

    pcm.data = std::make_shared<const std::vector<float>>(std::move(resampled));
    pcm.resampleRatio = ratio;
    pcm.resampled = true;
    return pcm;
}

} // namespace aod
