#pragma once

#include "Sampler.h"
#include <x10/sf2/Sf2Reader.h>
#include <x10/sf2/Sf2Flattener.h>
#include <x10/instrument/RegionIndex.h>
#include <juce_core/juce_core.h>
#include <memory>
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

    void resampleToHostRate(Sample& sample);
};

} // namespace eon
