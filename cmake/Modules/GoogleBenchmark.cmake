cmake_minimum_required(VERSION 3.0)

project(googlebenchmark NONE)

# Download and unpack googlebenchmark at configure time
configure_file(${CMAKE_SOURCE_DIR}/cmake/Modules/GoogleBenchmark.cmake.in ${CMAKE_BINARY_DIR}/googlebenchmark/CMakeLists.txt @ONLY)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
  -DALLOW_DOWNLOADING_GOOGLEBENCHMARK=${ALLOW_DOWNLOADING_GOOGLEBENCHMARK} -DGOOGLEBENCHMARK_PATH:PATH=${GOOGLEBENCHMARK_PATH} .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googlebenchmark
)

if(result)
  message(FATAL_ERROR "CMake step for googlebenchmark failed: ${result}")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googlebenchmark
)

if(result)
  message(FATAL_ERROR "Build step for googlebenchmark failed: ${result}")
endif()

# shared googlebenchmark exibits varous spririous failures.
# let's insist on static library.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(CMAKE_C_FLAGS_SAVE "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_SAVE "${CMAKE_CXX_FLAGS}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

set(CMAKE_CXX_CLANG_TIDY_SAVE "${CMAKE_CXX_CLANG_TIDY}")
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE_SAVE "${CMAKE_CXX_INCLUDE_WHAT_YOU_USE}")

unset(CMAKE_CXX_CLANG_TIDY)
unset(CMAKE_CXX_INCLUDE_WHAT_YOU_USE)

# Add googlebenchmark directly to our build. This defines the benchmark target.
add_subdirectory(${CMAKE_BINARY_DIR}/googlebenchmark/googlebenchmark-src
                 ${CMAKE_BINARY_DIR}/googlebenchmark/googlebenchmark-build
                 EXCLUDE_FROM_ALL)

set_target_properties(benchmark PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:benchmark,INTERFACE_INCLUDE_DIRECTORIES>)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_SAVE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_SAVE}")

set(CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY_SAVE}")
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${CMAKE_CXX_INCLUDE_WHAT_YOU_USE_SAVE}")
