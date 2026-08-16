# Format-neutral instrument schema.
#
# Deliberately separate from x10_sf2: SoundFont is an importer, not the internal
# format (planning document, issue A). Anything here that needs to know which
# file format a region came from is a design failure.
#
# STATIC rather than INTERFACE now that RegionIndex has real logic behind it —
# Region.h and Preset.h stay header-only, but a lookup table built over 128 MIDI
# keys per preset is not something to leave as an inline template.

add_library(x10_instrument STATIC
    src/RegionIndex.cpp)
add_library(x10::instrument ALIAS x10_instrument)

target_include_directories(x10_instrument PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(x10_instrument PUBLIC cxx_std_20)
target_link_libraries(x10_instrument PRIVATE x10::strict)
