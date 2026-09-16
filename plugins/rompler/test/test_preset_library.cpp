#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include "PluginProcessor.h"
#include "PresetLibrary.h"
TEST_CASE ("empty preset library scans", "[preset][library]") { const auto root = juce::File::createTempFile ("eon-lib").getSiblingFile ("eon-lib-dir"); root.deleteFile(); root.createDirectory(); aod::PresetLibrary library (root, root.getChildFile ("factory")); library.rescan(); REQUIRE (library.entries().empty()); root.deleteRecursively(); }

namespace
{
const aod::PresetParameterValue& requiredParameter (const aod::PresetDocument& document, const char* id)
{
    const auto* parameter = document.find (id);
    REQUIRE (parameter != nullptr);
    return *parameter;
}

float rawPresetValue (aod::RomplerProcessor& processor, const aod::PresetDocument& document, const char* id)
{
    const auto* parameter = processor.getValueTreeState().getParameter (id);
    REQUIRE (parameter != nullptr);
    CAPTURE (id, requiredParameter (document, id).value, parameter->getNormalisableRange().skew);
    return parameter->convertFrom0to1 (requiredParameter (document, id).value);
}
}

TEST_CASE ("factory sound profiles are complete and resolve their bundled SoundFont", "[preset][factory]")
{
    const auto userRoot = juce::File::createTempFile ("aoi-yume-user").getSiblingFile ("aoi-yume-user-dir");
    userRoot.deleteFile();
    REQUIRE (userRoot.createDirectory().wasOk());

    const juce::File factoryRoot (AOD_ROMPLER_FACTORY_PRESET_DIR);
    aod::PresetLibrary library (userRoot, factoryRoot);
    library.setSoundFontResolver (aod::SoundFontResolver ({ juce::File (X10_SF2_CROSSCHECK_TESTDATA) }));

    const auto* clear = library.findByUuid ("aoi-yume-clear-v1");
    const auto* character = library.findByUuid ("aoi-yume-character-v1");
    REQUIRE (clear != nullptr);
    REQUIRE (character != nullptr);
    REQUIRE (clear->document.source == aod::PresetSource::factory);
    REQUIRE (character->document.source == aod::PresetSource::factory);
    REQUIRE (clear->document.soundFontName == "Crystal Legacy.sf2");
    REQUIRE (character->document.soundFontName == "Crystal Legacy.sf2");
    REQUIRE_FALSE (clear->soundFontMissing);
    REQUIRE_FALSE (character->soundFontMissing);
    REQUIRE (library.resolveSoundFont (clear->document).file.getFileName() == "Crystal Legacy.sf2");
    REQUIRE (library.resolveSoundFont (character->document).file.getFileName() == "Crystal Legacy.sf2");

    for (const auto* id : aod::ParamSets::rotary)
    {
        REQUIRE (clear->document.find (id) != nullptr);
        REQUIRE (character->document.find (id) != nullptr);
    }
    for (const auto* id : aod::ParamSets::presetChoices)
    {
        REQUIRE (clear->document.find (id) != nullptr);
        REQUIRE (character->document.find (id) != nullptr);
    }

    aod::RomplerProcessor processor;
    REQUIRE (std::abs (rawPresetValue (processor, clear->document, aod::ParamIDs::fxChorusRate) - 0.65f) < 0.01f);
    REQUIRE (std::abs (rawPresetValue (processor, clear->document, aod::ParamIDs::fxChorusMix) - 8.0f) < 0.01f);
    REQUIRE (std::abs (rawPresetValue (processor, clear->document, aod::ParamIDs::fxReverbRoom) - 28.0f) < 0.01f);
    REQUIRE (std::abs (rawPresetValue (processor, clear->document, aod::ParamIDs::fxReverbMix) - 10.0f) < 0.01f);
    REQUIRE (std::abs (rawPresetValue (processor, character->document, aod::ParamIDs::voiceDrive) - 12.0f) < 0.01f);
    REQUIRE (std::abs (rawPresetValue (processor, character->document, aod::ParamIDs::busTapeDrive) - 14.0f) < 0.01f);
    // Character has compression and ambience by design, so its visible trim
    // compensates their level loss and lands near the Clear profile in a DAW
    // A/B without an invisible gain stage.
    REQUIRE (std::abs (rawPresetValue (processor, character->document, aod::ParamIDs::outTrim) - 0.0f) < 0.01f);
    const auto* cutoff = processor.getValueTreeState().getParameter (aod::ParamIDs::busFilterCutoff);
    REQUIRE (cutoff != nullptr);
    const auto expectedCharacterCutoff = cutoff->convertTo0to1 (17000.0f);
    CAPTURE (expectedCharacterCutoff);
    REQUIRE (std::abs (requiredParameter (character->document, aod::ParamIDs::busFilterCutoff).value - expectedCharacterCutoff) < 1.0e-6f);
    REQUIRE (std::abs (rawPresetValue (processor, character->document, aod::ParamIDs::busFilterCutoff) - 17000.0f) < 1.0f);
    REQUIRE (std::abs (rawPresetValue (processor, character->document, aod::ParamIDs::compMix) - 28.0f) < 0.01f);
    REQUIRE (std::abs (rawPresetValue (processor, character->document, aod::ParamIDs::fxReverbMix) - 16.0f) < 0.01f);

    userRoot.deleteRecursively();
}
