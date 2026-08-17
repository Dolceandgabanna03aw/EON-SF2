add_executable(rompler_tests test_plugin_smoke.cpp)

# AgeOfDistortion carries the JucePlugin_* definitions PUBLIC, but JUCE modules
# are linked PRIVATE on the shared-code target by convention, so their include
# directories do not propagate. The test needs the module explicitly.
target_link_libraries(rompler_tests PRIVATE
    AgeOfDistortion
    juce::juce_audio_utils
    Catch2::Catch2)

target_include_directories(rompler_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../Source")

target_compile_options(rompler_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -Wshadow)

catch_discover_tests(rompler_tests)
