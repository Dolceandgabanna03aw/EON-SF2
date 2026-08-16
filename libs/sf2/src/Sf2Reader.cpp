#include "x10/sf2/Sf2Reader.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>

namespace x10::sf2
{
namespace
{

// ---------------------------------------------------------------------------
// Primitive reads
//
// Byte-at-a-time and shifted rather than a cast onto a packed struct. This is
// both alignment-safe and endian-explicit: SoundFont is little-endian on disk
// regardless of the host, and chunk payloads routinely start on odd offsets
// because RIFF only pads chunks to even boundaries, not their contents.
// ---------------------------------------------------------------------------

[[nodiscard]] inline std::uint8_t u8At (const std::byte* p) noexcept
{
    return std::to_integer<std::uint8_t> (*p);
}

[[nodiscard]] inline std::uint16_t u16At (const std::byte* p) noexcept
{
    return static_cast<std::uint16_t> (static_cast<unsigned> (u8At (p))
                                       | (static_cast<unsigned> (u8At (p + 1)) << 8));
}

[[nodiscard]] inline std::uint32_t u32At (const std::byte* p) noexcept
{
    return static_cast<std::uint32_t> (u8At (p))
         | (static_cast<std::uint32_t> (u8At (p + 1)) << 8)
         | (static_cast<std::uint32_t> (u8At (p + 2)) << 16)
         | (static_cast<std::uint32_t> (u8At (p + 3)) << 24);
}

[[nodiscard]] inline std::int16_t s16At (const std::byte* p) noexcept
{
    return static_cast<std::int16_t> (u16At (p));
}

[[nodiscard]] inline std::int8_t s8At (const std::byte* p) noexcept
{
    return static_cast<std::int8_t> (u8At (p));
}

[[nodiscard]] bool idIs (std::span<const std::byte> data, std::size_t offset, const char (&id)[5]) noexcept
{
    if (offset + 4 > data.size())
        return false;

    for (std::size_t i = 0; i < 4; ++i)
        if (static_cast<char> (u8At (data.data() + offset + i)) != id[i])
            return false;

    return true;
}

/** Fixed-width SoundFont name: up to 20 bytes, NUL-terminated only if shorter. */
[[nodiscard]] std::string nameAt (const std::byte* p) noexcept
{
    std::string out;
    out.reserve (kNameBytes);

    for (std::size_t i = 0; i < kNameBytes; ++i)
    {
        const auto c = static_cast<char> (u8At (p + i));
        if (c == '\0')
            break;
        out.push_back (c);
    }

    while (! out.empty() && out.back() == ' ')
        out.pop_back();

    return out;
}

/** A NUL-terminated string chunk, bounded by the chunk itself. */
[[nodiscard]] std::string zeroTerminated (std::span<const std::byte> chunk)
{
    std::string out;
    out.reserve (chunk.size());

    for (std::byte b : chunk)
    {
        const auto c = static_cast<char> (std::to_integer<std::uint8_t> (b));
        if (c == '\0')
            break;
        out.push_back (c);
    }

    return out;
}

struct Chunk
{
    std::array<char, 4>        id {};
    std::span<const std::byte> payload {};
};

enum class WalkResult
{
    got,
    end,
    truncated
};

/**
    Advances to the next RIFF chunk.

    RIFF pads each chunk to an even length, and the pad byte is not counted in
    the chunk's own size field. A reader that forgets that drifts by one byte and
    then reads garbage for the rest of the file — which is why the odd-size case
    has its own regression test.
*/
[[nodiscard]] WalkResult nextChunk (std::span<const std::byte> region,
                                    std::size_t& offset,
                                    Chunk& out) noexcept
{
    if (offset >= region.size())
        return WalkResult::end;

    if (region.size() - offset < 8)
        return WalkResult::end; // trailing slack, not a chunk

    const std::byte* base = region.data() + offset;

    for (std::size_t i = 0; i < 4; ++i)
        out.id[i] = static_cast<char> (u8At (base + i));

    const std::uint32_t declared  = u32At (base + 4);
    const std::size_t   available = region.size() - (offset + 8);

    if (static_cast<std::size_t> (declared) > available)
        return WalkResult::truncated;

    out.payload = region.subspan (offset + 8, declared);

    const std::size_t advance = 8u + declared + (declared & 1u);
    offset = (advance > region.size() - offset) ? region.size() : offset + advance;

    return WalkResult::got;
}

[[nodiscard]] bool sameId (const Chunk& chunk, const char (&id)[5]) noexcept
{
    return chunk.id[0] == id[0] && chunk.id[1] == id[1]
        && chunk.id[2] == id[2] && chunk.id[3] == id[3];
}

/** Splits a table into fixed-size records, dropping the mandatory terminal entry. */
[[nodiscard]] Sf2Error recordCount (std::span<const std::byte> chunk,
                                    std::size_t recordBytes,
                                    std::size_t& usable) noexcept
{
    if (chunk.size() % recordBytes != 0)
        return Sf2Error::badRecordSize;

    const std::size_t total = chunk.size() / recordBytes;
    if (total == 0)
        return Sf2Error::emptyRecordTable;

    usable = total - 1; // strip EOP / EOI / EOS
    return Sf2Error::ok;
}

} // namespace

Sf2Error read (std::span<const std::byte> data, RawBank& out) noexcept
{
    out = RawBank{};

    if (data.size() < 12)
        return Sf2Error::tooSmall;

    if (! idIs (data, 0, "RIFF"))
        return Sf2Error::notRiff;

    if (! idIs (data, 8, "sfbk"))
        return Sf2Error::notSfbk;

    // The declared RIFF size is read but not trusted as a bound. Banks written
    // by older editors routinely disagree with their own file length; every read
    // below is bounds-checked against the buffer, so honouring a wrong size here
    // would only reject files that are otherwise perfectly readable.
    const std::span<const std::byte> body = data.subspan (12);

    std::span<const std::byte> infoList, sdtaList, pdtaList;

    std::size_t offset = 0;
    Chunk chunk;

    for (;;)
    {
        const auto result = nextChunk (body, offset, chunk);
        if (result == WalkResult::truncated)
            return Sf2Error::truncatedChunk;
        if (result == WalkResult::end)
            break;

        if (! sameId (chunk, "LIST") || chunk.payload.size() < 4)
            continue;

        const auto listType    = chunk.payload.subspan (0, 4);
        const auto listContent = chunk.payload.subspan (4);

        auto typeIs = [&listType] (const char (&id)[5])
        {
            for (std::size_t i = 0; i < 4; ++i)
                if (static_cast<char> (u8At (listType.data() + i)) != id[i])
                    return false;
            return true;
        };

        if (typeIs ("INFO"))      infoList = listContent;
        else if (typeIs ("sdta")) sdtaList = listContent;
        else if (typeIs ("pdta")) pdtaList = listContent;
    }

    if (infoList.empty()) return Sf2Error::missingInfoList;
    if (pdtaList.empty()) return Sf2Error::missingPdtaList;
    // sdta may legitimately be an empty list in a bank that only references ROM
    // samples, so its absence is checked separately from its emptiness.

    // ---------------------------------------------------------------- INFO
    bool haveVersion = false;

    offset = 0;
    for (;;)
    {
        const auto result = nextChunk (infoList, offset, chunk);
        if (result == WalkResult::truncated) return Sf2Error::truncatedChunk;
        if (result == WalkResult::end)       break;

        if (sameId (chunk, "ifil") && chunk.payload.size() >= 4)
        {
            out.version.major = u16At (chunk.payload.data());
            out.version.minor = u16At (chunk.payload.data() + 2);
            haveVersion = true;
        }
        else if (sameId (chunk, "isng")) out.soundEngine = zeroTerminated (chunk.payload);
        else if (sameId (chunk, "INAM")) out.name        = zeroTerminated (chunk.payload);
        else if (sameId (chunk, "irom")) out.romName     = zeroTerminated (chunk.payload);
    }

    if (! haveVersion)
        return Sf2Error::missingChunk;

    // Major version 2 is the whole of SF2. Version 1 has a different pdta layout
    // and version 3 stores Ogg-compressed samples; neither can be read by the
    // code below, and pretending otherwise would produce silent garbage.
    if (out.version.major != 2)
        return Sf2Error::badVersion;

    // ---------------------------------------------------------------- sdta
    std::span<const std::byte> smpl, sm24;

    offset = 0;
    for (;;)
    {
        const auto result = nextChunk (sdtaList, offset, chunk);
        if (result == WalkResult::truncated) return Sf2Error::truncatedChunk;
        if (result == WalkResult::end)       break;

        if (sameId (chunk, "smpl"))      smpl = chunk.payload;
        else if (sameId (chunk, "sm24")) sm24 = chunk.payload;
    }

    if (smpl.size() % 2 != 0)
        return Sf2Error::inconsistentSampleData;

    out.sampleData = smpl;

    // The spec is explicit that a mismatched sm24 must be ignored rather than
    // treated as an error: the bank is still a valid 16-bit SoundFont.
    if (! sm24.empty() && sm24.size() == smpl.size() / 2)
        out.sampleData24 = sm24;

    // ---------------------------------------------------------------- pdta
    // Presence is tracked separately from size. A zero-length table is a
    // different fault from an absent one — it means the writer emitted the chunk
    // but dropped even the terminal record — and collapsing the two would make
    // one of the diagnostics unreachable.
    struct Table
    {
        bool                       found = false;
        std::span<const std::byte> data {};
    };

    Table phdr, pbag, pmod, pgen, inst, ibag, imod, igen, shdr;

    auto capture = [] (Table& table, const Chunk& source)
    {
        table.found = true;
        table.data  = source.payload;
    };

    offset = 0;
    for (;;)
    {
        const auto result = nextChunk (pdtaList, offset, chunk);
        if (result == WalkResult::truncated) return Sf2Error::truncatedChunk;
        if (result == WalkResult::end)       break;

        if      (sameId (chunk, "phdr")) capture (phdr, chunk);
        else if (sameId (chunk, "pbag")) capture (pbag, chunk);
        else if (sameId (chunk, "pmod")) capture (pmod, chunk);
        else if (sameId (chunk, "pgen")) capture (pgen, chunk);
        else if (sameId (chunk, "inst")) capture (inst, chunk);
        else if (sameId (chunk, "ibag")) capture (ibag, chunk);
        else if (sameId (chunk, "imod")) capture (imod, chunk);
        else if (sameId (chunk, "igen")) capture (igen, chunk);
        else if (sameId (chunk, "shdr")) capture (shdr, chunk);
    }

    if (! phdr.found || ! pbag.found || ! pmod.found || ! pgen.found
        || ! inst.found || ! ibag.found || ! imod.found || ! igen.found || ! shdr.found)
        return Sf2Error::missingChunk;

    std::size_t nPresets = 0, nPresetBags = 0, nPresetMods = 0, nPresetGens = 0;
    std::size_t nInstruments = 0, nInstBags = 0, nInstMods = 0, nInstGens = 0;
    std::size_t nSamples = 0;

    struct TableSpec
    {
        std::span<const std::byte> chunk;
        std::size_t                recordBytes;
        std::size_t*               count;
    };

    const std::array<TableSpec, 9> tables { {
        { phdr.data, kPresetHeaderBytes, &nPresets },
        { pbag.data, kBagBytes,          &nPresetBags },
        { pmod.data, kModulatorBytes,    &nPresetMods },
        { pgen.data, kGeneratorBytes,    &nPresetGens },
        { inst.data, kInstrumentBytes,   &nInstruments },
        { ibag.data, kBagBytes,          &nInstBags },
        { imod.data, kModulatorBytes,    &nInstMods },
        { igen.data, kGeneratorBytes,    &nInstGens },
        { shdr.data, kSampleHeaderBytes, &nSamples },
    } };

    for (const auto& table : tables)
        if (const auto error = recordCount (table.chunk, table.recordBytes, *table.count);
            error != Sf2Error::ok)
            return error;

    // ------------------------------------------------------------- decode
    out.presets.reserve (nPresets);
    for (std::size_t i = 0; i < nPresets; ++i)
    {
        const std::byte* p = phdr.data.data() + i * kPresetHeaderBytes;
        PresetHeader header;
        header.name       = nameAt (p);
        header.preset     = u16At (p + 20);
        header.bank       = u16At (p + 22);
        header.bagIndex   = u16At (p + 24);
        header.library    = u32At (p + 26);
        header.genre      = u32At (p + 30);
        header.morphology = u32At (p + 34);
        out.presets.push_back (std::move (header));
    }

    auto decodeBags = [] (std::span<const std::byte> chunkData, std::size_t count, std::vector<Bag>& dest)
    {
        dest.reserve (count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::byte* p = chunkData.data() + i * kBagBytes;
            dest.push_back (Bag { u16At (p), u16At (p + 2) });
        }
    };

    auto decodeModulators = [] (std::span<const std::byte> chunkData, std::size_t count, std::vector<Modulator>& dest)
    {
        dest.reserve (count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::byte* p = chunkData.data() + i * kModulatorBytes;
            dest.push_back (Modulator { u16At (p), u16At (p + 2), s16At (p + 4), u16At (p + 6), u16At (p + 8) });
        }
    };

    auto decodeGenerators = [] (std::span<const std::byte> chunkData, std::size_t count, std::vector<Generator>& dest)
    {
        dest.reserve (count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const std::byte* p = chunkData.data() + i * kGeneratorBytes;
            Generator gen;
            gen.oper      = u16At (p);
            gen.amountU16 = u16At (p + 2);
            gen.amountS16 = s16At (p + 2);
            gen.amountLo  = u8At (p + 2);
            gen.amountHi  = u8At (p + 3);
            dest.push_back (gen);
        }
    };

    decodeBags (pbag.data, nPresetBags, out.presetBags);
    decodeModulators (pmod.data, nPresetMods, out.presetModulators);
    decodeGenerators (pgen.data, nPresetGens, out.presetGenerators);

    out.instruments.reserve (nInstruments);
    for (std::size_t i = 0; i < nInstruments; ++i)
    {
        const std::byte* p = inst.data.data() + i * kInstrumentBytes;
        out.instruments.push_back (Instrument { nameAt (p), u16At (p + 20) });
    }

    decodeBags (ibag.data, nInstBags, out.instrumentBags);
    decodeModulators (imod.data, nInstMods, out.instrumentModulators);
    decodeGenerators (igen.data, nInstGens, out.instrumentGenerators);

    out.sampleHeaders.reserve (nSamples);
    for (std::size_t i = 0; i < nSamples; ++i)
    {
        const std::byte* p = shdr.data.data() + i * kSampleHeaderBytes;
        SampleHeader header;
        header.name            = nameAt (p);
        header.start           = u32At (p + 20);
        header.end             = u32At (p + 24);
        header.startLoop       = u32At (p + 28);
        header.endLoop         = u32At (p + 32);
        header.sampleRate      = u32At (p + 36);
        header.originalPitch   = u8At (p + 40);
        header.pitchCorrection = s8At (p + 41);
        header.sampleLink      = u16At (p + 42);
        header.sampleType      = u16At (p + 44);
        out.sampleHeaders.push_back (std::move (header));
    }

    return Sf2Error::ok;
}

Sf2Error readFile (const std::string& path, std::vector<std::byte>& fileBytes, RawBank& out)
{
    std::ifstream stream (path, std::ios::binary | std::ios::ate);
    if (! stream)
        return Sf2Error::tooSmall;

    const auto size = stream.tellg();
    if (size <= 0)
        return Sf2Error::tooSmall;

    fileBytes.resize (static_cast<std::size_t> (size));
    stream.seekg (0);
    stream.read (reinterpret_cast<char*> (fileBytes.data()), size);

    if (! stream)
        return Sf2Error::tooSmall;

    return read (std::span<const std::byte> { fileBytes }, out);
}

} // namespace x10::sf2
