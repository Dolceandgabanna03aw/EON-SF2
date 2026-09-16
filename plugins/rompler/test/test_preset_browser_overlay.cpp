#include <catch2/catch_test_macros.hpp>

#include "PresetBrowserOverlay.h"

TEST_CASE ("preset browser is an opaque interactive overlay", "[preset][ui]")
{
    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("eon-overlay-test");
    aod::PresetLibrary library (root.getChildFile ("user"), root.getChildFile ("factory"));
    aod::PresetBrowserOverlay overlay (library);
    overlay.setSize (900, 600);

    REQUIRE (overlay.isOpaque());
    bool selfClicks = false;
    bool childClicks = false;
    overlay.getInterceptsMouseClicks (selfClicks, childClicks);
    REQUIRE (selfClicks);
    REQUIRE (childClicks);
    REQUIRE (overlay.getNumChildComponents() >= 8);
}
