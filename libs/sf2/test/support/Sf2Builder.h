#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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

/** A generator as it appears on disk: operator plus a raw 16-bit amount. */
using GeneratorPair = std::pair<std::uint16_t, std::uint16_t>;

/** Packs a key or velocity range into the byte-pair amount the format uses. */
[[nodiscard]] inline std::uint16_t rangeAmount (std::uint8_t low, std::uint8_t high) noexcept
{
    return static_cast<std::uint16_t> (static_cast<unsigned> (low)
                                       | (static_cast<unsigned> (high) << 8));
}

/** Reinterprets a signed amount the way the format stores it. */
[[nodiscard]] inline std::uint16_t signedAmount (std::int16_t value) noexcept
{
    return static_cast<std::uint16_t> (value);
}

struct BuilderZone
{
    std::vector<GeneratorPair> generators;
};

struct BuilderPreset
{
    std::string   name    = "Preset";
    std::uint16_t bank    = 0;
    std::uint16_t program = 0;
    std::vector<BuilderZone> zones;
};

struct BuilderInstrument
{
    std::string name = "Instrument";
    std::vector<BuilderZone> zones;
};

struct BuilderSample
{
    std::string   name            = "Sample";
    std::uint32_t start           = 0;
    std::uint32_t end             = 64;
    std::uint32_t loopStart       = 8;
    std::uint32_t loopEnd         = 56;
    std::uint32_t sampleRate      = 44100;
    std::uint8_t  originalPitch   = 60;
    std::int8_t   pitchCorrection = 0;
    std::uint16_t sampleLink      = 0;
    std::uint16_t sampleType      = sampleTypeMono;
};

