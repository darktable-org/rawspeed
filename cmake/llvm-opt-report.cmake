if(NOT USE_LLVM_OPT_REPORT OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  return()
endif()

include(CheckCXXCompilerFlag)

find_package(LLVMOptViewer REQUIRED)

message(STATUS "Checking for -fsave-optimization-record support")
CHECK_CXX_COMPILER_FLAG("-fsave-optimization-record" HAVE_CXX_FLAG_FSAVE_OPTIMIZATION_RECORD)
if(NOT HAVE_CXX_FLAG_FSAVE_OPTIMIZATION_RECORD)
  message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} does not support saving optimization records.")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsave-optimization-record")
endif()

set(REPORT_PATH "${CMAKE_CURRENT_BINARY_DIR}/opt-report")

add_custom_target(opt-report
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${REPORT_PATH}"
  COMMAND "${LLVMOPTVIEWER_PATH}"
    --output-dir "${REPORT_PATH}"
    -source-dir "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E echo "Use $$ sensible-browser \"${REPORT_PATH}\" to view the optimizatons report."
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Generating HTML output to visualize optimization records from the YAML files"
  VERBATIM
  USES_TERMINAL)
