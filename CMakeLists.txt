cmake_minimum_required(VERSION 3.22)

project(AgeOfDistortion VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(X10_BUILD_TESTS   "Build x10_dsp unit tests"                ON)
option(X10_BUILD_PLUGINS "Build JUCE plugin targets (M0 and later)" OFF)
option(X10_SANITIZE      "Enable ASan + UBSan on x10 targets"       OFF)

# JUCE is not fetched yet — no plugin target exists before M0. The version is
# pinned here so that M0 inherits it instead of re-deciding. See plan D1:
# x10_dsp itself must never depend on JUCE.
set(X10_JUCE_TAG "8.0.14" CACHE STRING "JUCE version pin for plugin targets (M0+)")

if(X10_BUILD_PLUGINS)
    message(FATAL_ERROR
        "X10_BUILD_PLUGINS is reserved for M0. No plugin target exists yet; "
        "JUCE ${X10_JUCE_TAG} will be fetched when one is added.")
endif()

add_subdirectory(libs/x10_dsp)

if(X10_BUILD_TESTS)
    enable_testing()
    add_subdirectory(libs/x10_dsp/test)
endif()
