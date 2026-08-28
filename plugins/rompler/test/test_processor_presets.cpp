#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"

TEST_CASE ("processor preset capture includes sound parameters but excludes engine settings", "[preset][processor]")
{
    aod::RomplerProcessor processor;
    const auto document = processor.capturePreset();

    REQUIRE (document.parameters.size() == aod::ParamSets::rotary.size() + aod::ParamSets::presetChoices.size());
    REQUIRE (document.find (aod::ParamIDs::busOsFactor) == nullptr);
    REQUIRE (document.find (aod::ParamIDs::polyLimit) == nullptr);
    REQUIRE (document.find (aod::ParamIDs::voiceDrive) != nullptr);
    REQUIRE (document.find (aod::ParamIDs::voiceCurve) != nullptr);
}

TEST_CASE ("processor preset application reports a missing SoundFont without changing parameters", "[preset][processor]")
{
    aod::RomplerProcessor processor;
    const auto before = processor.getValueTreeState().getRawParameterValue (aod::ParamIDs::voiceDrive)->load();

    aod::PresetDocument document;
    document.soundFontPath = "/definitely/missing/eon-test.sf2";
    document.parameters.push_back ({ aod::ParamIDs::voiceDrive, false, 0.9f, {} });

    REQUIRE (processor.applyPreset (document) == aod::PresetApplyStatus::soundFontMissing);
    REQUIRE (processor.getValueTreeState().getRawParameterValue (aod::ParamIDs::voiceDrive)->load() == before);
}
