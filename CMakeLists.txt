cmake_minimum_required(VERSION 3.0)

cmake_policy(SET CMP0011 NEW)
cmake_policy(SET CMP0025 NEW)

# Avoid source tree pollution
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

If(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)

include(GNUInstallDirs)

project(rawspeed CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BINARY_PACKAGE_BUILD "Sets march optimization to generic" OFF)
option(WITH_PTHREADS "Enable threading support through pthread. Highly recommended" ON)
option(WITH_OPENMP "Enable OpenMP support. Only used for tools, not the library itself." ON)
option(USE_XMLLINT "Run xmllint to test if data/cameras.xml is valid" ON)
option(USE_IWYU "Run iwyu tool when compiling sources" OFF)
option(BUILD_TESTING "Build the testing tree." ON)
option(BUILD_TOOLS "Build some tools (identify, rstest)." ON)

# Add a sensible build type default and warning because empty means no optimization and no debug info.
if(NOT CMAKE_BUILD_TYPE)
  message("WARNING: CMAKE_BUILD_TYPE is not defined!")

  set(default_build_type "RelWithDebInfo")

  if(BUILD_TESTING)
    set(default_build_type "Coverage")
  endif()

  message("WARNING: Defaulting to CMAKE_BUILD_TYPE=${default_build_type}. Use ccmake to set a proper value.")

  SET(CMAKE_BUILD_TYPE ${default_build_type} CACHE STRING "Choose the type of build, options are: None Coverage Debug RelWithDebInfo Release." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if(USE_IWYU)
  include(iwyu)
endif()

if(BUILD_TESTING)
  enable_testing()
endif()

include(compiler-flags)
include(compiler-warnings)

add_custom_target(check ALL)

add_subdirectory(RawSpeed)
add_subdirectory(data)

if(BUILD_TESTING AND CMAKE_BUILD_TYPE MATCHES "^[Cc][Oo][Vv][Ee][Rr][Aa][Gg][Ee]$")
  include(CodeCoverage)
  setup_target_for_coverage(coverage ctest coverage)
endif()
