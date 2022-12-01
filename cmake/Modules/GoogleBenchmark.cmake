# Download and unpack googlebenchmark at configure time
set(GOOGLEBENCHMARK_PREFIX "${RAWSPEED_BINARY_DIR}/src/external/googlebenchmark")
configure_file(${RAWSPEED_SOURCE_DIR}/cmake/Modules/GoogleBenchmark.cmake.in ${GOOGLEBENCHMARK_PREFIX}/CMakeLists.txt @ONLY)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
  -DALLOW_DOWNLOADING_GOOGLEBENCHMARK=${ALLOW_DOWNLOADING_GOOGLEBENCHMARK} -DGOOGLEBENCHMARK_PATH:PATH=${GOOGLEBENCHMARK_PATH} .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${GOOGLEBENCHMARK_PREFIX}
)

if(result)
  message(FATAL_ERROR "CMake step for googlebenchmark failed: ${result}")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${GOOGLEBENCHMARK_PREFIX}
)

if(result)
  message(FATAL_ERROR "Build step for googlebenchmark failed: ${result}")
endif()

# shared googlebenchmark exibits various spurious failures.
# let's insist on static library.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# not interested
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)

set(CMAKE_C_FLAGS_SAVE "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_SAVE "${CMAKE_CXX_FLAGS}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

set(CMAKE_CXX_CLANG_TIDY_SAVE "${CMAKE_CXX_CLANG_TIDY}")
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE_SAVE "${CMAKE_CXX_INCLUDE_WHAT_YOU_USE}")

unset(CMAKE_CXX_CLANG_TIDY)
unset(CMAKE_CXX_INCLUDE_WHAT_YOU_USE)

include(${GOOGLEBENCHMARK_PREFIX}/googlebenchmark-paths.cmake)

# Add googlebenchmark directly to our build. This defines the benchmark target.
add_subdirectory(${GOOGLEBENCHMARK_SOURCE_DIR}
                 ${GOOGLEBENCHMARK_BINARY_DIR}
                 EXCLUDE_FROM_ALL)

set_target_properties(benchmark PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:benchmark,INTERFACE_INCLUDE_DIRECTORIES>)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_SAVE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_SAVE}")

set(CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY_SAVE}")
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${CMAKE_CXX_INCLUDE_WHAT_YOU_USE_SAVE}")
