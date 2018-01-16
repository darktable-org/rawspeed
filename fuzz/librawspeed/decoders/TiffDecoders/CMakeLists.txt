add_custom_target(TiffBasedDecoderFuzzers ALL)
add_dependencies(fuzzers TiffBasedDecoderFuzzers)

function(add_decoder {decoder})
  set(fuzzer "TiffDecoderFuzzer-${decoder}")

  add_executable(${fuzzer} main.cpp)
  target_compile_definitions(${fuzzer}
    PRIVATE
      -DDECODER=${decoder}
  )

  add_fuzz_target(${fuzzer})

  add_dependencies(TiffBasedDecoderFuzzers ${fuzzer})
endfunction()

# see TiffParser::Map[]
set(DECODERS
  "ArwDecoder"
  "Cr2Decoder"
  "DcrDecoder"
  "DcsDecoder"
  "DngDecoder"
  "ErfDecoder"
  "IiqDecoder"
  "KdcDecoder"
  "MefDecoder"
  "MosDecoder"
  "NefDecoder"
  "OrfDecoder"
  "PefDecoder"
  "Rw2Decoder"
  "SrwDecoder"
  "ThreefrDecoder"
)

foreach(decoder IN LISTS DECODERS)
  add_decoder(${decoder})
endforeach()
