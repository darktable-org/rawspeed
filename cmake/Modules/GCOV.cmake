if("${CMAKE_CXX_COMPILER_ID}" MATCHES "(Apple)?[Cc]lang")
  if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 3)
    message(FATAL_ERROR "Clang version must be 3.0.0 or greater! Aborting...")
  endif()
elseif(NOT CMAKE_COMPILER_IS_GNUCXX)
  message(FATAL_ERROR "Compiler is not GNU gcc! Aborting...")
endif()

if(NOT (CMAKE_BUILD_TYPE STREQUAL "DEBUG" OR CMAKE_BUILD_TYPE STREQUAL "COVERAGE"))
  message(WARNING "Code coverage results with an optimized (non-Debug) build may be misleading")
endif()

find_package(GCov)

set(GCOV_OPTS "-pb")

if(NOT APPLE)
  # DONT elide source prefix.
  set(GCOV_OPTS ${GCOV_OPTS} -aflu)
endif()

add_custom_target(
  gcov
  COMMAND find "${CMAKE_BINARY_DIR}" -type f -name '*.gcno' -exec "${GCOV_PATH}" ${GCOV_OPTS} {} + > /dev/null
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Running gcov tool on all the *.gcno files"
)
