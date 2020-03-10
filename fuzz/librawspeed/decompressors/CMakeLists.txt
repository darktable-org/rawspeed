macro(add_simple_fuzzer name)
  set(TheFuzzer "${name}Fuzzer")
  rawspeed_add_executable(${TheFuzzer} "${name}.cpp")

  add_fuzz_target(${TheFuzzer})

  add_dependencies(fuzzers ${TheFuzzer})
endmacro()

add_subdirectory(HuffmanTable)

set(DECOMPRESSORS
  "Cr2Decompressor"
  "CrwDecompressor"
  "DummyLJpegDecompressor"
  "FujiDecompressor"
  "HasselbladDecompressor"
  "KodakDecompressor"
  "LJpegDecompressor"
  "NikonDecompressor"
  "OlympusDecompressor"
  "PanasonicDecompressor"
  "PanasonicDecompressorV5"
  "PanasonicDecompressorV6"
  "PentaxDecompressor"
  "PhaseOneDecompressor"
  "SamsungV0Decompressor"
  "SamsungV1Decompressor"
  "SamsungV2Decompressor"
  "SonyArw1Decompressor"
  "SonyArw2Decompressor"
  "VC5Decompressor"
)

foreach(decompressor IN LISTS DECOMPRESSORS)
  add_simple_fuzzer(${decompressor})
endforeach()
