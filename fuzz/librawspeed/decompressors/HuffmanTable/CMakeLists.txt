add_custom_target(HuffmanTableFuzzers ALL)
add_dependencies(fuzzers HuffmanTableFuzzers)

set(FullDecode true)
set(NoFullDecode false)

function(add_ht_solo_fuzzer impl pump decode)
  set(fuzzer "HuffmanTable${impl}Fuzzer-${pump}-${decode}")

  add_executable(${fuzzer} Solo.cpp)
  target_compile_definitions(${fuzzer}
    PRIVATE
      -DIMPL=HuffmanTable${impl}
      -DPUMP=${pump}
      -DFULLDECODE=${${decode}}
  )

  add_fuzz_target(${fuzzer})

  add_dependencies(HuffmanTableFuzzers ${fuzzer})
endfunction()

function(add_ht_dual_fuzzer impl0 impl1 pump decode)
  set(fuzzer "HuffmanTableFuzzer-${impl0}Vs${impl1}-${pump}-${decode}")

  add_executable(${fuzzer} Dual.cpp)
  target_compile_definitions(${fuzzer}
    PRIVATE
    -DIMPL0=HuffmanTable${impl0}
    -DIMPL1=HuffmanTable${impl1}
      -DPUMP=${pump}
      -DFULLDECODE=${${decode}}
  )

  add_fuzz_target(${fuzzer})

  add_dependencies(HuffmanTableFuzzers ${fuzzer})
endfunction()

set(IMPL "LUT" "Tree" "Vector")
set(PUMPS "BitPumpLSB" "BitPumpMSB" "BitPumpMSB16" "BitPumpMSB32" "BitPumpJPEG")
set(DECODE "FullDecode" "NoFullDecode")

foreach(impl IN LISTS IMPL)
  foreach(pump IN LISTS PUMPS)
    foreach(decode IN LISTS DECODE)
      add_ht_solo_fuzzer(${impl} ${pump} ${decode})
    endforeach()
  endforeach()
endforeach()

foreach(pump IN LISTS PUMPS)
  foreach(decode IN LISTS DECODE)
    add_ht_dual_fuzzer(LUT Tree ${pump} ${decode})
    add_ht_dual_fuzzer(LUT Vector ${pump} ${decode})
    add_ht_dual_fuzzer(Tree Vector ${pump} ${decode})
  endforeach()
endforeach()
