if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 3)
    message(FATAL_ERROR "Clang version must be 3.0.0 or greater! Aborting...")
  endif()
elseif(NOT CMAKE_COMPILER_IS_GNUCXX)
  message(FATAL_ERROR "Compiler is not GNU gcc! Aborting...")
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "COVERAGE")
  message(WARNING "Wrong build type, need COVERAGE.")
endif()

find_package(GenHtml REQUIRED)

add_custom_target(
  genhtml
  COMMAND "${GENHTML_PATH}"
    --demangle-cpp --precision 2
    -o "${CMAKE_BINARY_DIR}/coverage/"
    --prefix "${CMAKE_SOURCE_DIR}"
    lcov.final.info
  COMMAND "${CMAKE_COMMAND}" -E echo "Use $$ sensible-browser \"./coverage/index.html\" to view the coverage report."
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Running genhtml tool to generate HTML coverage report"
  USES_TERMINAL
)

add_custom_target(
  genhtml-clean
  COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_BINARY_DIR}/coverage"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Removing HTML coverage report"
)
