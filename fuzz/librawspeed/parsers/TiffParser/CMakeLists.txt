add_custom_target(TiffParserFuzzers ALL)
add_dependencies(fuzzers TiffParserFuzzers)

function(add_tp_fuzzer variant boolvar)
  set(fuzzer "TiffParserFuzzer${variant}")
  add_executable(${fuzzer} main.cpp)
  target_compile_definitions(${fuzzer}
    PRIVATE
      -DGETDECODER=${boolvar}
  )

  set_target_properties(${fuzzer} PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(${fuzzer} rawspeed_static)
  target_link_libraries(${fuzzer} libFuzzer)
  target_include_directories(${fuzzer} PUBLIC "${CONFIG_INCLUDE_PATH}")

  if(TARGET libFuzzer_dummy)
    add_test(NAME fuzzers/${fuzzer} COMMAND ${fuzzer})
  endif()

  add_dependencies(TiffParserFuzzers ${fuzzer})
endfunction()

add_tp_fuzzer("" false)
add_tp_fuzzer("-GetDecoder" true)
