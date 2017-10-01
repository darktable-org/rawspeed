if(NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
  message(FATAL_ERROR "Compiler is not clang! Aborting...")
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "COVERAGE")
  message(WARNING "Wrong build type, need COVERAGE.")
endif()

find_package(LLVMProfData REQUIRED)
find_package(Find REQUIRED)

add_custom_target(
  profdata
  COMMAND "${FIND_PATH}" "${CMAKE_BINARY_DIR}" -type f -name '*.profraw' -exec "${LLVMPROFDATA_PATH}" merge -o "${CMAKE_BINARY_DIR}/rawspeed.profdata" {} + > /dev/null
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
