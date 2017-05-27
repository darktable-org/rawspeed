add_custom_target(TiffBasedDecoderFuzzers ALL)
add_dependencies(fuzzers TiffBasedDecoderFuzzers)

function(add_decoder {decoder})
  set(fuzzer "TiffDecoderFuzzer-${decoder}")

  add_executable(${fuzzer} main.cpp)
  target_compile_definitions(${fuzzer}
    PRIVATE
      -DDECODER=${decoder}
  )

  set_target_properties(${fuzzer} PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(${fuzzer} rawspeed_static)
  target_link_libraries(${fuzzer} libFuzzer)
  target_include_directories(${fuzzer} PUBLIC "${CONFIG_INCLUDE_PATH}")

  if(TARGET libFuzzer_dummy)
    add_test(NAME fuzzers/${fuzzer} COMMAND ${fuzzer})
  endif()

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
  "KdcDecoder"
  "MefDecoder"
  "MosDecoder"
  "NefDecoder"
  "OrfDecoder"
  "PefDecoder"
  "RafDecoder"
  "Rw2Decoder"
  "SrwDecoder"
  "ThreefrDecoder"
)

foreach(decoder IN LISTS DECODERS)
  add_decoder(${decoder})
endforeach()
