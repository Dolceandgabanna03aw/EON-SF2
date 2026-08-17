#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <span>
#include <vector>

#include "support/Sf2Builder.h"
#include "x10/sf2/Sf2Reader.h"

using namespace x10::sf2;
using namespace x10::sf2::test;

namespace
{

[[nodiscard]] Sf2Error parse (const std::vector<std::byte>& bytes, RawBank& bank)
{
    return read (std::span<const std::byte> { bytes }, bank);
}

/** Deterministic PRNG so a fuzz failure can be replayed exactly. */
class Xorshift32
{
public:
    explicit Xorshift32 (std::uint32_t seed) noexcept : state_ (seed) {}

    [[nodiscard]] std::uint32_t next() noexcept
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }

private:
    std::uint32_t state_;
};

} // namespace

TEST_CASE ("a minimal bank round-trips", "[sf2][reader]")
{
    const Sf2Builder builder;
    RawBank bank;

    REQUIRE (parse (builder.build(), bank) == Sf2Error::ok);

    REQUIRE (bank.version.major == 2);
    REQUIRE (bank.version.minor == 4);
    REQUIRE (bank.name == "Test Bank");
    REQUIRE (bank.soundEngine == "EMU8000");

    // Terminal records are stripped, so these are usable counts.
    REQUIRE (bank.presets.size() == 1);
    REQUIRE (bank.presetBags.size() == 1);
    REQUIRE (bank.presetModulators.empty());
    REQUIRE (bank.presetGenerators.size() == 1);
    REQUIRE (bank.instruments.size() == 1);
    REQUIRE (bank.instrumentBags.size() == 1);
    REQUIRE (bank.instrumentModulators.empty());
    REQUIRE (bank.instrumentGenerators.size() == 1);
    REQUIRE (bank.sampleHeaders.size() == 1);

    REQUIRE (bank.presets[0].name == "Test Preset");
    REQUIRE (bank.instruments[0].name == "Test Instrument");
    REQUIRE (bank.sampleHeaders[0].name == "Test Sample");

    // Generator 41 is 'instrument', 53 is 'sampleID'. Getting these wrong is how
    // a reader silently produces a bank that plays nothing.
    REQUIRE (bank.presetGenerators[0].oper == 41);
    REQUIRE (bank.instrumentGenerators[0].oper == 53);

    REQUIRE (bank.sampleHeaders[0].sampleRate == 44100);
    REQUIRE (bank.sampleHeaders[0].originalPitch == 60);
    REQUIRE (bank.sampleHeaders[0].end == 64);
    REQUIRE_FALSE (bank.sampleHeaders[0].isRom());

    // 64 sample frames plus the 46 guard frames the spec asks for.
    REQUIRE (bank.sampleFrameCount() == 110);
    REQUIRE_FALSE (bank.has24BitSamples());
}

TEST_CASE ("24-bit sample extension is picked up when its size agrees", "[sf2][reader]")
{
    Sf2Builder builder;
    builder.include24Bit = true;

    RawBank bank;
    REQUIRE (parse (builder.build(), bank) == Sf2Error::ok);

    REQUIRE (bank.has24BitSamples());
    REQUIRE (bank.sampleData24.size() == bank.sampleData.size() / 2);
}

TEST_CASE ("an odd-length chunk does not desynchronise the walk", "[sf2][reader]")
{
    // RIFF pads odd chunks to an even boundary and does not count the pad byte
    // in the size field. A reader that misses this reads the rest of the file
    // one byte out of phase, which corrupts everything after it rather than
    // failing outright.
    Sf2Builder builder;
    builder.oddLengthInfoChunk = true;

    RawBank bank;
    REQUIRE (parse (builder.build(), bank) == Sf2Error::ok);

    // INAM follows the odd chunk, so reading it correctly proves resynchronisation.
    REQUIRE (bank.name == "Test Bank");
    REQUIRE (bank.presets.size() == 1);
}

TEST_CASE ("malformed containers are rejected by value, never by crashing", "[sf2][reader]")
{
    RawBank bank;

    SECTION ("empty input")
    {
        REQUIRE (parse ({}, bank) == Sf2Error::tooSmall);
    }

    SECTION ("not a RIFF file")
    {
        std::vector<std::byte> junk (64, std::byte { 0x5a });
        REQUIRE (parse (junk, bank) == Sf2Error::notRiff);
    }

    SECTION ("RIFF but not a SoundFont")
    {
        auto bytes = Sf2Builder{}.build();
        bytes[8] = std::byte { 'W' }; // sfbk -> Wfbk
        REQUIRE (parse (bytes, bank) == Sf2Error::notSfbk);
    }

    SECTION ("chunk claims more bytes than the file holds")
    {
        auto bytes = Sf2Builder{}.build();
        // Inflate the first top-level LIST size field.
        bytes[16] = std::byte { 0xff };
        bytes[17] = std::byte { 0xff };
        REQUIRE (parse (bytes, bank) == Sf2Error::truncatedChunk);
    }
}

