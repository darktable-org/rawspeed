add_custom_target(ParserFuzzers ALL)
add_dependencies(fuzzers ParserFuzzers)

function(add_parser_fuzzer parser variant boolvar boolvar2)
  set(fuzzer "${parser}Fuzzer${variant}")
  rawspeed_add_executable(${fuzzer} main.cpp)
  target_compile_definitions(${fuzzer}
    PRIVATE
      -DPARSER=${parser}
      -DGETDECODER=${boolvar}
      -DDECODE=${boolvar2}
  )

  add_fuzz_target(${fuzzer})

  add_dependencies(ParserFuzzers ${fuzzer})
endfunction()

set(PARSERS
  "Ciff"
  "Fiff"
  "Raw"
  "Tiff"
)

foreach(parser IN LISTS PARSERS)
  set(parser "${parser}Parser")
  # add_parser_fuzzer(${parser} "" false false)
  add_parser_fuzzer(${parser} "-GetDecoder" true false)
  add_parser_fuzzer(${parser} "-GetDecoder-Decode" true true)
endforeach()
