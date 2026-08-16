# Offline development tool. FluidSynth is LGPL: this target is never linked into
# the plugin and is never distributed (planning document §10). It is built only
# when the library is present.

find_path(FLUIDSYNTH_INCLUDE_DIR fluidsynth.h
    HINTS /opt/homebrew/include /usr/local/include)

find_library(FLUIDSYNTH_LIBRARY
    NAMES fluidsynth
    HINTS /opt/homebrew/lib /usr/local/lib)

if(NOT FLUIDSYNTH_INCLUDE_DIR OR NOT FLUIDSYNTH_LIBRARY)
    message(STATUS "FluidSynth not found — sf2_crosscheck will not be built. "
                   "Install it with: brew install fluid-synth")
    return()
endif()

add_executable(sf2_crosscheck main.cpp)

target_include_directories(sf2_crosscheck PRIVATE "${FLUIDSYNTH_INCLUDE_DIR}")
target_link_libraries(sf2_crosscheck PRIVATE x10::sf2 x10::strict "${FLUIDSYNTH_LIBRARY}")

message(STATUS "sf2_crosscheck enabled (FluidSynth: ${FLUIDSYNTH_LIBRARY})")
