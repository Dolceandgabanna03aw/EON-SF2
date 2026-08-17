add_executable(x10_instrument_tests
    test_region_index.cpp
    support/AllocationGuard.cpp)

target_link_libraries(x10_instrument_tests PRIVATE
    x10::instrument
    x10::strict
    Catch2::Catch2WithMain)

target_include_directories(x10_instrument_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

catch_discover_tests(x10_instrument_tests)
