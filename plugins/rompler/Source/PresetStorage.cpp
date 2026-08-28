#include "PresetStorage.h"
#include <cmath>

namespace aod
{
namespace
{
juce::var arrayToVar (const juce::StringArray& values)
{
    juce::Array<juce::var> result;
    for (const auto& value : values) result.add (value);
    return juce::var (result);
}

bool has (const juce::DynamicObject* object, const char* key) { return object != nullptr && object->hasProperty (key); }
bool integer (const juce::var& value) { return value.isInt() || value.isInt64(); }
}

juce::String PresetStorage::toJson (const PresetDocument& document)
{
    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("schemaVersion", document.schemaVersion);
    root->setProperty ("uuid", document.uuid); root->setProperty ("name", document.name);
    root->setProperty ("category", document.category); root->setProperty ("tags", arrayToVar (document.tags));
    root->setProperty ("source", document.source == PresetSource::factory ? "factory" : "user");
    root->setProperty ("createdAtMs", document.createdAtMs); root->setProperty ("modifiedAtMs", document.modifiedAtMs);
    root->setProperty ("soundFontName", document.soundFontName); root->setProperty ("soundFontPath", document.soundFontPath);
    root->setProperty ("bank", document.bank); root->setProperty ("program", document.program);
    juce::Array<juce::var> parameters;
    for (const auto& parameter : document.parameters)
    {
        juce::DynamicObject::Ptr item (new juce::DynamicObject());
        item->setProperty ("id", parameter.id); item->setProperty ("isChoice", parameter.isChoice);
        if (parameter.isChoice) item->setProperty ("text", parameter.text);
        else item->setProperty ("value", parameter.value);
        parameters.add (juce::var (item.get()));
    }
    root->setProperty ("parameters", juce::var (parameters));
    return juce::JSON::toString (juce::var (root.get()), false);
}

PresetStorage::ParseResult PresetStorage::fromJson (const juce::String& json)
{
    ParseResult result;
    juce::var parsed;
    juce::Result parseResult = juce::JSON::parse (json, parsed);
    auto* root = parsed.getDynamicObject();
    if (parseResult.failed() || root == nullptr) return result;
    if (has (root, "name") && root->getProperty ("name").isString()) result.document.name = root->getProperty ("name").toString();
    if (has (root, "schemaVersion") && integer (root->getProperty ("schemaVersion"))) result.document.schemaVersion = (int) root->getProperty ("schemaVersion");
    if (result.document.schemaVersion > PresetDocument::currentSchemaVersion) { result.status = ParseStatus::unsupportedSchema; return result; }
    const char* required[] = { "schemaVersion", "uuid", "name", "category", "tags", "source", "createdAtMs", "modifiedAtMs", "soundFontName", "soundFontPath", "bank", "program", "parameters" };
    for (const auto* key : required) if (! has (root, key)) return result;
    auto stringProperty = [&] (const char* key, juce::String& out) { const auto value = root->getProperty (key); if (! value.isString()) return false; out = value.toString(); return true; };
    if (! integer (root->getProperty ("schemaVersion")) || ! stringProperty ("uuid", result.document.uuid) || ! stringProperty ("name", result.document.name)
        || ! stringProperty ("category", result.document.category) || ! stringProperty ("soundFontName", result.document.soundFontName)
        || ! stringProperty ("soundFontPath", result.document.soundFontPath) || ! integer (root->getProperty ("createdAtMs"))
        || ! integer (root->getProperty ("modifiedAtMs")) || ! integer (root->getProperty ("bank")) || ! integer (root->getProperty ("program"))) return result;
    result.document.schemaVersion = (int) root->getProperty ("schemaVersion");
    result.document.createdAtMs = (juce::int64) root->getProperty ("createdAtMs"); result.document.modifiedAtMs = (juce::int64) root->getProperty ("modifiedAtMs");
    result.document.bank = (int) root->getProperty ("bank"); result.document.program = (int) root->getProperty ("program");
    const auto source = root->getProperty ("source");
    if (! source.isString() || (source.toString() != "user" && source.toString() != "factory")) return result;
    result.document.source = source.toString() == "factory" ? PresetSource::factory : PresetSource::user;
    if (! root->getProperty ("tags").isArray()) return result;
    for (const auto& tag : *root->getProperty ("tags").getArray()) { if (! tag.isString()) return result; result.document.tags.add (tag.toString()); }
    const auto parameterVar = root->getProperty ("parameters");
    if (! parameterVar.isArray()) return result;
    juce::StringArray ids;
    for (const auto& value : *parameterVar.getArray())
    {
        auto* item = value.getDynamicObject(); if (item == nullptr || ! has (item, "id") || ! has (item, "isChoice") || ! item->getProperty ("id").isString() || ! item->getProperty ("isChoice").isBool()) return result;
        PresetParameterValue parameter; parameter.id = item->getProperty ("id").toString(); parameter.isChoice = (bool) item->getProperty ("isChoice");
        if (parameter.id.isEmpty() || ids.contains (parameter.id)) return result; ids.add (parameter.id);
        if (parameter.isChoice) { if (! has (item, "text") || ! item->getProperty ("text").isString()) return result; parameter.text = item->getProperty ("text").toString(); }
        else { if (! has (item, "value") || (! item->getProperty ("value").isDouble() && ! item->getProperty ("value").isInt())) return result; parameter.value = (float) item->getProperty ("value"); if (! std::isfinite (parameter.value) || parameter.value < 0.0f || parameter.value > 1.0f) return result; }
        result.document.parameters.push_back (std::move (parameter));
    }
    result.status = ParseStatus::ok; return result;
}

PresetStorage::WriteStatus PresetStorage::writeAtomically (const PresetDocument& document, const juce::File& destination)
{
    if (! destination.getParentDirectory().createDirectory().wasOk()) return WriteStatus::temporaryFileFailed;
    const auto temporary = destination.getSiblingFile ("." + destination.getFileName() + ".tmp-" + juce::Uuid().toString());
    { juce::FileOutputStream stream (temporary); if (! stream.openedOk() || ! stream.writeString (toJson (document))) { temporary.deleteFile(); return WriteStatus::temporaryFileFailed; } stream.flush(); }
    const bool replaced = destination.replaceWithFile (temporary);
    if (! replaced) { temporary.deleteFile(); return WriteStatus::replaceFailed; }
    return WriteStatus::ok;
}
PresetStorage::ParseResult PresetStorage::readFile (const juce::File& file) { return file.existsAsFile() ? fromJson (file.loadFileAsString()) : ParseResult{}; }
juce::File PresetStorage::userLibraryDirectory() { return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("EON SF2").getChildFile ("Presets"); }
juce::File PresetStorage::factoryLibraryDirectory() { return juce::File::getSpecialLocation (juce::File::currentApplicationFile).getChildFile ("Contents/Resources/Presets"); }
juce::String PresetStorage::fileExtension() { return ".eonpreset"; }
juce::String PresetStorage::packageExtension() { return ".eonpack"; }
juce::String PresetStorage::sanitiseFileName (const juce::String& displayName)
{
    auto name = displayName.trim().replaceCharacters ("/:\\*?\"<>|", "_________");
    name = name.replaceCharacters ("\r\n\t", "   "); while (name.contains ("  ")) name = name.replace ("  ", " ");
    if (name.isEmpty() || name == "." || name == "..") name = "Untitled";
    return name;
}
}
