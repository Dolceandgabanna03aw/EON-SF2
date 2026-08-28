#include <catch2/catch_test_macros.hpp>
#include "PresetLibrary.h"
TEST_CASE ("empty preset library scans", "[preset][library]") { const auto root = juce::File::createTempFile ("eon-lib").getSiblingFile ("eon-lib-dir"); root.deleteFile(); root.createDirectory(); aod::PresetLibrary library (root, root.getChildFile ("factory")); library.rescan(); REQUIRE (library.entries().empty()); root.deleteRecursively(); }
