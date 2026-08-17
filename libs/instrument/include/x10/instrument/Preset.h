#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "x10/instrument/Region.h"

namespace x10::instrument
{

/**
    One playable preset: a MIDI (bank, program) pair and its resolved regions.

    This is what an importer hands to RegionIndex. It carries no trace of where
    it came from — an SF2 flattener and a future SFZ importer produce the same
    shape, which is the point of keeping this format-neutral (planning
    document, issue A).
*/
struct Preset
{
    std::string   name;
    std::uint16_t bank    = 0;
    std::uint16_t program = 0;

    std::vector<Region> regions;
};

} // namespace x10::instrument
