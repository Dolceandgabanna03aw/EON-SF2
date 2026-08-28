#include <catch2/catch_test_macros.hpp>
#include "PresetStorage.h"
TEST_CASE ("preset JSON round trip", "[preset][storage]")
{
    aod::PresetDocument d; d.uuid = "u"; d.name = "Bell"; d.category = "Keys"; d.tags = { "bright" }; d.parameters.push_back ({ "voice.drive", false, 0.5f, {} });
    const auto p = aod::PresetStorage::fromJson (aod::PresetStorage::toJson (d)); REQUIRE (p.status == aod::PresetStorage::ParseStatus::ok); REQUIRE (p.document.find ("voice.drive") != nullptr);
}
