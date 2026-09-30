add_executable(rompler_tests test_plugin_smoke.cpp test_sf2_playback.cpp test_dsp_behaviors.cpp)

# EONDS50 carries the JucePlugin_* definitions PUBLIC, but JUCE modules
# are linked PRIVATE on the shared-code target by convention, so their include
# directories do not propagate. The test needs the module explicitly.
target_link_libraries(rompler_tests PRIVATE
    EONDS50
    juce::juce_audio_utils
    juce::juce_dsp
    x10::dsp
    x10::sf2
    x10::instrument
    Catch2::Catch2)

target_include_directories(rompler_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Source"
    "${CMAKE_SOURCE_DIR}/libs/sf2/test/support")

target_compile_options(rompler_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wshadow)

# Points at the local, untracked SF2 corpus (see the top-level .gitignore).
# The cache entry lets isolated worktrees reuse a corpus without copying it.
set(X10_SF2_TESTDATA_DIR "${CMAKE_SOURCE_DIR}/testdata/sf2" CACHE PATH
    "Directory containing the optional SF2 test corpus")
target_compile_definitions(rompler_tests PRIVATE
    X10_SF2_CROSSCHECK_TESTDATA="${X10_SF2_TESTDATA_DIR}")

catch_discover_tests(rompler_tests)
