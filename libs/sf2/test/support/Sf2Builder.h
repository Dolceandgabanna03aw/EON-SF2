#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "x10/sf2/Sf2Types.h"

namespace x10::sf2::test
{

/**
    Assembles SoundFont byte streams, valid or deliberately broken.

    Real banks are megabytes and cannot go in the repository; worse, a real bank
    exercises whichever features its author happened to use, so a parser bug
    shows up as "some file fails" rather than as a named defect. These fixtures
    are a few hundred bytes each and isolate one structural property apiece.
*/
class ByteWriter
{
public:
    void u8 (std::uint8_t v) { bytes_.push_back (std::byte { v }); }

    void u16 (std::uint16_t v)
    {
        u8 (static_cast<std::uint8_t> (v & 0xffu));
        u8 (static_cast<std::uint8_t> ((v >> 8) & 0xffu));
    }

    void u32 (std::uint32_t v)
    {
        u16 (static_cast<std::uint16_t> (v & 0xffffu));
        u16 (static_cast<std::uint16_t> ((v >> 16) & 0xffffu));
    }

    void s16 (std::int16_t v) { u16 (static_cast<std::uint16_t> (v)); }

    void id (std::string_view fourCC)
    {
        for (std::size_t i = 0; i < 4; ++i)
            u8 (i < fourCC.size() ? static_cast<std::uint8_t> (fourCC[i]) : 0u);
    }

    /** Fixed 20-byte SoundFont name field, zero padded. */
    void name20 (std::string_view text)
    {
        for (std::size_t i = 0; i < kNameBytes; ++i)
            u8 (i < text.size() ? static_cast<std::uint8_t> (text[i]) : 0u);
    }

    void zstr (std::string_view text)
    {
        for (char c : text)
            u8 (static_cast<std::uint8_t> (c));
        u8 (0);
    }

    void raw (std::span<const std::byte> data)
    {
        bytes_.insert (bytes_.end(), data.begin(), data.end());
    }

    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::vector<std::byte>& bytes() noexcept { return bytes_; }

private:
    std::vector<std::byte> bytes_;
};

/** Wraps a payload as a RIFF chunk, adding the pad byte an odd length requires. */
[[nodiscard]] inline std::vector<std::byte> makeChunk (std::string_view fourCC,
                                                       const std::vector<std::byte>& payload)
{
    ByteWriter w;
    w.id (fourCC);
    w.u32 (static_cast<std::uint32_t> (payload.size()));
    w.raw (payload);

    if ((payload.size() & 1u) != 0u)
        w.u8 (0); // RIFF pad, not counted in the size field

    return w.bytes();
}

[[nodiscard]] inline std::vector<std::byte> makeList (std::string_view listType,
                                                      const std::vector<std::vector<std::byte>>& chunks)
{
    ByteWriter inner;
    inner.id (listType);
    for (const auto& c : chunks)
        inner.raw (c);

    return makeChunk ("LIST", inner.bytes());
}

/**
    A minimal but genuinely valid bank: one preset -> one instrument -> one
    sample, with every mandatory terminal record present.

    Each knob breaks exactly one property so a failing test names the defect.
*/
struct Sf2Builder
{
    std::uint16_t versionMajor = 2;
    std::uint16_t versionMinor = 4;
    std::string   bankName     = "Test Bank";
    std::string   soundEngine  = "EMU8000";

    std::size_t sampleFrames = 64;
    bool        include24Bit = false;

    bool includeInfoList = true;
    bool includeSdtaList = true;
    bool includePdtaList = true;

    /// pdta chunk id to leave out entirely, e.g. "shdr".
    std::string omitPdtaChunk;
    /// pdta chunk id to emit with zero length.
    std::string emptyPdtaChunk;
    /// pdta chunk id to emit with one trailing byte, breaking record alignment.
    std::string misalignPdtaChunk;

    /// Emits an odd-length ICMT before INAM, so the reader must honour the pad byte.
    bool oddLengthInfoChunk = false;

    [[nodiscard]] std::vector<std::byte> build() const
    {
        std::vector<std::vector<std::byte>> topLevel;

        if (includeInfoList)
            topLevel.push_back (buildInfo());
        if (includeSdtaList)
            topLevel.push_back (buildSdta());
        if (includePdtaList)
            topLevel.push_back (buildPdta());

        ByteWriter body;
        body.id ("sfbk");
        for (const auto& c : topLevel)
            body.raw (c);

        ByteWriter file;
        file.id ("RIFF");
        file.u32 (static_cast<std::uint32_t> (body.bytes().size()));
        file.raw (body.bytes());
        return file.bytes();
    }

private:
    [[nodiscard]] std::vector<std::byte> buildInfo() const
    {
        std::vector<std::vector<std::byte>> chunks;

        {
            ByteWriter w;
            w.u16 (versionMajor);
            w.u16 (versionMinor);
            chunks.push_back (makeChunk ("ifil", w.bytes()));
        }
        {
            ByteWriter w;
            w.zstr (soundEngine);
            chunks.push_back (makeChunk ("isng", w.bytes()));
        }

        if (oddLengthInfoChunk)
        {
            ByteWriter w;
            w.zstr ("odd"); // 4 bytes with the NUL... make it genuinely odd
            w.u8 (static_cast<std::uint8_t> ('x'));
            chunks.push_back (makeChunk ("ICMT", w.bytes()));
        }

        {
            ByteWriter w;
            w.zstr (bankName);
            chunks.push_back (makeChunk ("INAM", w.bytes()));
        }

        return makeList ("INFO", chunks);
    }

