#pragma once
#include "PresetModel.h"
#include "PresetStorage.h"
#include "SoundFontResolver.h"
#include <array>
#include <functional>
#include <vector>
namespace aod
{
struct PresetEntryModel { PresetDocument document; juce::File file; bool isFavourite = false; int quickSlot = 0; bool soundFontMissing = false; bool invalid = false; bool upgradeRequired = false; };
struct PresetQuery { juce::String text, category; bool favouritesOnly = false; bool userOnly = false; };
enum class LibraryStatus { ok, notFound, readOnly, writeFailed, invalidPackage, slotOutOfRange };
class PresetLibrary
{
public:
    static constexpr int quickSlotCount = 8;
    PresetLibrary (juce::File userRoot, juce::File factoryRoot);
    void rescan();
    [[nodiscard]] const std::vector<PresetEntryModel>& entries() const noexcept;
    [[nodiscard]] std::vector<const PresetEntryModel*> search (const PresetQuery&) const;
    [[nodiscard]] const PresetEntryModel* findByUuid (const juce::String&) const;
    [[nodiscard]] juce::StringArray categories() const;
    [[nodiscard]] LibraryStatus saveAs (PresetDocument, const juce::String&, juce::String&);
    [[nodiscard]] LibraryStatus overwrite (const juce::String&, PresetDocument);
    [[nodiscard]] LibraryStatus rename (const juce::String&, const juce::String&);
    [[nodiscard]] LibraryStatus duplicate (const juce::String&, juce::String&);
    [[nodiscard]] LibraryStatus remove (const juce::String&);
    [[nodiscard]] LibraryStatus setFavourite (const juce::String&, bool);
    [[nodiscard]] LibraryStatus setTags (const juce::String&, const juce::StringArray&);
    [[nodiscard]] LibraryStatus assignQuickSlot (int, const juce::String&);
    [[nodiscard]] LibraryStatus clearQuickSlot (int);
    [[nodiscard]] juce::String quickSlotUuid (int) const;
    [[nodiscard]] LibraryStatus importFile (const juce::File&, juce::String&);
    [[nodiscard]] LibraryStatus exportPresetOnly (const juce::String&, const juce::File&) const;
    [[nodiscard]] LibraryStatus exportPackage (const juce::String&, const juce::File&, bool) const;
    [[nodiscard]] juce::String proposeUniqueName (const juce::String&) const;
    void setSoundFontResolver (SoundFontResolver resolver);
    [[nodiscard]] SoundFontResolver::Result resolveSoundFont (const PresetDocument&) const;
    [[nodiscard]] LibraryStatus relinkSoundFont (const juce::String&, const juce::File&);
    std::function<void()> onCatalogueChanged;
private:
    void loadIndex(); [[nodiscard]] bool saveIndex() const; void applyIndexToEntries(); [[nodiscard]] juce::File indexFile() const; [[nodiscard]] PresetEntryModel* mutableEntry (const juce::String&);
    juce::File userRoot_, factoryRoot_; std::vector<PresetEntryModel> entries_; juce::StringArray favourites_; std::array<juce::String, quickSlotCount> quickSlots_ {}; SoundFontResolver resolver_;
};
}
