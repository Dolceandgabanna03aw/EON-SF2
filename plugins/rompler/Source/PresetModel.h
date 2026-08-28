#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace aod
{
enum class PresetSource { user, factory };

struct PresetParameterValue
{
    juce::String id;
    bool isChoice = false;
    float value = 0.0f;
    juce::String text;
};

struct PresetDocument
{
    static constexpr int currentSchemaVersion = 1;
    int schemaVersion = currentSchemaVersion;
    juce::String uuid, name, category;
    juce::StringArray tags;
    PresetSource source = PresetSource::user;
    juce::int64 createdAtMs = 0, modifiedAtMs = 0;
    juce::String soundFontName, soundFontPath;
    int bank = 0, program = 0;
    std::vector<PresetParameterValue> parameters;

    [[nodiscard]] const PresetParameterValue* find (juce::StringRef id) const noexcept
    {
        for (const auto& entry : parameters)
            if (entry.id == id) return &entry;
        return nullptr;
    }
};
}
