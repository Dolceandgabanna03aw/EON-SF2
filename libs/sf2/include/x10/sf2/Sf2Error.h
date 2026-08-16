#pragma once

namespace x10::sf2
{

/**
    Every way reading a bank can fail.

    The reader never throws and never asserts on malformed input. A SoundFont is
    untrusted data — the corpus this project targets is thirty years of files
    written by dozens of tools, some of which got the spec wrong — so a bad file
    has to come back as a value, not as a crash in the host's process.
*/
enum class Sf2Error
{
    ok = 0,

    tooSmall,              ///< Not even a RIFF header fits.
    notRiff,               ///< Missing the 'RIFF' magic.
    notSfbk,               ///< RIFF container, but not a SoundFont.
    truncatedChunk,        ///< A chunk claims more bytes than the file holds.
    missingInfoList,
    missingSdtaList,
    missingPdtaList,
    missingChunk,          ///< A required chunk inside pdta is absent.
    badVersion,            ///< ifil major version is not 2.
    badRecordSize,         ///< A record table's size is not a multiple of its record.
    emptyRecordTable,      ///< A required table has not even a terminal record.
    inconsistentSampleData ///< smpl/sm24 sizes disagree.
};

[[nodiscard]] constexpr const char* toString (Sf2Error error) noexcept
{
    switch (error)
    {
        case Sf2Error::ok:                     return "ok";
        case Sf2Error::tooSmall:               return "file is too small to be a RIFF container";
        case Sf2Error::notRiff:                return "missing RIFF magic";
        case Sf2Error::notSfbk:                return "RIFF container is not an sfbk";
        case Sf2Error::truncatedChunk:         return "chunk extends past the end of the file";
        case Sf2Error::missingInfoList:        return "missing LIST INFO";
        case Sf2Error::missingSdtaList:        return "missing LIST sdta";
        case Sf2Error::missingPdtaList:        return "missing LIST pdta";
        case Sf2Error::missingChunk:           return "a required pdta chunk is absent";
        case Sf2Error::badVersion:             return "unsupported SoundFont major version";
        case Sf2Error::badRecordSize:          return "record table size is not a multiple of the record size";
        case Sf2Error::emptyRecordTable:       return "record table has no terminal record";
        case Sf2Error::inconsistentSampleData: return "smpl and sm24 sizes disagree";
    }
    return "unknown";
}

} // namespace x10::sf2
