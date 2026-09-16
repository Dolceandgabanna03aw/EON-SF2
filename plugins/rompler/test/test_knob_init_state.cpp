#include <catch2/catch_test_macros.hpp>

#include "PluginEditor.h"

TEST_CASE ("knob reports its parameter init state", "[preset][ui]")
{
    aod::RomplerProcessor processor;
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (
        processor.getValueTreeState().getParameter (aod::ParamIDs::voiceDrive));
    REQUIRE (parameter != nullptr);

    aod::Knob knob (*parameter);
    knob.syncFromParameter();
    REQUIRE (knob.isAtInitState());
    parameter->setValueNotifyingHost (0.25f);
    knob.syncFromParameter();
    REQUIRE_FALSE (knob.isAtInitState());
}
