macro(add_simple_fuzzer name)
  set(TheFuzzer "${name}Fuzzer")
  rawspeed_add_executable(${TheFuzzer} "${name}.cpp")

  add_fuzz_target(${TheFuzzer})

  add_dependencies(fuzzers ${TheFuzzer})
endmacro()

set(DECOMPRESSORS
  "Cr2LJpegDecoder"
  "CrwDecompressor"
  "DummyLJpegDecoder"
  "FujiDecompressor"
  "HasselbladDecompressor"
  "HasselbladLJpegDecoder"
  "KodakDecompressor"
  "LJpegDecoder"
  "LJpegDecompressor"
  "NikonDecompressor"
  "OlympusDecompressor"
  "PanasonicV4Decompressor"
  "PanasonicV5Decompressor"
  "PanasonicV6Decompressor"
  "PanasonicV7Decompressor"
  "PentaxDecompressor"
  "PhaseOneDecompressor"
  "SamsungV0Decompressor"
  "SamsungV1Decompressor"
  "SamsungV2Decompressor"
  "SonyArw1Decompressor"
  "SonyArw2Decompressor"
  "UncompressedDecompressor"
  "VC5Decompressor"
)

foreach(decompressor ${DECOMPRESSORS})
  add_simple_fuzzer(${decompressor})
endforeach()


macro(add_specialized_fuzzer Name PrefixCodeDecoderImpl)
  set(fuzzer "${Name}Fuzzer-${PrefixCodeDecoderImpl}")

  rawspeed_add_executable(${fuzzer} ${Name}.cpp)
  target_compile_definitions(${fuzzer}
    PRIVATE
    -DPrefixCodeDecoderImpl=${PrefixCodeDecoderImpl}
    -DWITH_${PrefixCodeDecoderImpl}
  )

  add_fuzz_target(${fuzzer})

  add_dependencies(PrefixCodeDecoderFuzzers ${fuzzer})
endmacro()

add_specialized_fuzzer("Cr2Decompressor" PrefixCodeDecoder)
add_specialized_fuzzer("Cr2Decompressor" DummyPrefixCodeDecoder)
