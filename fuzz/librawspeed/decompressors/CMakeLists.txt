macro(add_simple_fuzzer name)
  set(TheFuzzer "${name}Fuzzer")
  add_executable(${TheFuzzer} "${name}.cpp")
  target_link_libraries(${TheFuzzer} rawspeed)
  target_link_libraries(${TheFuzzer} libFuzzer)

  add_fuzz_target(${TheFuzzer})

  add_dependencies(fuzzers ${TheFuzzer})
endmacro()

add_subdirectory(HuffmanTable)

set(DECOMPRESSORS
  "SonyArw1Decompressor"
)

foreach(decompressor IN LISTS DECOMPRESSORS)
  add_simple_fuzzer(${decompressor})
endforeach()
