add_executable(RawSpeedFuzzer main.cpp)
target_link_libraries(RawSpeedFuzzer rawspeed)
target_link_libraries(RawSpeedFuzzer libFuzzer)

add_fuzz_target(RawSpeedFuzzer)

add_dependencies(fuzzers RawSpeedFuzzer)