TEST_CASE ("missing structural lists are reported specifically", "[sf2][reader]")
{
    RawBank bank;

    SECTION ("no INFO")
    {
        Sf2Builder builder;
        builder.includeInfoList = false;
        REQUIRE (parse (builder.build(), bank) == Sf2Error::missingInfoList);
    }

    SECTION ("no pdta")
    {
        Sf2Builder builder;
        builder.includePdtaList = false;
        REQUIRE (parse (builder.build(), bank) == Sf2Error::missingPdtaList);
    }
}

TEST_CASE ("record tables are validated individually", "[sf2][reader]")
{
    RawBank bank;

    SECTION ("an absent table is missingChunk")
    {
        Sf2Builder builder;
        builder.omitPdtaChunk = "shdr";
        REQUIRE (parse (builder.build(), bank) == Sf2Error::missingChunk);
    }

    SECTION ("a present but empty table is emptyRecordTable")
    {
        // Distinct from the case above: the writer emitted the chunk and then
        // dropped even the mandatory terminal record.
        Sf2Builder builder;
        builder.emptyPdtaChunk = "phdr";
        REQUIRE (parse (builder.build(), bank) == Sf2Error::emptyRecordTable);
    }

    SECTION ("a table whose size is not a whole number of records")
    {
        Sf2Builder builder;
        builder.misalignPdtaChunk = "inst";
        REQUIRE (parse (builder.build(), bank) == Sf2Error::badRecordSize);
    }
}

TEST_CASE ("only SoundFont major version 2 is accepted", "[sf2][reader]")
{
    RawBank bank;

    SECTION ("version 1 has a different pdta layout")
    {
        Sf2Builder builder;
        builder.versionMajor = 1;
        REQUIRE (parse (builder.build(), bank) == Sf2Error::badVersion);
    }

    SECTION ("version 3 stores compressed samples")
    {
        Sf2Builder builder;
        builder.versionMajor = 3;
        REQUIRE (parse (builder.build(), bank) == Sf2Error::badVersion);
    }

    SECTION ("every 2.x minor is accepted")
    {
        for (std::uint16_t minor : { std::uint16_t { 0 }, std::uint16_t { 1 }, std::uint16_t { 4 } })
        {
            Sf2Builder builder;
            builder.versionMinor = minor;
            CAPTURE (minor);
            REQUIRE (parse (builder.build(), bank) == Sf2Error::ok);
        }
    }
}

TEST_CASE ("truncation at any offset is survivable", "[sf2][reader][fuzz]")
{
    // Half-downloaded files and interrupted copies are ordinary in a sample
    // library. Every prefix of a valid bank must produce an error or a coherent
    // parse, and never a read past the end.
    const auto complete = Sf2Builder{}.build();

    for (std::size_t length = 0; length < complete.size(); ++length)
    {
        const std::span<const std::byte> prefix { complete.data(), length };

        RawBank bank;
        const auto error = read (prefix, bank);

        // No assertion on which error: the point is that it returns one.
        CAPTURE (length, static_cast<int> (error));
        REQUIRE (toString (error) != nullptr);
    }
}

TEST_CASE ("single-byte corruption is survivable", "[sf2][reader][fuzz]")
{
    // Run this under the ASan/UBSan preset for it to mean anything: the failure
    // being hunted is an out-of-bounds read, which is invisible in a normal
    // build until it happens to land on an unmapped page.
    const auto complete = Sf2Builder{}.build();
    Xorshift32 rng { 0x5f2u };

    for (int iteration = 0; iteration < 4000; ++iteration)
    {
        auto corrupted = complete;
        const std::size_t index = rng.next() % corrupted.size();
        corrupted[index] = std::byte { static_cast<std::uint8_t> (rng.next() & 0xffu) };

        RawBank bank;
        const auto error = read (std::span<const std::byte> { corrupted }, bank);

        CAPTURE (iteration, index, static_cast<int> (error));
        REQUIRE (toString (error) != nullptr);
    }
}

TEST_CASE ("a failed parse leaves the output empty rather than half-filled", "[sf2][reader]")
{
    // A caller that ignores the error code must not find a plausible-looking
    // bank sitting in the out parameter.
    RawBank bank;

    Sf2Builder good;
    REQUIRE (parse (good.build(), bank) == Sf2Error::ok);
    REQUIRE (bank.presets.size() == 1);

    Sf2Builder bad;
    bad.versionMajor = 3;
    REQUIRE (parse (bad.build(), bank) == Sf2Error::badVersion);

    REQUIRE (bank.presets.empty());
    REQUIRE (bank.instruments.empty());
    REQUIRE (bank.sampleHeaders.empty());
    REQUIRE (bank.sampleData.empty());
}
