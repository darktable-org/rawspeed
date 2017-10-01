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

find_package(GCov REQUIRED)
find_package(LCov REQUIRED)

add_custom_target(
  lcov-baseline
  COMMAND "${LCOV_PATH}" --capture --initial --gcov-tool "${GCOV_PATH}"
    --directory "${CMAKE_BINARY_DIR}"
    --output-file lcov.baseline.info
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Capturnig initial zero coverage info"
  USES_TERMINAL
)

add_custom_target(
  lcov-capture
  COMMAND "${LCOV_PATH}" --capture --gcov-tool "${GCOV_PATH}"
    --directory "${CMAKE_BINARY_DIR}"
    --output-file lcov.coverage.info
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Capturing the actual code coverage info"
  USES_TERMINAL
)

add_custom_target(
  lcov-combine
  COMMAND "${LCOV_PATH}"
    --add-tracefile lcov.baseline.info --add-tracefile lcov.coverage.info
    --output-file lcov.total.info
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Combining initial zero coverage info and actual code coverage info"
  USES_TERMINAL
)

add_custom_target(
  lcov-postprocess
  COMMAND "${LCOV_PATH}" --extract lcov.total.info '${CMAKE_SOURCE_DIR}/*' --output-file lcov.cleaned.info
  COMMAND "${LCOV_PATH}" --remove lcov.cleaned.info '${CMAKE_BINARY_DIR}/*' --output-file lcov.final.info
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Cleaning the final code coverage info"
  USES_TERMINAL
)

add_custom_target(
  lcov-clean
  COMMAND "${LCOV_PATH}" --directory "${CMAKE_BINARY_DIR}" --zerocounters > /dev/null
  COMMAND "${CMAKE_COMMAND}" -E remove lcov.baseline.info lcov.coverage.info lcov.total.info lcov.cleaned.info lcov.final.info
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Removing all the *.gcda files"
)
