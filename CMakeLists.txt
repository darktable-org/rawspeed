cmake_minimum_required(VERSION 3.0)

cmake_policy(SET CMP0011 NEW)
cmake_policy(SET CMP0025 NEW)

# Avoid source tree pollution
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

If(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

include(GNUInstallDirs)

project(rawspeed CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WITH_PTHREADS "Enable threading support through pthread. Highly recommended" ON)
option(USE_XMLLINT "Run xmllint to test if data/cameras.xml is valid" ON)

if(UNIX AND NOT APPLE)
  option(BUILD_TESTING "Build the testing tree." ON)
else()
  option(BUILD_TESTING "Build the testing tree." OFF)
endif()

if(BUILD_TESTING)
  enable_testing()
endif()

include(CpuMarch)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

# should be fixed IMHO
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")

add_subdirectory(RawSpeed)
add_subdirectory(data)

if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
  include(CodeCoverage)
  setup_target_for_coverage(coverage ctest coverage)
endif()
