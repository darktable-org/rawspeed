# We are in ${Q}/rawspeed/lnt/CMakeLists.txt
# We were add_subdirectory()'d from LLVM test-suite/CMakeLists.txt
# We need to find the root source directory of RawSpeed.
# We can't just go up, because this directory was likely reached via symlink.

# Resolve symlinks.
get_filename_component(RAWSPEED_LNT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" REALPATH)
# Go into parent directory.
get_filename_component(RAWSPEED_SOURCE_DIR "${RAWSPEED_LNT_SOURCE_DIR}/../" REALPATH)

# Configure some options..

# By default, try to build everything we possibly can.
set(BUILD_TOOLS ON CACHE BOOL "")
set(BUILD_RS_TESTING ON CACHE BOOL "")
set(BUILD_FUZZERS ON CACHE BOOL "")
set(BUILD_BENCHMARKING ON CACHE BOOL "")

# But, if we are only after benchmarking, force-build benchmarking,
# and disable *everything* else.
if(TEST_SUITE_BENCHMARKING_ONLY)
  set(BUILD_BENCHMARKING ON CACHE BOOL "" FORCE)

  set(BUILD_RS_TESTING OFF)
  set(BUILD_FUZZERS OFF)

  # Only build stuff we will *REALLY* need.
  set(EXCLUDE_FROM_ALL EXCLUDE_FROM_ALL)
endif()

if(BUILD_BENCHMARKING)
  # All the interesting benchmarks are sample-based.
  set(RAWSPEED_ENABLE_SAMPLE_BASED_TESTING ON CACHE BOOL "" FORCE)
endif()

# Even though we are not building rawspeed standalone, we won't redistribute
# the built product, therefore we do not care about reproducibility.
set(RAWSPEED_STANDALONE_BUILD ON CACHE BOOL "")

include(CMakeParseArguments)

# Provide special custom wrappers for add_executable() / add_library()
function(rawspeed_add_executable target)
  llvm_test_executable_no_test(${target} ${ARGN})
endfunction()
function(rawspeed_add_library target)
  llvm_test_library(${target} ${ARGN})
endfunction()
function(rawspeed_add_test)
  cmake_parse_arguments(RAWSPEED_ADD_TEST "" "NAME;WORKING_DIRECTORY" "COMMAND;CONFIGURATIONS" ${ARGN})

  if(RAWSPEED_ADD_TEST_UNPARSED_ARGUMENTS)
    message(SEND_ERROR "Incorrect rawspeed_add_test() - unexpected arguments: "
                        "${RAWSPEED_ADD_TEST_UNPARSED_ARGUMENTS}")
  endif()

  if(CONFIGURATIONS)
    message(SEND_ERROR "CONFIGURATIONS param is not supported.")
  endif()

  list(GET RAWSPEED_ADD_TEST_COMMAND 0 RAWSPEED_ADD_TEST_EXECUTABLE)
  list(REMOVE_AT RAWSPEED_ADD_TEST_COMMAND 0)

  if(NOT RAWSPEED_ADD_TEST_WORKING_DIRECTORY)
    set(RAWSPEED_ADD_TEST_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${RAWSPEED_ADD_TEST_EXECUTABLE}>")
  endif()

  if(TEST_SUITE_BENCHMARKING_ONLY AND NOT "${RAWSPEED_ADD_TEST_EXECUTABLE}" STREQUAL "rsbench")
    # Don't add tests if we only want benchmarks. Include rsbench though.
    return()
  endif()

  llvm_test_run(WORKDIR "${RAWSPEED_ADD_TEST_WORKING_DIRECTORY}"
                "${RAWSPEED_ADD_TEST_COMMAND}")
  llvm_add_test_for_target(${RAWSPEED_ADD_TEST_EXECUTABLE})
endfunction()

set(RAWSPEED_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/build")
add_subdirectory("${RAWSPEED_SOURCE_DIR}" "${RAWSPEED_BINARY_DIR}" ${EXCLUDE_FROM_ALL})

# Collect compile/link time, compiler stats globally.
# We have to do this here, once, at the top-level, because normally we'd do
# this per test that was run, and that would be overly optimistic since it would
# only look in the test's dir and lower. Which is not correct for us because of
# all this nice tree dir structure.
# Oh, also we need to at least try to produce different exe every time,
# so that LNT will always consider this test as changed..
rawspeed_add_executable(RawSpeed "${RAWSPEED_LNT_SOURCE_DIR}/RawSpeed.cpp")
add_custom_target(RawSpeed-touch
  COMMAND "${CMAKE_COMMAND}" -E touch "${RAWSPEED_LNT_SOURCE_DIR}/RawSpeed.cpp"
  COMMENT "Touching RawSpeed.cpp" VERBATIM USES_TERMINAL)
add_dependencies(RawSpeed RawSpeed-touch)
llvm_test_run(WORKDIR "${CMAKE_CURRENT_BINARY_DIR}")
llvm_add_test_for_target(RawSpeed)

if(RAWSPEED_ENABLE_SAMPLE_BASED_TESTING)
  add_subdirectory(raw-sample-archive)
endif()

if(TEST_SUITE_BENCHMARKING_ONLY)
  # Since we passed EXCLUDE_FROM_ALL, we now need to make sure that the required
  # components are still reached from 'all' target. Presently, all benchmarks
  # are only sample-based, thus we only need rsbench.
  add_custom_target(rawspeed-benchmarking-only ALL)
  add_dependencies(rawspeed-benchmarking-only rsbench)
endif()

# test-suite instruments compile command with a wrapper to measure compiler
# perf. Normally, llvm_test_executable_no_test() is enough. But we have external
# deps that we build in-tree. So we have to manually add these deps for them.
# FIXME: this could break if those deps themselves depend on something else,
# since we won't add deps to that something else...
test_suite_add_build_dependencies(dependencies)
if(TARGET benchmark)
  test_suite_add_build_dependencies(benchmark)
endif()
if(BUILD_RS_TESTING)
  test_suite_add_build_dependencies(gtest)
  test_suite_add_build_dependencies(gtest_main)
  test_suite_add_build_dependencies(gmock)
  test_suite_add_build_dependencies(gmock_main)
endif()

file(COPY lit.local.cfg DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY litsupport_rawspeed DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

file(COPY build-lit.local.cfg DESTINATION "${RAWSPEED_BINARY_DIR}")
file(RENAME "${RAWSPEED_BINARY_DIR}/build-lit.local.cfg" "${RAWSPEED_BINARY_DIR}/lit.local.cfg")
