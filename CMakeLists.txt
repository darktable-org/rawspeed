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

set(GOOGLETEST_PATH "/usr/src/googletest" CACHE PATH
                    "Path to the googletest root tree. Should contain googletest and googlemock subdirs. And CMakeLists.txt in root, and in both of these subdirs")

set(PUGIXML_PATH "/usr/src/pugixml" CACHE PATH "Path to the pugixml root tree.")

set(ZLIB_PATH "/usr/src/zlib" CACHE PATH "Path to the zlib root tree.")

# Add a sensible build type default and warning because empty means no optimization and no debug info.
if(NOT CMAKE_BUILD_TYPE)
  message("WARNING: CMAKE_BUILD_TYPE is not defined!")

  set(default_build_type "RelWithDebInfo")

  if(BUILD_TESTING)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      set(default_build_type "Coverage")
    else()
      set(default_build_type "ASAN")
    endif()
  endif()

  message("WARNING: Defaulting to CMAKE_BUILD_TYPE=${default_build_type}. Use ccmake to set a proper value.")

  SET(CMAKE_BUILD_TYPE ${default_build_type} CACHE STRING "" FORCE)
endif(NOT CMAKE_BUILD_TYPE)

# yes, these build types need to be specified here in upper-case.
set(SPECIAL_BUILD_TYPES COVERAGE ASAN UBSAN)
set(CMAKE_CONFIGURATION_TYPES DEBUG RELWITHDEBINFO RELEASE ${SPECIAL_BUILD_TYPES})
set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "All the avaliable build types" FORCE)

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)
SET(CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING "Choose the type of build, options are: ${CMAKE_CONFIGURATION_TYPES}." FORCE )

# is this one of the known build types?
list (FIND CMAKE_CONFIGURATION_TYPES ${CMAKE_BUILD_TYPE} BUILD_TYPE)
if (${BUILD_TYPE} EQUAL -1)
 message(SEND_ERROR "Unknown build type: ${CMAKE_BUILD_TYPE}. Please specify one of: ${CMAKE_CONFIGURATION_TYPES}")
endif()

# is this a special build?
list (FIND SPECIAL_BUILD_TYPES ${CMAKE_BUILD_TYPE} SPECIAL_BUILD)
if (${SPECIAL_BUILD} EQUAL -1)
 unset(SPECIAL_BUILD)
else()
 set(SPECIAL_BUILD 1)
endif()

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
include(compiler-warnings)

add_custom_target(check ALL)
add_custom_target(dependencies ALL)

add_subdirectory(src)
add_subdirectory(data)

if(BUILD_TESTING AND CMAKE_BUILD_TYPE MATCHES "^[Cc][Oo][Vv][Ee][Rr][Aa][Gg][Ee]$")
  include(CodeCoverage)
  setup_target_for_coverage(coverage ctest coverage)
endif()
