add_executable(RawSpeedFuzzer main.cpp)
set_target_properties(RawSpeedFuzzer PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(RawSpeedFuzzer rawspeed_static)
target_link_libraries(RawSpeedFuzzer libFuzzer)
target_include_directories(RawSpeedFuzzer PUBLIC "${CONFIG_INCLUDE_PATH}")

if(TARGET libFuzzer_dummy)
  add_test(NAME fuzzers/RawSpeedFuzzer COMMAND RawSpeedFuzzer)
endif()

add_dependencies(fuzzers RawSpeedFuzzer)
