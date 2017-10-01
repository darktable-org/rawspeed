if("${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
  if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 3)
    message(FATAL_ERROR "Clang version must be 3.0.0 or greater! Aborting...")
  endif()
elseif(NOT CMAKE_COMPILER_IS_GNUCXX)
  message(FATAL_ERROR "Compiler is not GNU gcc! Aborting...")
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "COVERAGE")
  message(WARNING "Wrong build type, need COVERAGE.")
endif()

add_custom_target(
  coverage-clean
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target genhtml-clean
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target lcov-clean
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target gcov-clean
)

add_custom_target(
  coverage
  DEPENDS tests
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target coverage-clean
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target lcov-baseline
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target test
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target lcov-capture
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target lcov-combine
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target lcov-postprocess
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target genhtml
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Doing everything to generate clean fresh HTML coverage report"
  USES_TERMINAL
)
