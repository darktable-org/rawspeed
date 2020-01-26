cmake_minimum_required(VERSION 3.10.0)

cmake_policy(SET CMP0011 NEW)
cmake_policy(SET CMP0025 NEW)

# Avoid source tree pollution
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

If(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)

include(GNUInstallDirs)
include(FeatureSummary)
include(CheckCXXCompilerFlag)

# NOTE: if rawspeed is being added as a submodule to another build,
# this CMakeLists.txt should be added via add_subdirectory().

project(rawspeed CXX)

set(RAWSPEED_SOURCE_DIR "${PROJECT_SOURCE_DIR}" CACHE PATH "" FORCE)
set(RAWSPEED_BINARY_DIR "${PROJECT_BINARY_DIR}" CACHE PATH "" FORCE)

if("${CMAKE_SOURCE_DIR}" STREQUAL "${RAWSPEED_SOURCE_DIR}")
  set(RAWSPEED_STANDALONE_BUILD TRUE CACHE BOOL "")
else()
  set(RAWSPEED_STANDALONE_BUILD FALSE CACHE BOOL "")
endif()

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules/" ${CMAKE_MODULE_PATH})
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/" ${CMAKE_MODULE_PATH})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(BINARY_PACKAGE_BUILD "Sets march optimization to generic" OFF)
option(WITH_SSE2 "If SSE2 support is available, do build SSE2 codepaths" ON)
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  option(RAWSPEED_USE_LIBCXX "(Clang only) Build using libc++ as the standard library." OFF)

  # when OSX_DEPLOYMENT_TARGET is 10.9 and newer, the default is libc++,
  # but for no reason let's always specify/use libc++ for apple.
  if(APPLE)
    set(RAWSPEED_USE_LIBCXX ON)
  endif()
else()
  set(RAWSPEED_USE_LIBCXX OFF CACHE BOOL "(Clang only) Build using libc++ as the standard library." FORCE)
endif()

set(CMAKE_REQUIRED_FLAGS_ORIG "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "-c -std=c++14 -g")
# see https://bugs.llvm.org/show_bug.cgi?id=16091
# see https://reviews.llvm.org/rL221704
CHECK_CXX_SOURCE_COMPILES(
"struct foo {
  static auto func();
};

foo f;

auto foo::func() {
  return 1;
}"
  RAWSPEED_CXX_DEBUG_INFO_FOR_AUTO_IS_SUPPORTED
)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS_ORIG}")

option(RAWSPEED_ENABLE_DEBUG_INFO "Whether to generate debug info or not." ${RAWSPEED_CXX_DEBUG_INFO_FOR_AUTO_IS_SUPPORTED})

option(RAWSPEED_ENABLE_WERROR "Stop and fail the build, if a compiler warning is triggered." ON)

option(WITH_OPENMP "Enable OpenMP support." ON)
if(WITH_OPENMP)
  option(USE_BUNDLED_LLVMOPENMP "Build and use LLVM OpenMP runtime library in-tree" OFF)
else()
  set(USE_BUNDLED_LLVMOPENMP OFF CACHE BOOL "Build and use LLVM OpenMP runtime library in-tree" FORCE)
endif()
set(LLVMOPENMP_PATH "/usr/src/openmp" CACHE PATH "Path to the LLVM OpenMP runtime library root tree.")
if(WITH_OPENMP AND USE_BUNDLED_LLVMOPENMP)
  option(ALLOW_DOWNLOADING_LLVMOPENMP "If LLVM OpenMP runtime library src tree is not found in location specified by LLVMOPENMP_PATH, do fetch the archive from internet" OFF)
else()
  set(ALLOW_DOWNLOADING_LLVMOPENMP OFF CACHE BOOL "If LLVM OpenMP runtime library src tree is not found in location specified by LLVMOPENMP_PATH, do fetch the archive from internet" FORCE)
endif()

option(WITH_PUGIXML "Enable XML support for cameras.xml reading" ON)
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
option(BUILD_RS_TESTING "Build the testing tree." ON)
if(BUILD_RS_TESTING)
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
option(BUILD_DOCS "Build the documentation (Sphinx+Doxygen)." OFF)
option(BUILD_FUZZERS "Build the fuzzing tree." ON)
if(BUILD_TOOLS)
  option(RAWSPEED_ENABLE_SAMPLE_BASED_TESTING "If enabled, allows to use rstest to check the samples specified by RAWSPEED_REFERENCE_SAMPLE_ARCHIVE" OFF)
else()
  set(RAWSPEED_ENABLE_SAMPLE_BASED_TESTING OFF CACHE BOOL "If enabled, allows to use rstest to check the samples specified by RAWSPEED_REFERENCE_SAMPLE_ARCHIVE" FORCE)
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  option(USE_LLVM_OPT_REPORT "(Clang only) Save compiler optimizations records and show them" OFF)
else()
  set(USE_LLVM_OPT_REPORT OFF CACHE BOOL "(Clang only) Save compiler optimizations records and show them" FORCE)
