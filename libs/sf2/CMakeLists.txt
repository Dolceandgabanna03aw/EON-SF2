# x10_sf2 — SoundFont 2 container reader.
#
# Kept separate from the eventual Region schema on purpose (planning document,
# issue A): SF2 is one importer, not the internal format. Swapping in an SFZ or
# a native importer must mean adding a sibling to this directory, not editing
# the engine.
#
# JUCE-free, like x10_dsp, so parser tests run in milliseconds.

add_library(x10_sf2 STATIC
    src/Sf2Reader.cpp
    src/Sf2Flattener.cpp)
add_library(x10::sf2 ALIAS x10_sf2)
set_target_properties(x10_sf2 PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(x10_sf2 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(x10_sf2 PUBLIC cxx_std_20)

# The Region schema is public: callers of flatten() receive them.
target_link_libraries(x10_sf2 PUBLIC x10::instrument)
target_link_libraries(x10_sf2 PRIVATE x10::strict)
