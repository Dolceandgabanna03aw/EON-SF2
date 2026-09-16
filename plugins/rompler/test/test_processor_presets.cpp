#include <catch2/catch_test_macros.hpp>
#include <cmath>

#include "PluginProcessor.h"
#include "PresetStorage.h"
#include "SoundFontResolver.h"

namespace
{
const aod::PresetDocument& requiredFactoryProfile (const char* fileName, aod::PresetStorage::ParseResult& storage)
{
    storage = aod::PresetStorage::readFile (juce::File (AOD_ROMPLER_FACTORY_PRESET_DIR).getChildFile (fileName));
    REQUIRE (storage.status == aod::PresetStorage::ParseStatus::ok);
    return storage.document;
}

float parameterValue (aod::RomplerProcessor& processor, const char* id)
{
    const auto* value = processor.getValueTreeState().getRawParameterValue (id);
    REQUIRE (value != nullptr);
    return value->load();
}
}

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

    REQUIRE (processor.applyPreset (document) == aod::RomplerProcessor::ApplyStatus::soundFontMissing);
    REQUIRE (std::abs (processor.getValueTreeState().getRawParameterValue (aod::ParamIDs::voiceDrive)->load() - before) < 1.0e-6f);
}

TEST_CASE ("factory sound profiles apply their resolved SoundFont and control values", "[preset][processor]")
{
    aod::PresetStorage::ParseResult clearStorage, characterStorage;
    const auto& clear = requiredFactoryProfile ("Aoi Clear.eonpreset", clearStorage);
    const auto& character = requiredFactoryProfile ("Aoi Character.eonpreset", characterStorage);
    const aod::SoundFontResolver resolver ({ juce::File (X10_SF2_CROSSCHECK_TESTDATA) });
    const auto resolved = resolver.resolve (clear);
    REQUIRE (resolved.status != aod::SoundFontResolver::Status::missing);

    aod::RomplerProcessor processor;
    REQUIRE (processor.applyPreset (clear, resolved.file) == aod::RomplerProcessor::ApplyStatus::ok);
    REQUIRE (std::abs (parameterValue (processor, aod::ParamIDs::voiceDrive)) < 1.0e-5f);
    REQUIRE (std::abs (parameterValue (processor, aod::ParamIDs::fxChorusMix) - 8.0f) < 1.0e-5f);
    REQUIRE (std::abs (parameterValue (processor, aod::ParamIDs::fxReverbMix) - 10.0f) < 1.0e-5f);

    REQUIRE (processor.applyPreset (character, resolved.file) == aod::RomplerProcessor::ApplyStatus::ok);
    REQUIRE (std::abs (parameterValue (processor, aod::ParamIDs::voiceDrive) - 12.0f) < 1.0e-5f);
    REQUIRE (std::abs (parameterValue (processor, aod::ParamIDs::busTapeDrive) - 14.0f) < 1.0e-5f);
    REQUIRE (std::abs (parameterValue (processor, aod::ParamIDs::busFilterCutoff) - 17000.0f) < 1.0f);
    REQUIRE (std::abs (parameterValue (processor, aod::ParamIDs::compMix) - 28.0f) < 1.0e-5f);
    REQUIRE (std::abs (parameterValue (processor, aod::ParamIDs::fxReverbMix) - 16.0f) < 1.0e-5f);
}
