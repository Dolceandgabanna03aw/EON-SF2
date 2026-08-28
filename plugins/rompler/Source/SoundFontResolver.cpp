#include "SoundFontResolver.h"

namespace aod
{
SoundFontResolver::SoundFontResolver() : searchDirectories_ (defaultSearchDirectories()) {}
SoundFontResolver::SoundFontResolver (juce::Array<juce::File> directories) : searchDirectories_ (std::move (directories)) {}
void SoundFontResolver::addSearchDirectory (const juce::File& directory)
{
    if (directory.isDirectory() && ! searchDirectories_.contains (directory)) searchDirectories_.add (directory);
}
SoundFontResolver::Result SoundFontResolver::resolve (const PresetDocument& document) const { return resolve (document.soundFontPath, document.soundFontName); }
SoundFontResolver::Result SoundFontResolver::resolve (const juce::String& storedPath, const juce::String& fileName) const
{
    const juce::File stored (storedPath);
    if (stored.existsAsFile()) return { Status::resolved, stored };
    const auto relink = relinks_.find (fileName);
    if (relink != relinks_.end() && relink->second.existsAsFile()) return { Status::relocated, relink->second };
    for (const auto& directory : searchDirectories_)
    {
        const auto candidate = directory.getChildFile (fileName);
        if (candidate.existsAsFile()) return { Status::relocated, candidate };
    }
    return {};
}
void SoundFontResolver::rememberRelink (const juce::String& fileName, const juce::File& chosen) { if (chosen.existsAsFile()) relinks_[fileName] = chosen; }
juce::Array<juce::File> SoundFontResolver::defaultSearchDirectories()
{
    juce::Array<juce::File> result;
    const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    const auto bundled = app.getChildFile ("Contents/Resources/SoundFonts");
    const auto user = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("EON SF2/SoundFonts");
    if (bundled.isDirectory()) result.add (bundled);
    if (user.isDirectory() && ! result.contains (user)) result.add (user);
    return result;
}
}
