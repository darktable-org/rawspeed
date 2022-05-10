if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  if("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 3)
    message(FATAL_ERROR "Clang version must be 3.0.0 or greater! Aborting...")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
elseif(NOT CMAKE_COMPILER_IS_GNUCXX)
  message(FATAL_ERROR "Compiler is not GNU gcc! Aborting...")
endif()

if(NOT RAWSPEED_COVERAGE_BUILD)
  message(WARNING "Wrong build type, need COVERAGE.")
endif()

find_package(GCov REQUIRED)
find_package(Find REQUIRED)

set(GCOV_OPTS "-pb")

if(NOT APPLE)
  # DON'T elide source prefix.
  set(GCOV_OPTS ${GCOV_OPTS} -aflu)
endif()

# Find all *.gcno files and run gcov on them, but ignore stdout of gcov.
# While you'd normally just "> /dev/null", there are edge cases where that does not work.
file(WRITE "${CMAKE_BINARY_DIR}/run-gcov-wrapper.cmake"
"execute_process(
  COMMAND \"${FIND_PATH}\" \"${CMAKE_BINARY_DIR}\" -type f -name *.gcno -exec \"${GCOV_PATH}\" ${GCOV_OPTS} {} +
  WORKING_DIRECTORY \"${CMAKE_BINARY_DIR}\"
  OUTPUT_QUIET
  COMMAND_ECHO STDOUT
)
")
add_custom_target(
  gcov
  COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/run-gcov-wrapper.cmake"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Running gcov tool on all the *.gcno files to produce *.gcov files"
)

# DON'T remove *.gcno/*.gcov files here!
add_custom_target(
  gcov-clean
  COMMAND "${FIND_PATH}" "${CMAKE_BINARY_DIR}" -type f -name '*.gcda' -delete > /dev/null
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Removing all the *.gcda files"
)
