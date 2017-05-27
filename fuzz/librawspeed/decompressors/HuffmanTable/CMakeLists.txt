add_custom_target(HuffmanTableFuzzers ALL)
add_dependencies(fuzzers HuffmanTableFuzzers)

set(FullDecode true)
set(NoFullDecode false)

function(add_ht_fuzzer pump decode)
  set(fuzzer "HuffmanTableFuzzer-${pump}-${decode}")

  add_executable(${fuzzer} main.cpp)
  target_compile_definitions(${fuzzer}
    PRIVATE
      -DPUMP=${pump}
      -DFULLDECODE=${${decode}}
  )

  set_target_properties(${fuzzer} PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(${fuzzer} rawspeed_static)
  target_link_libraries(${fuzzer} libFuzzer)
  target_include_directories(${fuzzer} PUBLIC "${CONFIG_INCLUDE_PATH}")

  if(TARGET libFuzzer_dummy)
    add_test(NAME fuzzers/${fuzzer} COMMAND ${fuzzer})
  endif()

  add_dependencies(HuffmanTableFuzzers ${fuzzer})
endfunction()

set(PUMPS "BitPumpLSB" "BitPumpMSB" "BitPumpMSB16" "BitPumpMSB32" "BitPumpJPEG")
set(DECODE "FullDecode" "NoFullDecode")

foreach(pump IN LISTS PUMPS)
  foreach(decode IN LISTS DECODE)
    add_ht_fuzzer(${pump} ${decode})
  endforeach()
endforeach()
