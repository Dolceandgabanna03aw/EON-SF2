#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "x10/sf2/Sf2Error.h"
#include "x10/sf2/Sf2Types.h"

namespace x10::sf2
{

/**
    A SoundFont decoded down to flat record tables, with nothing interpreted.

    Record tables are owned and naturally aligned; sample data stays as a view
    into the caller's buffer so that a 68 MB bank is not copied just to be read.
    That makes the input buffer's lifetime part of the contract: it must outlive
    the RawBank, or at least outlive the conversion to float that SampleStore
    performs. After that conversion the original bytes can be released.

    Terminal records (EOP / EOI / EOS) are stripped. The counts here are usable
    entries, so an empty bank is a valid RawBank with zero presets rather than an
    error — but the *tables* must have contained their terminal record, since its
    absence means the file is malformed rather than empty.
*/
struct RawBank
{
    Version     version {};
    std::string soundEngine;
    std::string name;
    std::string romName;

    std::vector<PresetHeader> presets;
    std::vector<Bag>          presetBags;
    std::vector<Modulator>    presetModulators;
    std::vector<Generator>    presetGenerators;

    std::vector<Instrument>   instruments;
    std::vector<Bag>          instrumentBags;
    std::vector<Modulator>    instrumentModulators;
    std::vector<Generator>    instrumentGenerators;

    std::vector<SampleHeader> sampleHeaders;

    /// 16-bit sample words, viewing the caller's buffer.
    std::span<const std::byte> sampleData;
    /// Optional low-order bytes that extend sampleData to 24 bits (SF2.04 sm24).
    std::span<const std::byte> sampleData24;

    [[nodiscard]] std::size_t sampleFrameCount() const noexcept { return sampleData.size() / 2; }
    [[nodiscard]] bool has24BitSamples() const noexcept { return ! sampleData24.empty(); }
};

/**
    Parses a SoundFont from memory.

    Bounds are checked on every read; no input can make this function read out of
    range, throw, or abort. On failure @p out is left empty and the reason is
    returned.
*/
[[nodiscard]] Sf2Error read (std::span<const std::byte> data, RawBank& out) noexcept;

/** Convenience wrapper that loads a file first. Returns Sf2Error::tooSmall if it cannot be read. */
[[nodiscard]] Sf2Error readFile (const std::string& path,
                                 std::vector<std::byte>& fileBytes,
                                 RawBank& out);

} // namespace x10::sf2
