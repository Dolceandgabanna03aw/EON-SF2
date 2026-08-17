add_executable(x10_dsp_tests
    test_adaa_correctness.cpp
    test_adaa_conditioning.cpp
    test_curves.cpp
    test_reset.cpp
    test_filters.cpp
    test_gate_selfcheck.cpp
)

target_link_libraries(x10_dsp_tests PRIVATE
    x10::dsp
    x10::strict
    Catch2::Catch2WithMain)

target_include_directories(x10_dsp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

catch_discover_tests(x10_dsp_tests)
