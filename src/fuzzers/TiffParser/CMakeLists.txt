add_executable(TiffParserFuzzer main.cpp)
set_target_properties(TiffParserFuzzer PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(TiffParserFuzzer rawspeed_static)
target_link_libraries(TiffParserFuzzer libFuzzer)
target_include_directories(TiffParserFuzzer PUBLIC "${CONFIG_INCLUDE_PATH}")

if(TARGET libFuzzer_dummy)
  add_test(NAME fuzzers/TiffParserFuzzer COMMAND TiffParserFuzzer)
endif()

add_dependencies(fuzzers TiffParserFuzzer)
