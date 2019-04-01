file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/all-fuzzers.txt" ALL_EXPECTED_FUZZERS)
list(SORT ALL_EXPECTED_FUZZERS)

set(ALL_FUZZERS "" CACHE INTERNAL "" FORCE)

if(CMAKE_BUILD_TYPE STREQUAL "COVERAGE")
  # want all the symbols.
  rawspeed_add_library(rawspeed_fuzz STATIC) # SHARED, but crashes due to global constructors/destructors
else()
  rawspeed_add_library(rawspeed_fuzz STATIC)
endif()

target_link_libraries(rawspeed_fuzz PUBLIC rawspeed)

macro(add_fuzz_target__base fuzzer)
  target_link_libraries(${fuzzer} PUBLIC rawspeed)
  target_link_libraries(${fuzzer} PUBLIC rawspeed_fuzz)

  list(APPEND ALL_FUZZERS "${fuzzer}")
  set(ALL_FUZZERS "${ALL_FUZZERS}" CACHE INTERNAL "" FORCE)

  add_test(NAME fuzzers/${fuzzer} COMMAND ${fuzzer} -help=1)
endmacro()

if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_BUILD_TYPE STREQUAL "FUZZ"))
  message(WARNING "For fuzzets to function, compiler must be Clang, and build type must be FUZZ.")

  target_sources(rawspeed_fuzz PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/libFuzzer_dummy_main.cpp"
  )

  if(TARGET RawSpeed::OpenMP_CXX)
    target_link_libraries(rawspeed_fuzz PRIVATE RawSpeed::OpenMP_CXX)
  endif()

  macro(add_fuzz_target fuzzer)
    add_fuzz_target__base(${fuzzer})
  endmacro()
else()
  if(NOT LIB_FUZZING_ENGINE)
    # specialhandling: oss-fuzz provides all the needed flags already.
    target_compile_options(rawspeed_fuzz INTERFACE "-fsanitize=fuzzer")
    target_link_libraries (rawspeed_fuzz INTERFACE "-fsanitize=fuzzer")
  else()
    message(STATUS "LIB_FUZZING_ENGINE override option is passed, not setting special compiler flags.")
    find_package(LibFuzzingEngine REQUIRED)
    target_link_libraries(rawspeed_fuzz PUBLIC LibFuzzingEngine)
  endif()

  add_feature_info("Fuzzing" ON "will be actually able to do the fuzzing")

  file(GLOB FUZZ_DICT "common.dict")

  macro(add_fuzz_target fuzzer)
    add_fuzz_target__base(${fuzzer})

    install(TARGETS ${fuzzer} DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${FUZZ_DICT}" DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME "${fuzzer}.dict")
  endmacro()
endif()

add_custom_target(fuzzers ALL)

add_subdirectory(librawspeed)
add_subdirectory(rawspeed)

add_subdirectory(corpora)

list(SORT ALL_FUZZERS)
if(NOT "${ALL_FUZZERS}" STREQUAL "${ALL_EXPECTED_FUZZERS}")
  message(SEND_ERROR "Actual list of fuzzers does not match the expected list: "
    "${ALL_FUZZERS} vs ${ALL_EXPECTED_FUZZERS}")
endif()
