if(NOT BUILD_FUZZERS)
  return()
endif()

if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "FUZZ"))
  message(WARNING "For fuzzets to function, compiler must be Clang, and build type must be FUZZ.")
  add_library(libFuzzer_dummy STATIC "${CMAKE_CURRENT_SOURCE_DIR}/libFuzzer_dummy_main.cpp")
  add_library(libFuzzer ALIAS libFuzzer_dummy)
else()
  add_library(libFuzzer STATIC IMPORTED "${LIBFUZZER_ARCHIVE}")
  set_property(TARGET libFuzzer PROPERTY IMPORTED_LOCATION "${LIBFUZZER_ARCHIVE}")
endif()

add_custom_target(fuzzers ALL)

add_subdirectory(HuffmanTable)
add_subdirectory(TiffParser)
add_subdirectory(rawspeed)
