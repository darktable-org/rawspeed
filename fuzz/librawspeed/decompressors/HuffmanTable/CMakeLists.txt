add_custom_target(HuffmanTableFuzzers ALL)
add_dependencies(fuzzers HuffmanTableFuzzers)

set(FullDecode true)
set(NoFullDecode false)

function(add_ht_solo_fuzzer impl pump decode)
  set(fuzzer "HuffmanTable${impl}Fuzzer-${pump}-${decode}")

  rawspeed_add_executable(${fuzzer} Solo.cpp)
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

  rawspeed_add_executable(${fuzzer} Dual.cpp)
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

set(IMPL "LUT" "Lookup" "Tree" "Vector")
set(PUMPS "BitPumpMSB" "BitPumpMSB32" "BitPumpJPEG")
set(DECODE "FullDecode" "NoFullDecode")

foreach(impl ${IMPL})
  foreach(pump ${PUMPS})
    foreach(decode ${DECODE})
      add_ht_solo_fuzzer(${impl} ${pump} ${decode})
    endforeach()
  endforeach()
endforeach()

set(ALL_UNIQUE_IMPL_PAIRS)
foreach(impl0 ${IMPL})
  foreach(impl1 ${IMPL})
    if("${impl0}" STREQUAL "${impl1}")
      continue()
    endif()
    set(PAIR ${impl0} ${impl1})
    list(SORT PAIR)
    string(REPLACE ";" "|" PAIR "${PAIR}")
    list(APPEND ALL_UNIQUE_IMPL_PAIRS "${PAIR}")
  endforeach()
endforeach()

list(REMOVE_DUPLICATES ALL_UNIQUE_IMPL_PAIRS)

foreach(pump ${PUMPS})
  foreach(decode ${DECODE})
    foreach(impl ${ALL_UNIQUE_IMPL_PAIRS})
      string(REPLACE "|" ";" impl "${impl}")
      list(GET impl 0 impl0)
      list(GET impl 1 impl1)

      add_ht_dual_fuzzer(${impl0} ${impl1} ${pump} ${decode})
    endforeach()
  endforeach()
endforeach()