/**
    Builds a bank from an explicit hierarchy, or a minimal valid default when
    none is given: one preset -> one instrument -> one sample.

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

    /// Left empty for the default single-chain bank.
    std::vector<BuilderPreset>    presets;
    std::vector<BuilderInstrument> instruments;
    std::vector<BuilderSample>     samples;

    [[nodiscard]] std::vector<std::byte> build() const
    {
        std::vector<std::vector<std::byte>> topLevel;

        if (includeInfoList) topLevel.push_back (buildInfo());
        if (includeSdtaList) topLevel.push_back (buildSdta());
        if (includePdtaList) topLevel.push_back (buildPdta());

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
    [[nodiscard]] std::vector<BuilderPreset> effectivePresets() const
    {
        if (! presets.empty())
            return presets;

        BuilderPreset preset;
        preset.name = "Test Preset";
        preset.zones.push_back (BuilderZone { { { 41, 0 } } }); // instrument -> 0
        return { preset };
    }

    [[nodiscard]] std::vector<BuilderInstrument> effectiveInstruments() const
    {
        if (! instruments.empty())
            return instruments;

        BuilderInstrument inst;
        inst.name = "Test Instrument";
        inst.zones.push_back (BuilderZone { { { 53, 0 } } }); // sampleID -> 0
        return { inst };
    }

    [[nodiscard]] std::vector<BuilderSample> effectiveSamples() const
    {
        if (! samples.empty())
            return samples;

        BuilderSample sample;
        sample.name    = "Test Sample";
        sample.end     = static_cast<std::uint32_t> (sampleFrames);
        sample.loopEnd = sampleFrames > 8 ? static_cast<std::uint32_t> (sampleFrames - 8)
                                          : static_cast<std::uint32_t> (sampleFrames);
        return { sample };
    }

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
            w.zstr ("odd");
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

    [[nodiscard]] std::vector<std::byte> emitTable (std::string_view fourCC,
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
        const auto presetList     = effectivePresets();
        const auto instrumentList = effectiveInstruments();
        const auto sampleList     = effectiveSamples();

        std::vector<std::vector<std::byte>> chunks;

        auto emit = [&] (std::string_view fourCC, const std::vector<std::byte>& payload)
        {
            if (! omitPdtaChunk.empty() && omitPdtaChunk == fourCC)
                return;
            chunks.push_back (emitTable (fourCC, payload));
        };

        // ---- preset side ----
        ByteWriter phdr, pbag, pgen;
        std::uint16_t presetZoneCursor = 0;
        std::uint16_t presetGenCursor  = 0;

        for (const auto& preset : presetList)
        {
            phdr.name20 (preset.name);
            phdr.u16 (preset.program);
            phdr.u16 (preset.bank);
            phdr.u16 (presetZoneCursor);
            phdr.u32 (0); phdr.u32 (0); phdr.u32 (0);

            for (const auto& zone : preset.zones)
            {
                pbag.u16 (presetGenCursor);
                pbag.u16 (0); // modulators are out of scope (ADR-09)

                for (const auto& generator : zone.generators)
                {
                    pgen.u16 (generator.first);
                    pgen.u16 (generator.second);
                    ++presetGenCursor;
                }

                ++presetZoneCursor;
            }
        }

        phdr.name20 ("EOP");
        phdr.u16 (0); phdr.u16 (0); phdr.u16 (presetZoneCursor);
        phdr.u32 (0); phdr.u32 (0); phdr.u32 (0);

        pbag.u16 (presetGenCursor); pbag.u16 (0); // terminal bag
        pgen.u16 (0); pgen.u16 (0);               // terminal generator

        // ---- instrument side ----
        ByteWriter inst, ibag, igen;
        std::uint16_t instrumentZoneCursor = 0;
        std::uint16_t instrumentGenCursor  = 0;

        for (const auto& instrument : instrumentList)
        {
            inst.name20 (instrument.name);
            inst.u16 (instrumentZoneCursor);

            for (const auto& zone : instrument.zones)
            {
                ibag.u16 (instrumentGenCursor);
                ibag.u16 (0);

                for (const auto& generator : zone.generators)
                {
                    igen.u16 (generator.first);
                    igen.u16 (generator.second);
                    ++instrumentGenCursor;
                }

                ++instrumentZoneCursor;
            }
        }

        inst.name20 ("EOI");
        inst.u16 (instrumentZoneCursor);

        ibag.u16 (instrumentGenCursor); ibag.u16 (0);
        igen.u16 (0); igen.u16 (0);

        // ---- samples ----
        ByteWriter shdr;
        for (const auto& sample : sampleList)
        {
            shdr.name20 (sample.name);
            shdr.u32 (sample.start);
            shdr.u32 (sample.end);
            shdr.u32 (sample.loopStart);
            shdr.u32 (sample.loopEnd);
            shdr.u32 (sample.sampleRate);
            shdr.u8 (sample.originalPitch);
            shdr.u8 (static_cast<std::uint8_t> (sample.pitchCorrection));
            shdr.u16 (sample.sampleLink);
            shdr.u16 (sample.sampleType);
        }

        shdr.name20 ("EOS");
        shdr.u32 (0); shdr.u32 (0); shdr.u32 (0); shdr.u32 (0); shdr.u32 (0);
        shdr.u8 (0); shdr.u8 (0); shdr.u16 (0); shdr.u16 (0);

        ByteWriter terminalModulator;
        terminalModulator.u16 (0); terminalModulator.u16 (0);
        terminalModulator.s16 (0);
        terminalModulator.u16 (0); terminalModulator.u16 (0);

        emit ("phdr", phdr.bytes());
        emit ("pbag", pbag.bytes());
        emit ("pmod", terminalModulator.bytes());
        emit ("pgen", pgen.bytes());
        emit ("inst", inst.bytes());
        emit ("ibag", ibag.bytes());
        emit ("imod", terminalModulator.bytes());
        emit ("igen", igen.bytes());
        emit ("shdr", shdr.bytes());

        return makeList ("pdta", chunks);
    }
};

} // namespace x10::sf2::test
