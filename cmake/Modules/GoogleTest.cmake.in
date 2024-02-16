cmake_minimum_required(VERSION 3.22) # FIXME: 3.24.2 / 3.25

project(googletest-download NONE)

# Enable ExternalProject CMake module
include(ExternalProject)

option(ALLOW_DOWNLOADING_GOOGLETEST "If googletest src tree is not found in location specified by GOOGLETEST_PATH, do fetch the archive from internet" OFF)
set(GOOGLETEST_PATH "/usr/src/googletest" CACHE PATH
                    "Path to the googletest root tree. Should contain googletest and googlemock subdirs. And CMakeLists.txt in root, and in both of these subdirs")

# Download and install GoogleTest

message(STATUS "Looking for Google Test sources")
message(STATUS "Looking for Google Test sources in ${GOOGLETEST_PATH}")
if(EXISTS "${GOOGLETEST_PATH}"            AND IS_DIRECTORY "${GOOGLETEST_PATH}"            AND EXISTS "${GOOGLETEST_PATH}/CMakeLists.txt" AND
   EXISTS "${GOOGLETEST_PATH}/googletest" AND IS_DIRECTORY "${GOOGLETEST_PATH}/googletest" AND EXISTS "${GOOGLETEST_PATH}/googletest/CMakeLists.txt" AND
   EXISTS "${GOOGLETEST_PATH}/googlemock" AND IS_DIRECTORY "${GOOGLETEST_PATH}/googlemock" AND EXISTS "${GOOGLETEST_PATH}/googlemock/CMakeLists.txt")
  message(STATUS "Found Google Test in ${GOOGLETEST_PATH}")

  ExternalProject_Add(
    googletest
    PREFIX            "${CMAKE_BINARY_DIR}"
    DOWNLOAD_DIR      "${CMAKE_BINARY_DIR}/download"
    SOURCE_DIR        "${GOOGLETEST_PATH}" # use existing src dir.
    BINARY_DIR        "${CMAKE_BINARY_DIR}/build"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
    INSTALL_COMMAND   ""
    TEST_COMMAND      ""
  )
else()
  if(NOT ALLOW_DOWNLOADING_GOOGLETEST)
    message(SEND_ERROR "Did not find Google Test sources! Either pass correct path in GOOGLETEST_PATH, or enable ALLOW_DOWNLOADING_GOOGLETEST, or disable BUILD_TESTING.")
  else()
    message(WARNING "Did not find Google Test sources! Fetching from web...")
    ExternalProject_Add(
      googletest
      URL               https://github.com/google/googletest/archive/v1.13.0.tar.gz
      URL_HASH          SHA512=70c0cfb1b4147bdecb467ecb22ae5b5529eec0abc085763213a796b7cdbd81d1761d12b342060539b936fa54f345d33f060601544874d6213fdde79111fa813e
      PREFIX            "${CMAKE_BINARY_DIR}"
      STAMP_DIR         "${CMAKE_BINARY_DIR}/stamp"
      DOWNLOAD_DIR      "${CMAKE_BINARY_DIR}/download"
      SOURCE_DIR        "${CMAKE_BINARY_DIR}/src"
      BINARY_DIR        "${CMAKE_BINARY_DIR}/build"
      CONFIGURE_COMMAND ""
      BUILD_COMMAND     ""
      INSTALL_COMMAND   ""
      TEST_COMMAND      ""
    )
  endif()
endif()

ExternalProject_Get_Property(googletest SOURCE_DIR BINARY_DIR)
file(WRITE googletest-paths.cmake
"set(GOOGLETEST_SOURCE_DIR \"${SOURCE_DIR}\")
set(GOOGLETEST_BINARY_DIR \"${BINARY_DIR}\")
")
