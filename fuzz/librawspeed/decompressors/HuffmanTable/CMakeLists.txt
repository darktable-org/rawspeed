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

  add_fuzz_target(${fuzzer})

  add_dependencies(HuffmanTableFuzzers ${fuzzer})
endfunction()

set(PUMPS "BitPumpLSB" "BitPumpMSB" "BitPumpMSB16" "BitPumpMSB32" "BitPumpJPEG")
set(DECODE "FullDecode" "NoFullDecode")

foreach(pump IN LISTS PUMPS)
  foreach(decode IN LISTS DECODE)
    add_ht_fuzzer(${pump} ${decode})
  endforeach()
endforeach()
