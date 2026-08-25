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
    // Takes no sample rate: samples are kept at the rate they were recorded at
    // and the voice folds the conversion into its playback increment, so the
    // loader has no use for the host's rate at all.
    SF2Loader() = default;

    bool loadFile(const juce::File& file);

    /** Returns nullptr if no matching region/sample was found. */
    [[nodiscard]] Sample* getSample(int bank, int program, int key, int velocity) noexcept;

    /** (bank, program) of preset 0 in load order, or {0, 0} if nothing loaded. */
    [[nodiscard]] std::pair<int, int> firstPresetProgram() const noexcept;

private:
    std::unique_ptr<x10::instrument::RegionIndex> regionIndex_;
    std::unordered_map<const x10::instrument::Region*, Sample> samples_;

    /** Converts one raw PCM range to float, at its own recorded rate — the
        voice handles rate conversion as part of its playback increment. Pure
        function of its arguments: two regions naming the same range always
        produce the same buffer, which is what makes the cache in loadFile()
        correct rather than merely convenient. */
    [[nodiscard]] static std::shared_ptr<const std::vector<float>>
        buildSharedPcm(std::span<const std::byte> sampleData,
                       std::uint32_t start, std::uint32_t end);
};

} // namespace eon
