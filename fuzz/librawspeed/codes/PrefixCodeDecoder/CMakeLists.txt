add_custom_target(PrefixCodeDecoderFuzzers ALL)
add_dependencies(fuzzers PrefixCodeDecoderFuzzers)

function(add_ht_solo_fuzzer impl)
  string(REPLACE ":" ";" impl "${impl}")
  list(GET impl 0 frontend)
  list(GET impl -1 backend)

  string(REPLACE ";" "With" implName "${impl}")
  set(fuzzer "PrefixCode${implName}DecoderFuzzer")

  rawspeed_add_executable(${fuzzer} Solo.cpp Common.h)
  target_compile_definitions(${fuzzer}
    PRIVATE
      -DIMPL=PrefixCode${frontend}Decoder
  )

  if(NOT "${frontend}" STREQUAL "${backend}")
    target_compile_definitions(${fuzzer}
      PRIVATE
        -DBACKIMPL=PrefixCode${backend}Decoder
    )
  endif()

  add_fuzz_target(${fuzzer})

  add_dependencies(PrefixCodeDecoderFuzzers ${fuzzer})
endfunction()

function(add_ht_dual_fuzzer impl0 impl1)
  string(REPLACE ":" ";" impl0 "${impl0}")
  string(REPLACE ":" ";" impl1 "${impl1}")

  list(GET impl0 0 frontend0)
  list(GET impl1 0 frontend1)
  list(GET impl0 -1 backend0)
  list(GET impl1 -1 backend1)

  string(REPLACE ";" "With" impl0Name "${impl0}")
  string(REPLACE ";" "With" impl1Name "${impl1}")

  set(fuzzer "PrefixCodeDecoderFuzzer-${impl0Name}Vs${impl1Name}")

  rawspeed_add_executable(${fuzzer} Dual.cpp Common.h)
  target_compile_definitions(${fuzzer}
    PRIVATE
    -DIMPL0=PrefixCode${frontend0}Decoder
    -DIMPL1=PrefixCode${frontend1}Decoder
  )

  if(NOT "${frontend0}" STREQUAL "${backend0}")
    target_compile_definitions(${fuzzer}
      PRIVATE
      -DBACKIMPL0=PrefixCode${backend0}Decoder
    )
  endif()

  if(NOT "${frontend1}" STREQUAL "${backend1}")
    target_compile_definitions(${fuzzer}
      PRIVATE
      -DBACKIMPL1=PrefixCode${backend1}Decoder
    )
  endif()

  add_fuzz_target(${fuzzer})

  add_dependencies(PrefixCodeDecoderFuzzers ${fuzzer})
endfunction()

set(FRONTEND "LUT")
set(BACKEND  "Tree" "Vector" "Lookup")

foreach(backend ${BACKEND})
  list(APPEND IMPL "${backend}")
  foreach(frontend ${FRONTEND})
    list(APPEND IMPL "${frontend}:${backend}")
  endforeach()
endforeach()


foreach(impl ${IMPL})
  add_ht_solo_fuzzer(${impl})
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

foreach(impl ${ALL_UNIQUE_IMPL_PAIRS})
  string(REPLACE "|" ";" impl "${impl}")
  list(GET impl 0 impl0)
  list(GET impl 1 impl1)

  add_ht_dual_fuzzer(${impl0} ${impl1})
endforeach()
