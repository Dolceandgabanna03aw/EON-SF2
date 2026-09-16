#include <catch2/catch_test_macros.hpp>

#include "PluginEditor.h"

TEST_CASE ("editor preset header accepts a document and clean state", "[preset][ui]")
{
    aod::RomplerProcessor processor;
    aod::RomplerEditor editor (processor);
    auto document = processor.capturePreset();
    document.name = "Test Patch";
    editor.setPresetHeaderDocument (document);
    REQUIRE_FALSE (editor.isPresetHeaderDirty());
    editor.markPresetHeaderSaved();
    REQUIRE_FALSE (editor.isPresetHeaderDirty());
}
