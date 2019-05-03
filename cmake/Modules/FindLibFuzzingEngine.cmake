# - Create LibFuzzingEngine imported target.

include(LibFindMacros)

libfind_process(LibFuzzingEngine)

add_library(LibFuzzingEngine INTERFACE IMPORTED)

if(LIB_FUZZING_ENGINE)
  # If LIB_FUZZING_ENGINE is specified, the compile-time flags were passed via CFLAGS / CXXFLAGS already.
  set_property(TARGET LibFuzzingEngine PROPERTY INTERFACE_LINK_LIBRARIES "${LIB_FUZZING_ENGINE}")
else()
  set_property(TARGET LibFuzzingEngine PROPERTY INTERFACE_COMPILE_OPTIONS "-fsanitize=fuzzer-no-link")
  set_property(TARGET LibFuzzingEngine PROPERTY INTERFACE_LINK_LIBRARIES  "-fsanitize=fuzzer")
endif()

set_package_properties(LibFuzzingEngine PROPERTIES
                       TYPE REQUIRED
                       DESCRIPTION "A prebuilt fuzzing engine library (e.g. libFuzzer.a, or -fsanitize=fuzzer) that needs to be linked with all fuzz target"
                       PURPOSE "Used to actually drive the fuzz targets")
