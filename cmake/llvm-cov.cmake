if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  message(FATAL_ERROR "Compiler is not clang! Aborting...")
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "COVERAGE")
  message(WARNING "Wrong build type, need COVERAGE.")
endif()

find_package(LLVMCov REQUIRED)
find_package(Demangler REQUIRED)

# FIXME: all this does not really work because only the rstest coverage is shown

add_custom_target(
  coverage-show
  DEPENDS rstest
  COMMAND "${LLVMCOV_PATH}" show -Xdemangler=$<TARGET_FILE:demangler> -instr-profile "${RAWSPEED_PROFDATA_FILE}" "$<TARGET_FILE:rstest>" -format html -output-dir "${CMAKE_BINARY_DIR}/coverage"
  COMMAND "${CMAKE_COMMAND}" -E echo "Use $$ sensible-browser \"./coverage/index.html\" to view the coverage report."
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Running llvm-cov tool on the *.profdata file to generate HTML coverage report"
  VERBATIM
)

add_custom_target(
  coverage-clean
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_BINARY_DIR}/coverage"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Removing HTML coverage report"
)

add_custom_target(
  coverage
  DEPENDS tests rstest
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target coverage-clean
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target profdata-clean
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target test
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target profdata
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target coverage-show
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Doing everything to generate clean fresh HTML coverage report"
  USES_TERMINAL
)
