#pragma once

#include "Sampler.h"
#include <x10/sf2/Sf2Reader.h>
#include <x10/sf2/Sf2Flattener.h>
#include <x10/instrument/RegionIndex.h>
#include <juce_core/juce_core.h>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eon
{

class SF2Loader
{
public:
    explicit SF2Loader(int hostSampleRate) : hostSampleRate_(hostSampleRate) {}

    bool loadFile(const juce::File& file);

    /** Returns nullptr if no matching region/sample was found. */
    [[nodiscard]] Sample* getSample(int bank, int program, int key, int velocity) noexcept;

    /** (bank, program) of preset 0 in load order, or {0, 0} if nothing loaded. */
    [[nodiscard]] std::pair<int, int> firstPresetProgram() const noexcept;

private:
    int hostSampleRate_;
    std::unique_ptr<x10::instrument::RegionIndex> regionIndex_;
    std::unordered_map<const x10::instrument::Region*, Sample> samples_;

    /** One converted-and-resampled buffer, shared by every region whose
        [start, end) names the same raw PCM range. Regions split a recording by
        key or velocity far more often than they need a private copy of it. */
    struct SharedPcm
    {
        std::shared_ptr<const std::vector<float>> data;
        // hostSampleRate_ / region.sampleRateHz for this buffer, needed to
        // rescale each region's own loop points into resampled-frame indices.
        // Meaningless when `resampled` is false — kept at 1.0f then, but not
        // compared against, since -Wfloat-equal (rightly) forbids that.
        float resampleRatio = 1.0f;
        bool resampled = false;
    };

    /** Converts one raw PCM range to float at the host sample rate. Pure
        function of its arguments: two regions with the same start/end and
        source rate always produce the same buffer, which is what makes the
        cache in loadFile() correct rather than merely convenient. */
    [[nodiscard]] SharedPcm buildSharedPcm(std::span<const std::byte> sampleData,
                                           std::uint32_t start, std::uint32_t end,
                                           int sourceSampleRate) const;
};

} // namespace eon
