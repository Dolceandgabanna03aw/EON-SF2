#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace x10::sf2
{

// ---------------------------------------------------------------------------
// On-disk record sizes, in bytes.
//
// These are the sizes the file uses, not sizeof() of the structs below. The
// structs are naturally aligned for our use and would be padded differently;
// decoding goes field by field so no packing pragma or unaligned struct cast is
// involved. That is deliberate — reinterpret_cast onto a packed layout is the
// usual way SoundFont readers acquire alignment bugs that only appear on the
// one file whose chunk happened to land on an odd offset.
// ---------------------------------------------------------------------------
inline constexpr std::size_t kPresetHeaderBytes = 38;
inline constexpr std::size_t kBagBytes          = 4;
inline constexpr std::size_t kModulatorBytes    = 10;
inline constexpr std::size_t kGeneratorBytes    = 4;
inline constexpr std::size_t kInstrumentBytes   = 22;
inline constexpr std::size_t kSampleHeaderBytes = 46;

inline constexpr std::size_t kNameBytes = 20;

/** SoundFont version from the ifil chunk. */
struct Version
{
    std::uint16_t major = 0;
    std::uint16_t minor = 0;
};

struct PresetHeader
{
    std::string   name;
    std::uint16_t preset       = 0;
    std::uint16_t bank         = 0;
    std::uint16_t bagIndex     = 0;
    std::uint32_t library      = 0;
    std::uint32_t genre        = 0;
    std::uint32_t morphology   = 0;
};

struct Instrument
{
    std::string   name;
    std::uint16_t bagIndex = 0;
};

/** A zone's first generator and modulator index. */
struct Bag
{
    std::uint16_t generatorIndex = 0;
    std::uint16_t modulatorIndex = 0;
};

/**
    One generator. The amount is a union on disk; both readings are kept because
    which one is correct depends on the operator, and that decision belongs to
    the flattener, not to the byte reader.
*/
struct Generator
{
    std::uint16_t oper       = 0;
    std::uint16_t amountU16  = 0;
    std::int16_t  amountS16  = 0;
    std::uint8_t  amountLo   = 0;
    std::uint8_t  amountHi   = 0;
};

struct Modulator
{
    std::uint16_t sourceOper      = 0;
    std::uint16_t destinationOper = 0;
    std::int16_t  amount          = 0;
    std::uint16_t amountSourceOper = 0;
    std::uint16_t transformOper    = 0;
};

/** Sample type flags from sfSampleType. */
enum SampleType : std::uint16_t
{
    sampleTypeMono      = 1,
    sampleTypeRight     = 2,
    sampleTypeLeft      = 4,
    sampleTypeLinked    = 8,
    sampleTypeRomMono   = 0x8001,
    sampleTypeRomRight  = 0x8002,
    sampleTypeRomLeft   = 0x8004,
    sampleTypeRomLinked = 0x8008,
};

struct SampleHeader
{
    std::string   name;
    std::uint32_t start           = 0;
    std::uint32_t end             = 0;
    std::uint32_t startLoop       = 0;
    std::uint32_t endLoop         = 0;
    std::uint32_t sampleRate      = 0;
    std::uint8_t  originalPitch   = 60;
    std::int8_t   pitchCorrection = 0;
    std::uint16_t sampleLink      = 0;
    std::uint16_t sampleType      = sampleTypeMono;

    [[nodiscard]] bool isRom() const noexcept { return (sampleType & 0x8000u) != 0; }
};

} // namespace x10::sf2
