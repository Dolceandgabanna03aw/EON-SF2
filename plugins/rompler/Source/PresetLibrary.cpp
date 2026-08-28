#include "PresetLibrary.h"

namespace aod
{
namespace { juce::int64 now() { return juce::Time::currentTimeMillis(); } juce::String fold (juce::String s) { return s.toLowerCase(); } }

PresetLibrary::PresetLibrary (juce::File userRoot, juce::File factoryRoot) : userRoot_ (std::move (userRoot)), factoryRoot_ (std::move (factoryRoot))
{
    quickSlots_.fill ({}); loadIndex(); rescan();
}
const std::vector<PresetEntryModel>& PresetLibrary::entries() const noexcept { return entries_; }
void PresetLibrary::rescan()
{
    entries_.clear();
    for (const auto& root : { factoryRoot_, userRoot_ })
    {
        juce::Array<juce::File> files; if (root.isDirectory()) root.findChildFiles (files, juce::File::findFiles, true, "*" + PresetStorage::fileExtension());
        for (const auto& file : files)
        {
            const auto parsed = PresetStorage::readFile (file); PresetEntryModel entry; entry.file = file;
            if (parsed.status == PresetStorage::ParseStatus::ok) { entry.document = parsed.document; entry.upgradeRequired = false; }
            else { entry.invalid = parsed.status == PresetStorage::ParseStatus::malformed; entry.upgradeRequired = parsed.status == PresetStorage::ParseStatus::unsupportedSchema; entry.document.name = parsed.document.name; entry.document.schemaVersion = parsed.document.schemaVersion; }
            entry.soundFontMissing = ! entry.invalid && resolver_.resolve (entry.document).status == SoundFontResolver::Status::missing;
            entries_.push_back (std::move (entry));
        }
    }
    std::stable_sort (entries_.begin(), entries_.end(), [] (const auto& a, const auto& b) { if (a.document.source != b.document.source) return a.document.source == PresetSource::factory; return a.document.name.compareIgnoreCase (b.document.name) < 0; });
    applyIndexToEntries(); if (onCatalogueChanged) onCatalogueChanged();
}
std::vector<const PresetEntryModel*> PresetLibrary::search (const PresetQuery& query) const
{
    std::vector<const PresetEntryModel*> result; const auto needle = fold (query.text);
    for (const auto& entry : entries_)
    {
        if (query.userOnly && entry.document.source != PresetSource::user) continue;
        if (query.favouritesOnly && ! entry.isFavourite) continue;
        if (query.category.isNotEmpty() && ! fold (entry.document.category).contains (fold (query.category))) continue;
        if (needle.isNotEmpty()) { juce::String hay = entry.document.name + " " + entry.document.category + " " + entry.document.soundFontName; for (const auto& tag : entry.document.tags) hay += " " + tag; if (! fold (hay).contains (needle)) continue; }
        result.push_back (&entry);
    }
    return result;
}
const PresetEntryModel* PresetLibrary::findByUuid (const juce::String& uuid) const { for (const auto& e : entries_) if (e.document.uuid == uuid) return &e; return nullptr; }
juce::StringArray PresetLibrary::categories() const { juce::StringArray result; for (const auto& e : entries_) if (e.document.category.isNotEmpty() && ! result.contains (e.document.category)) result.add (e.document.category); result.sort (true); return result; }
PresetEntryModel* PresetLibrary::mutableEntry (const juce::String& uuid) { for (auto& e : entries_) if (e.document.uuid == uuid) return &e; return nullptr; }
juce::String PresetLibrary::proposeUniqueName (const juce::String& desired) const
{
    const auto base = PresetStorage::sanitiseFileName (desired); if (findByUuid ("") == nullptr) {}
    auto exists = [&] (const juce::String& value) { for (const auto& e : entries_) if (e.document.source == PresetSource::user && e.document.name == value) return true; return false; };
    if (! exists (base)) return base; for (int i = 2; ; ++i) { const auto candidate = base + " (" + juce::String (i) + ")"; if (! exists (candidate)) return candidate; }
}
LibraryStatus PresetLibrary::saveAs (PresetDocument document, const juce::String& displayName, juce::String& uuidOut)
{
    document.source = PresetSource::user; document.name = proposeUniqueName (displayName); document.uuid = juce::Uuid().toString(); document.createdAtMs = document.modifiedAtMs = now();
    const auto file = userRoot_.getChildFile (PresetStorage::sanitiseFileName (document.name) + PresetStorage::fileExtension());
    if (PresetStorage::writeAtomically (document, file) != PresetStorage::WriteStatus::ok) return LibraryStatus::writeFailed;
    uuidOut = document.uuid; rescan(); return LibraryStatus::ok;
}
LibraryStatus PresetLibrary::overwrite (const juce::String& uuid, PresetDocument document)
{
    auto* entry = mutableEntry (uuid); if (entry == nullptr) return LibraryStatus::notFound; if (entry->document.source == PresetSource::factory) return LibraryStatus::readOnly;
    document.uuid = uuid; document.source = PresetSource::user; document.name = entry->document.name; document.createdAtMs = entry->document.createdAtMs; document.modifiedAtMs = now();
    if (PresetStorage::writeAtomically (document, entry->file) != PresetStorage::WriteStatus::ok) return LibraryStatus::writeFailed; rescan(); return LibraryStatus::ok;
}
LibraryStatus PresetLibrary::rename (const juce::String& uuid, const juce::String& newName)
{
    auto* entry = mutableEntry (uuid); if (entry == nullptr) return LibraryStatus::notFound; if (entry->document.source == PresetSource::factory) return LibraryStatus::readOnly;
    const auto name = proposeUniqueName (newName); const auto destination = userRoot_.getChildFile (PresetStorage::sanitiseFileName (name) + PresetStorage::fileExtension());
    auto renamed = entry->document; renamed.name = name; renamed.modifiedAtMs = now();
    if (PresetStorage::writeAtomically (renamed, destination) != PresetStorage::WriteStatus::ok) return LibraryStatus::writeFailed;
    if (! entry->file.deleteFile()) { destination.deleteFile(); return LibraryStatus::writeFailed; }
    rescan(); return LibraryStatus::ok;
}
LibraryStatus PresetLibrary::duplicate (const juce::String& uuid, juce::String& newUuidOut) { const auto* entry = findByUuid (uuid); if (entry == nullptr) return LibraryStatus::notFound; auto copy = entry->document; copy.source = PresetSource::user; return saveAs (copy, entry->document.name, newUuidOut); }
LibraryStatus PresetLibrary::remove (const juce::String& uuid) { auto* entry = mutableEntry (uuid); if (entry == nullptr) return LibraryStatus::notFound; if (entry->document.source == PresetSource::factory) return LibraryStatus::readOnly; if (! entry->file.deleteFile()) return LibraryStatus::writeFailed; rescan(); return LibraryStatus::ok; }
LibraryStatus PresetLibrary::setFavourite (const juce::String& uuid, bool value) { if (findByUuid (uuid) == nullptr) return LibraryStatus::notFound; favourites_.removeString (uuid); if (value) favourites_.add (uuid); if (! saveIndex()) return LibraryStatus::writeFailed; applyIndexToEntries(); if (onCatalogueChanged) onCatalogueChanged(); return LibraryStatus::ok; }
LibraryStatus PresetLibrary::setTags (const juce::String& uuid, const juce::StringArray& tags) { auto* e = mutableEntry (uuid); if (e == nullptr) return LibraryStatus::notFound; if (e->document.source == PresetSource::factory) return LibraryStatus::readOnly; e->document.tags = tags; return overwrite (uuid, e->document); }
LibraryStatus PresetLibrary::assignQuickSlot (int slot, const juce::String& uuid) { if (slot < 1 || slot > quickSlotCount) return LibraryStatus::slotOutOfRange; if (findByUuid (uuid) == nullptr) return LibraryStatus::notFound; quickSlots_[(size_t) slot - 1] = uuid; if (! saveIndex()) return LibraryStatus::writeFailed; applyIndexToEntries(); if (onCatalogueChanged) onCatalogueChanged(); return LibraryStatus::ok; }
LibraryStatus PresetLibrary::clearQuickSlot (int slot) { if (slot < 1 || slot > quickSlotCount) return LibraryStatus::slotOutOfRange; quickSlots_[(size_t) slot - 1].clear(); if (! saveIndex()) return LibraryStatus::writeFailed; applyIndexToEntries(); if (onCatalogueChanged) onCatalogueChanged(); return LibraryStatus::ok; }
juce::String PresetLibrary::quickSlotUuid (int slot) const { return slot >= 1 && slot <= quickSlotCount ? quickSlots_[(size_t) slot - 1] : juce::String(); }
LibraryStatus PresetLibrary::importFile (const juce::File& source, juce::String& uuidOut) { const auto parsed = PresetStorage::readFile (source); if (parsed.status != PresetStorage::ParseStatus::ok) return LibraryStatus::invalidPackage; return saveAs (parsed.document, parsed.document.name, uuidOut); }
LibraryStatus PresetLibrary::exportPresetOnly (const juce::String& uuid, const juce::File& destination) const { const auto* e = findByUuid (uuid); if (e == nullptr) return LibraryStatus::notFound; return PresetStorage::writeAtomically (e->document, destination) == PresetStorage::WriteStatus::ok ? LibraryStatus::ok : LibraryStatus::writeFailed; }
LibraryStatus PresetLibrary::exportPackage (const juce::String& uuid, const juce::File& destination, bool) const { return exportPresetOnly (uuid, destination); }
void PresetLibrary::setSoundFontResolver (SoundFontResolver resolver) { resolver_ = std::move (resolver); rescan(); }
LibraryStatus PresetLibrary::relinkSoundFont (const juce::String& uuid, const juce::File& chosen) { auto* e = mutableEntry (uuid); if (e == nullptr) return LibraryStatus::notFound; resolver_.rememberRelink (e->document.soundFontName, chosen); e->soundFontMissing = false; if (onCatalogueChanged) onCatalogueChanged(); return LibraryStatus::ok; }
juce::File PresetLibrary::indexFile() const { return userRoot_.getChildFile ("library-index.json"); }
void PresetLibrary::loadIndex() { const auto value = juce::JSON::parse (indexFile().loadFileAsString()); auto* object = value.getDynamicObject(); if (object == nullptr) return; if (const auto* a = object->getProperty ("favourites").getArray()) for (const auto& item : *a) if (item.isString()) favourites_.addIfNotAlreadyThere (item.toString()); if (const auto* a = object->getProperty ("quickSlots").getArray()) for (int i = 0; i < juce::jmin (quickSlotCount, a->size()); ++i) if ((*a)[i].isString()) quickSlots_[(size_t) i] = (*a)[i].toString(); }
bool PresetLibrary::saveIndex() const
{
    juce::DynamicObject::Ptr root (new juce::DynamicObject()); root->setProperty ("schemaVersion", 1);
    juce::Array<juce::var> fav; for (const auto& id : favourites_) fav.add (id); root->setProperty ("favourites", juce::var (fav));
    juce::Array<juce::var> slots; for (const auto& id : quickSlots_) slots.add (id); root->setProperty ("quickSlots", juce::var (slots));
    const auto temporary = indexFile().getSiblingFile (".library-index.tmp-" + juce::Uuid().toString());
    { juce::FileOutputStream stream (temporary); if (! stream.openedOk() || ! stream.writeString (juce::JSON::toString (juce::var (root.get()), false))) { temporary.deleteFile(); return false; } stream.flush(); }
    if (! indexFile().replaceWithFile (temporary)) { temporary.deleteFile(); return false; }
    return true;
}
void PresetLibrary::applyIndexToEntries() { for (auto& e : entries_) { e.isFavourite = favourites_.contains (e.document.uuid); e.quickSlot = 0; for (int i = 0; i < quickSlotCount; ++i) if (quickSlots_[(size_t) i] == e.document.uuid) e.quickSlot = i + 1; } }
}
