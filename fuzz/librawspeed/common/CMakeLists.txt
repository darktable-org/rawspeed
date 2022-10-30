macro(add_simple_fuzzer name)
  set(TheFuzzer "${name}Fuzzer")
  rawspeed_add_executable(${TheFuzzer} "${name}.cpp")

  add_fuzz_target(${TheFuzzer})

  add_dependencies(fuzzers ${TheFuzzer})
endmacro()

set(NAMES
  "DngOpcodes"
)

foreach(name ${NAMES})
  add_simple_fuzzer(${name})
endforeach()
