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

if(NOT (CMAKE_VERSION VERSION_LESS 3.4.0-rc1))
  project(rawspeed CXX)
else()
  # FindThreads.cmake issue
  project(rawspeed CXX C)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(INTERPROCEDURAL_OPTIMIZATION ON)

option(BINARY_PACKAGE_BUILD "Sets march optimization to generic" OFF)
option(WITH_PTHREADS "Enable threading support through pthread. Highly recommended" ON)
option(WITH_OPENMP "Enable OpenMP support. Only used for tools, not the library itself." ON)
set(WITH_PUGIXML ON CACHE BOOL "Enable XML support for cameras.xml reading" FORCE)
if(WITH_PUGIXML)
  option(USE_BUNDLED_PUGIXML "Build and use pugixml in-tree" OFF)
else()
  set(USE_BUNDLED_PUGIXML OFF CACHE BOOL "Build and use pugixml in-tree" FORCE)
endif()
if(WITH_PUGIXML AND USE_BUNDLED_PUGIXML)
  option(ALLOW_DOWNLOADING_PUGIXML "If pugixml src tree is not found in location specified by PUGIXML_PATH, do fetch the archive from internet" OFF)
else()
  set(ALLOW_DOWNLOADING_PUGIXML OFF CACHE BOOL "If pugixml src tree is not found in location specified by PUGIXML_PATH, do fetch the archive from internet" FORCE)
endif()
option(WITH_JPEG "Enable JPEG support for DNG Lossy JPEG support" ON)
option(WITH_ZLIB "Enable ZLIB support for DNG deflate support" ON)
if(WITH_ZLIB)
  option(USE_BUNDLED_ZLIB "Build and use zlib in-tree" OFF)
else()
  set(USE_BUNDLED_ZLIB OFF CACHE BOOL "Build and use zlib in-tree" FORCE)
endif()
if(WITH_ZLIB AND USE_BUNDLED_ZLIB)
  option(ALLOW_DOWNLOADING_ZLIB "If ZLIB src tree is not found in location specified by ZLIB_PATH, do fetch the archive from internet" OFF)
else()
  set(ALLOW_DOWNLOADING_ZLIB OFF CACHE BOOL "If ZLIB src tree is not found in location specified by ZLIB_PATH, do fetch the archive from internet" FORCE)
endif()
option(USE_XMLLINT "Run xmllint to test if data/cameras.xml is valid" ON)
option(USE_IWYU "Run iwyu tool when compiling sources" OFF)
option(USE_CLANG_TIDY "Run clang-tidy tool when compiling sources" OFF)
option(BUILD_TESTING "Build the testing tree." ON)
if(BUILD_TESTING)
  option(ALLOW_DOWNLOADING_GOOGLETEST "If googletest src tree is not found in location specified by GOOGLETEST_PATH, do fetch the archive from internet" OFF)
else()
  set(ALLOW_DOWNLOADING_GOOGLETEST OFF CACHE BOOL "If googletest src tree is not found in location specified by GOOGLETEST_PATH, do fetch the archive from internet" FORCE)
endif()
option(BUILD_TOOLS "Build some tools (identify, rstest)." ON)
option(BUILD_BENCHMARKING "Build some benchmarks." OFF)
if(BUILD_BENCHMARKING)
  option(ALLOW_DOWNLOADING_GOOGLEBENCHMARK "If googlebenchmark src tree is not found in location specified by GOOGLEBENCHMARK_PATH, do fetch the archive from internet" OFF)
else()
  set(ALLOW_DOWNLOADING_GOOGLEBENCHMARK OFF CACHE BOOL "If googlebenchmark src tree is not found in location specified by GOOGLEBENCHMARK_PATH, do fetch the archive from internet" FORCE)
endif()

set(GOOGLETEST_PATH "/usr/src/googletest" CACHE PATH
                    "Path to the googletest root tree. Should contain googletest and googlemock subdirs. And CMakeLists.txt in root, and in both of these subdirs")

set(PUGIXML_PATH "/usr/src/pugixml" CACHE PATH "Path to the pugixml root tree.")

set(ZLIB_PATH "/usr/src/zlib" CACHE PATH "Path to the zlib root tree.")

set(GOOGLEBENCHMARK_PATH "/usr/src/googlebenchmark" CACHE PATH
                    "Path to the googlebenchmark root tree.")

include(build-type)

if(USE_IWYU)
  include(iwyu)
endif()

if((UNIX OR APPLE) AND USE_CLANG_TIDY)
  include(clang-tidy)
endif()

if(BUILD_TESTING)
  enable_testing()
endif()

include(compiler-versions)
include(compiler-flags)

add_custom_target(check ALL)
add_custom_target(dependencies ALL)

add_subdirectory(src)
add_subdirectory(data)

if(BUILD_TESTING AND CMAKE_BUILD_TYPE STREQUAL "COVERAGE")
  include(CodeCoverage)
  setup_target_for_coverage(coverage ctest coverage)
  include(GCOV)
endif()