    [[nodiscard]] std::vector<std::byte> buildSdta() const
    {
        std::vector<std::vector<std::byte>> chunks;

        ByteWriter smpl;
        for (std::size_t i = 0; i < sampleFrames; ++i)
            smpl.s16 (static_cast<std::int16_t> (i * 100));

        // The spec asks for 46 zero frames after the last sample so that
        // interpolators can read past the end without special-casing.
        for (std::size_t i = 0; i < 46; ++i)
            smpl.s16 (0);

        chunks.push_back (makeChunk ("smpl", smpl.bytes()));

        if (include24Bit)
        {
            ByteWriter sm24;
            for (std::size_t i = 0; i < sampleFrames + 46; ++i)
                sm24.u8 (static_cast<std::uint8_t> (i & 0xffu));

            if ((sm24.bytes().size() & 1u) != 0u)
                sm24.u8 (0);

            chunks.push_back (makeChunk ("sm24", sm24.bytes()));
        }

        return makeList ("sdta", chunks);
    }

    [[nodiscard]] std::vector<std::byte> appendTable (std::string_view fourCC,
                                                      const std::vector<std::byte>& payload) const
    {
        if (! emptyPdtaChunk.empty() && emptyPdtaChunk == fourCC)
            return makeChunk (fourCC, {});

        if (! misalignPdtaChunk.empty() && misalignPdtaChunk == fourCC)
        {
            auto broken = payload;
            broken.push_back (std::byte { 0 });
            return makeChunk (fourCC, broken);
        }

        return makeChunk (fourCC, payload);
    }

    [[nodiscard]] std::vector<std::byte> buildPdta() const
    {
        std::vector<std::vector<std::byte>> chunks;

        auto emit = [&] (std::string_view fourCC, const std::vector<std::byte>& payload)
        {
            if (! omitPdtaChunk.empty() && omitPdtaChunk == fourCC)
                return;
            chunks.push_back (appendTable (fourCC, payload));
        };

        {   // phdr: one preset plus the terminal EOP
            ByteWriter w;
            w.name20 ("Test Preset"); w.u16 (0); w.u16 (0); w.u16 (0); w.u32 (0); w.u32 (0); w.u32 (0);
            w.name20 ("EOP");         w.u16 (0); w.u16 (0); w.u16 (1); w.u32 (0); w.u32 (0); w.u32 (0);
            emit ("phdr", w.bytes());
        }
        {   // pbag: one zone plus terminal
            ByteWriter w;
            w.u16 (0); w.u16 (0);
            w.u16 (1); w.u16 (0);
            emit ("pbag", w.bytes());
        }
        {   // pmod: terminal only
            ByteWriter w;
            w.u16 (0); w.u16 (0); w.s16 (0); w.u16 (0); w.u16 (0);
            emit ("pmod", w.bytes());
        }
        {   // pgen: instrument generator (41) plus terminal
            ByteWriter w;
            w.u16 (41); w.u16 (0);
            w.u16 (0);  w.u16 (0);
            emit ("pgen", w.bytes());
        }
        {   // inst: one instrument plus terminal EOI
            ByteWriter w;
            w.name20 ("Test Instrument"); w.u16 (0);
            w.name20 ("EOI");             w.u16 (1);
            emit ("inst", w.bytes());
        }
        {   // ibag
            ByteWriter w;
            w.u16 (0); w.u16 (0);
            w.u16 (1); w.u16 (0);
            emit ("ibag", w.bytes());
        }
        {   // imod: terminal only
            ByteWriter w;
            w.u16 (0); w.u16 (0); w.s16 (0); w.u16 (0); w.u16 (0);
            emit ("imod", w.bytes());
        }
        {   // igen: sampleID generator (53) plus terminal
            ByteWriter w;
            w.u16 (53); w.u16 (0);
            w.u16 (0);  w.u16 (0);
            emit ("igen", w.bytes());
        }
        {   // shdr: one sample plus terminal EOS
            ByteWriter w;
            const auto end = static_cast<std::uint32_t> (sampleFrames);
            w.name20 ("Test Sample");
            w.u32 (0); w.u32 (end); w.u32 (8); w.u32 (end > 8 ? end - 8 : end);
            w.u32 (44100);
            w.u8 (60); w.u8 (0);
            w.u16 (0); w.u16 (sampleTypeMono);

            w.name20 ("EOS");
            w.u32 (0); w.u32 (0); w.u32 (0); w.u32 (0);
            w.u32 (0);
            w.u8 (0); w.u8 (0);
            w.u16 (0); w.u16 (0);

            emit ("shdr", w.bytes());
        }

        return makeList ("pdta", chunks);
    }
};

} // namespace x10::sf2::test
