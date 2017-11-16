add_executable(RawSpeedFuzzer main.cpp)

add_fuzz_target(RawSpeedFuzzer)

add_dependencies(fuzzers RawSpeedFuzzer)
