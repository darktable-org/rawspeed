if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "FUZZ"))
  message(WARNING "For fuzzets to function, compiler must be Clang, and build type must be FUZZ.")
  add_library(libFuzzer_dummy STATIC "${CMAKE_CURRENT_SOURCE_DIR}/libFuzzer_dummy_main.cpp")
  target_link_libraries(libFuzzer_dummy rawspeed_static)
  add_library(libFuzzer ALIAS libFuzzer_dummy)

  macro(add_fuzz_target fuzzer)
    add_test(NAME fuzzers/${fuzzer} COMMAND ${fuzzer})
  endmacro()
else()
  add_library(libFuzzer STATIC IMPORTED "${LIBFUZZER_ARCHIVE}")
  set_property(TARGET libFuzzer PROPERTY IMPORTED_LOCATION "${LIBFUZZER_ARCHIVE}")

  macro(add_fuzz_target fuzzer)
    install(TARGETS ${fuzzer} DESTINATION "${CMAKE_INSTALL_BINDIR}")
  endmacro()
endif()

add_custom_target(fuzzers ALL)

add_subdirectory(librawspeed)
add_subdirectory(rawspeed)
