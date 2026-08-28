#pragma once
#include "PresetModel.h"
#include <map>
namespace aod
{
class SoundFontResolver
{
public:
    enum class Status { resolved, relocated, missing };
    struct Result { Status status = Status::missing; juce::File file; };
    SoundFontResolver();
    explicit SoundFontResolver (juce::Array<juce::File> searchDirectories);
    void addSearchDirectory (const juce::File& directory);
    [[nodiscard]] Result resolve (const PresetDocument&) const;
    [[nodiscard]] Result resolve (const juce::String&, const juce::String&) const;
    void rememberRelink (const juce::String&, const juce::File&);
    [[nodiscard]] static juce::Array<juce::File> defaultSearchDirectories();
private:
    juce::Array<juce::File> searchDirectories_;
    std::map<juce::String, juce::File> relinks_;
};
}