endif()

set(GOOGLETEST_PATH "/usr/src/googletest" CACHE PATH
                    "Path to the googletest root tree. Should contain googletest and googlemock subdirs. And CMakeLists.txt in root, and in both of these subdirs")

set(PUGIXML_PATH "/usr/src/pugixml" CACHE PATH "Path to the pugixml root tree.")

set(ZLIB_PATH "/usr/src/zlib" CACHE PATH "Path to the zlib root tree.")

set(GOOGLEBENCHMARK_PATH "/usr/src/googlebenchmark" CACHE PATH
                    "Path to the googlebenchmark root tree.")

set(LIB_FUZZING_ENGINE "OFF" CACHE STRING "Either OFF, or overrides the fuzzing engine library (e.g. libFuzzer) that needs to be linked with all fuzz targets.")

set(RAWSPEED_REFERENCE_SAMPLE_ARCHIVE "~/raw-camera-samples/raw.pixls.us-unique" CACHE PATH "The location of the reference sample set to use. Should contain filelist.sha1 and timestamp.txt")

set(RAWSPEED_PROFDATA_FILE "${PROJECT_BINARY_DIR}/rawspeed.profdata"
    CACHE FILEPATH "The location of the .profdata file")

option(RAWSPEED_ENABLE_LTO "Add appropriate flag to the compile and link command lines, enabling link-time optimization." OFF)

if(CMAKE_MAKE_PROGRAM MATCHES "ninja")
  set(RAWSPEED_PARALLEL_LINK_JOBS "" CACHE STRING
    "Define the maximum number of concurrent link jobs.")

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
     RAWSPEED_ENABLE_LTO AND NOT RAWSPEED_PARALLEL_LINK_JOBS)
     message(STATUS "Clang's ThinLTO provides its own parallel linking - limiting parallel link jobs to 2.")
     set(RAWSPEED_PARALLEL_LINK_JOBS "2")
  endif()

  if(RAWSPEED_PARALLEL_LINK_JOBS)
    set_property(GLOBAL APPEND PROPERTY
                 JOB_POOLS link_job_pool=${RAWSPEED_PARALLEL_LINK_JOBS})
    set(CMAKE_JOB_POOL_LINK link_job_pool)
  endif()
else()
  set(RAWSPEED_PARALLEL_LINK_JOBS "" CACHE STRING
      "(Ninja-only) Define the maximum number of concurrent link jobs." FORCE)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND RAWSPEED_ENABLE_LTO)
    message(WARNING "RAWSPEED_ENABLE_LTO is enabled but RAWSPEED_PARALLEL_LINK_JOBS is only supported for Ninja.\nConsider using Ninja Generator!")
  endif()
endif()

include(cmake-command-wrappers)

include(build-type)

if(RAWSPEED_USE_LIBCXX)
  include(libc++)
endif()

if(USE_IWYU)
  include(iwyu)
endif()

if(USE_LLVM_OPT_REPORT)
  include(llvm-opt-report)
endif()

if((UNIX OR APPLE) AND USE_CLANG_TIDY)
  include(clang-tidy)
endif()

if(BUILD_RS_TESTING)
  enable_testing()
endif()

include(compiler-versions)
include(compiler-flags)

add_custom_target(check-rawspeed ALL)
add_custom_target(dependencies ALL)
add_custom_target(tests ALL)

include(compiler-warnings)

if(RAWSPEED_ENABLE_WERROR)
  set_directory_properties(PROPERTIES COMPILE_OPTIONS "-Werror")
endif()

if(BUILD_BENCHMARKING)
  add_custom_target(benchmarks ALL)
endif()

rawspeed_add_library(rawspeed STATIC)

include(src-dependencies)

add_subdirectory(src)

if(BUILD_RS_TESTING)
  add_subdirectory(test)
endif()

if(BUILD_BENCHMARKING)
  add_subdirectory(bench)
endif()

if(BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif()

add_subdirectory(data)

if(BUILD_DOCS)
  add_subdirectory(docs)
endif()

if(BUILD_RS_TESTING AND RAWSPEED_COVERAGE_BUILD)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    include(llvm-profdata)
    include(llvm-cov)
  elseif(CMAKE_COMPILER_IS_GNUCXX OR
         CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    include(gcc-gcov)

    find_package(LCov)
    find_package(GenHtml)

    if(LCov_FOUND AND GenHtml_FOUND)
      include(lcov)
      include(genhtml)
      include(gcc-coverage)
    else()
      message(WARNING "Did not find lcov and genhtml. "
                      "Will not be able to generate HTML reports")
    endif()
  endif()
endif()

feature_summary(WHAT ALL)
