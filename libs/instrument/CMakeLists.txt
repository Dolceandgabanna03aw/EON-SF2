# Format-neutral instrument schema.
#
# Deliberately separate from x10_sf2: SoundFont is an importer, not the internal
# format (planning document, issue A). Anything here that needs to know which
# file format a region came from is a design failure.

add_library(x10_instrument INTERFACE)
add_library(x10::instrument ALIAS x10_instrument)

target_include_directories(x10_instrument INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(x10_instrument INTERFACE cxx_std_20)
