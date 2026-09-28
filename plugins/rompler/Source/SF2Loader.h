#pragma once

#include "Sampler.h"
#include <x10/sf2/Sf2Reader.h>
#include <x10/sf2/Sf2Flattener.h>
#include <x10/instrument/RegionIndex.h>
#include <juce_core/juce_core.h>
#include <cstddef>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace aod
{

class SF2Loader
{
public:
    explicit SF2Loader(int hostSampleRate) : hostSampleRate_(hostSampleRate) {}

    bool loadFile(const juce::File& file);

    /**
        Return an already decoded bank when the file identity and host rate
        match. Loading and cache eviction happen off the audio thread only.
        Callers retain shared ownership while voices may reference its samples.
    */
    [[nodiscard]] static std::shared_ptr<const SF2Loader> loadCached (const juce::File& file,
                                                                       int hostSampleRate);

    [[nodiscard]] int hostSampleRate() const noexcept { return hostSampleRate_; }

    /** Approximate bytes held by decoded sample buffers. */
    [[nodiscard]] std::size_t sampleStorageBytes() const noexcept;

    /** Returns nullptr if no matching region/sample was found. */
    [[nodiscard]] const Sample* getSample(int bank, int program, int key, int velocity) const noexcept;

    /** (bank, program) of preset 0 in load order, or {0, 0} if nothing loaded. */
    [[nodiscard]] std::pair<int, int> firstPresetProgram() const noexcept;

    [[nodiscard]] int presetCount() const noexcept;
    [[nodiscard]] juce::String presetName (int presetIndex) const noexcept;
    [[nodiscard]] std::pair<int, int> presetBankProgram (int presetIndex) const noexcept;

private:
    int hostSampleRate_;
    std::unique_ptr<x10::instrument::RegionIndex> regionIndex_;
    std::unordered_map<const x10::instrument::Region*, Sample> samples_;

    void resampleToHostRate(Sample& sample);
};

} // namespace aod
