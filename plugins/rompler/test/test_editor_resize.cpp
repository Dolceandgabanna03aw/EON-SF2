#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>
#include <utility>

#include "PluginEditor.h"
#include "PluginProcessor.h"

TEST_CASE ("editor resize preserves the skin canvas aspect ratio", "[plugin][ui][layout]")
{
    const juce::ScopedJuceInitialiser_GUI gui;
    aod::RomplerProcessor processor;
    aod::RomplerEditor editor (processor);

    constexpr double designAspect = 1563.0 / 1006.0;
    REQUIRE (editor.isResizable());
    REQUIRE (editor.getConstrainer() != nullptr);
    REQUIRE (std::abs (editor.getConstrainer()->getFixedAspectRatio() - designAspect) < 1.0e-6);

    for (const auto& size : { std::pair<int, int> { 960, 618 },
                              std::pair<int, int> { 1200, 800 },
                              std::pair<int, int> { 1563, 1006 },
                              std::pair<int, int> { 2000, 1000 } })
    {
        const auto [width, height] = size;
        editor.setSize (width, height);
        const auto canvas = editor.getSkinCanvasBoundsForTesting();
        const double actualAspect = static_cast<double> (canvas.getWidth())
                                  / static_cast<double> (canvas.getHeight());

        INFO ("requested size: " << width << "x" << height);
        INFO ("canvas size: " << canvas.getWidth() << "x" << canvas.getHeight());
        REQUIRE (canvas.getWidth() > 0);
        REQUIRE (canvas.getHeight() > 0);
        REQUIRE (std::abs (actualAspect - designAspect) < 0.003);
        REQUIRE (canvas.getX() >= 0);
        REQUIRE (canvas.getY() >= 0);
        REQUIRE (canvas.getRight() <= width);
        REQUIRE (canvas.getBottom() <= height);
    }
}
