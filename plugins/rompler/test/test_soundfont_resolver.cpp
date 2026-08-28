#include <catch2/catch_test_macros.hpp>
#include "SoundFontResolver.h"
TEST_CASE ("missing soundfont is reported", "[preset][soundfont]") { aod::PresetDocument d; d.soundFontPath = "/does/not/exist.sf2"; REQUIRE (aod::SoundFontResolver().resolve (d).status == aod::SoundFontResolver::Status::missing); }
