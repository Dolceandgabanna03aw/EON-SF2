add_executable(x10_sf2_tests test_sf2_reader.cpp)

target_link_libraries(x10_sf2_tests PRIVATE
    x10::sf2
    x10::strict
    Catch2::Catch2WithMain)

target_include_directories(x10_sf2_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

catch_discover_tests(x10_sf2_tests)
