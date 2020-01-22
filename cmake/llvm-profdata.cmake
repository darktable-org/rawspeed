if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "Compiler is not clang! Aborting...")
endif()

if(NOT RAWSPEED_COVERAGE_BUILD)
  message(WARNING "Wrong build type, need COVERAGE.")
endif()

find_package(LLVMProfData REQUIRED)
find_package(Find REQUIRED)

add_custom_target(
  profdata
  COMMAND "${FIND_PATH}" "${CMAKE_BINARY_DIR}" -type f -name '*.profraw' -exec "${LLVMPROFDATA_PATH}" merge -o "${RAWSPEED_PROFDATA_FILE}" {} + > /dev/null
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Running llvm-profdata tool on all the *.profraw files"
)

add_custom_target(
  profdata-clean
  COMMAND "${FIND_PATH}" "${CMAKE_BINARY_DIR}" -type f -name '*.profdata' -delete > /dev/null
  COMMAND "${FIND_PATH}" "${CMAKE_BINARY_DIR}" -type f -name '*.profraw'  -delete > /dev/null
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Removing all the *.profdata and *.profraw files"
)
