juce_add_plugin(AgeOfDistortion
    PLUGIN_MANUFACTURER_CODE    Sngh
    PLUGIN_CODE                 Aod1
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "Age of Distortion"
    DESCRIPTION                 "Polyphonic SF2 rompler with per-voice nonlinear drive"
    COMPANY_NAME                "Sisyphus Audio"
    BUNDLE_ID                   com.sisyphus.ageofdistortion
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    AU_SANDBOX_SAFE             TRUE)

target_sources(AgeOfDistortion PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Sampler.cpp
    Source/SF2Loader.cpp
    Source/BusProcessor.cpp)
target_include_directories(AgeOfDistortion PRIVATE Source)

target_compile_definitions(AgeOfDistortion PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# NOTE: JUCE_DISPLAY_SPLASH_SCREEN is deliberately left at its default. Turning
# it off requires a paid JUCE licence, and the licence tier for this project is
# still unresolved in the planning document. Setting it now would be asserting a
# licence we may not hold.

target_link_libraries(AgeOfDistortion
    PRIVATE
        x10::dsp
        x10::sf2
        x10::instrument
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Our own sources are held to the same bar as x10_dsp, minus the numeric
# warnings that JUCE's own headers do not compile cleanly under.
target_compile_options(AgeOfDistortion PRIVATE -Wall -Wextra -Wpedantic -Werror -Wshadow)

# --------------------------------------------------------------------------
# Ad-hoc signing
#
# macOS refuses to load an unsigned component, and only an "Apple Development"
# identity is available on this machine — there is no "Developer ID Application"
# certificate, so notarised distribution signing is not possible yet and is
# deferred to M6. Ad-hoc signing is enough for local hosts and for pluginval.
# --------------------------------------------------------------------------
#
# JUCE_PLUGIN_ARTEFACT_FILE holds a generator expression as its *value*, so
# reading it yields another unevaluated genex. It has to be run through
# GENEX_EVAL or codesign receives the literal "$<TARGET_BUNDLE_DIR:...>".
#
# This step must come last. JUCE runs its own signature check and then writes
# moduleinfo.json into the VST3 bundle, which invalidates whatever signature was
# there; signing afterwards is what makes the bundle actually loadable.
if(APPLE)
    foreach(format VST3 AU Standalone)
        if(TARGET AgeOfDistortion_${format})
            add_custom_command(TARGET AgeOfDistortion_${format} POST_BUILD
                COMMAND codesign --force --sign - --timestamp=none
                        "$<GENEX_EVAL:$<TARGET_PROPERTY:AgeOfDistortion_${format},JUCE_PLUGIN_ARTEFACT_FILE>>"
                COMMENT "Ad-hoc signing ${format}"
                VERBATIM)
        endif()
    endforeach()
endif()

# --------------------------------------------------------------------------
# pluginval — Gate 3
# --------------------------------------------------------------------------
find_program(PLUGINVAL_EXECUTABLE
    NAMES pluginval
    HINTS "/Applications/pluginval.app/Contents/MacOS"
          "$ENV{HOME}/Applications/pluginval.app/Contents/MacOS")

if(PLUGINVAL_EXECUTABLE)
    add_test(NAME pluginval_vst3_strictness10
        COMMAND "${PLUGINVAL_EXECUTABLE}"
                --strictness-level 10
                --timeout-ms 300000
                # Same GENEX_EVAL requirement as the signing step above: the
                # property's value is itself a generator expression.
                --validate "$<GENEX_EVAL:$<TARGET_PROPERTY:AgeOfDistortion_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
    set_tests_properties(pluginval_vst3_strictness10 PROPERTIES TIMEOUT 600)
else()
    message(WARNING "pluginval not found — Gate 3 will not run. "
                    "Install it to /Applications for the M0 acceptance test.")
endif()

if(X10_BUILD_TESTS)
    add_subdirectory(test)
endif()
