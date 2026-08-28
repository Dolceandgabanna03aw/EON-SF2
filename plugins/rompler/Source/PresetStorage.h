#pragma once
#include "PresetModel.h"
namespace aod
{
class PresetStorage
{
public:
    enum class ParseStatus { ok, malformed, unsupportedSchema };
    struct ParseResult { ParseStatus status = ParseStatus::malformed; PresetDocument document; };
    enum class WriteStatus { ok, temporaryFileFailed, replaceFailed };
    [[nodiscard]] static juce::String toJson (const PresetDocument&);
    [[nodiscard]] static ParseResult fromJson (const juce::String&);
    [[nodiscard]] static WriteStatus writeAtomically (const PresetDocument&, const juce::File&);
    [[nodiscard]] static ParseResult readFile (const juce::File&);
    [[nodiscard]] static juce::File userLibraryDirectory();
    [[nodiscard]] static juce::File factoryLibraryDirectory();
    [[nodiscard]] static juce::String fileExtension();
    [[nodiscard]] static juce::String packageExtension();
    [[nodiscard]] static juce::String sanitiseFileName (const juce::String&);
    PresetStorage() = delete;
};
}
