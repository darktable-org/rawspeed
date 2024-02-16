cmake_minimum_required(VERSION 3.22) # FIXME: 3.24.2 / 3.25

project(llvm-openmp-download NONE)

# Enable ExternalProject CMake module
include(ExternalProject)

option(ALLOW_DOWNLOADING_LLVMOPENMP "If LLVM OpenMP runtime library src tree is not found in location specified by LLVMOPENMP_PATH, do fetch the archive from internet" OFF)
set(LLVMOPENMP_PATH "/usr/src/openmp" CACHE PATH "Path to the LLVM OpenMP runtime library root tree.")

# Download and install LLVM OpenMP runtime library

message(STATUS "Looking for LLVM OpenMP runtime library sources")
message(STATUS "Looking for LLVM OpenMP runtime library sources in ${LLVMOPENMP_PATH}")
if(EXISTS "${LLVMOPENMP_PATH}" AND IS_DIRECTORY "${LLVMOPENMP_PATH}" AND EXISTS "${LLVMOPENMP_PATH}/CMakeLists.txt")
  message(STATUS "Found LLVM OpenMP runtime library in ${LLVMOPENMP_PATH}")

  ExternalProject_Add(
    llvm-openmp
    PREFIX            "${CMAKE_BINARY_DIR}"
    DOWNLOAD_DIR      "${CMAKE_BINARY_DIR}/download"
    SOURCE_DIR        "${LLVMOPENMP_PATH}" # use existing src dir.
    BINARY_DIR        "${CMAKE_BINARY_DIR}/build"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
    INSTALL_COMMAND   ""
    TEST_COMMAND      ""
  )
else()
  if(NOT ALLOW_DOWNLOADING_LLVMOPENMP)
    message(SEND_ERROR "Did not find LLVM OpenMP runtime library sources! Either pass correct path in LLVMOPENMP_PATH, or enable ALLOW_DOWNLOADING_LLVMOPENMP, or disable USE_BUNDLED_LLVMOPENMP, or disable WITH_OPENMP.")
  else()
    message(WARNING "Did not find LLVM OpenMP runtime library sources! Fetching from web...")
    ExternalProject_Add(
      llvm-cmake
      URL               https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/cmake-17.0.6.src.tar.xz
      URL_HASH          SHA512=b2c5e404ca36542d44e1a7f2801bbcecbcf5f1e8c63b793bb2308228483406bdfe002720aadb913c0228cd2bbe5998465eaadc4a49fad6a5eb6ff907fa5fd45a
      PREFIX            "${CMAKE_BINARY_DIR}/llvm-cmake"
      STAMP_DIR         "stamp"
      DOWNLOAD_DIR      "download"
      SOURCE_DIR        "${CMAKE_BINARY_DIR}/src/cmake"
      BINARY_DIR        "build"
      CONFIGURE_COMMAND ""
      BUILD_COMMAND     ""
      INSTALL_COMMAND   ""
      TEST_COMMAND      ""
    )
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/src/openmp")
    ExternalProject_Add(
      llvm-openmp
      URL               https://github.com/llvm/llvm-project/releases/download/llvmorg-17.0.6/openmp-17.0.6.src.tar.xz
      URL_HASH          SHA512=836c48db873e3da64835913c2c1d80efaebdfc7061f153acc7b478ea0d73b9c546a09d70dd1465e3e92684947c4a9cb197886b29cbd42418a778faa56b08cc5e
      PREFIX            "${CMAKE_BINARY_DIR}/llvm-openmp"
      STAMP_DIR         "stamp"
      DOWNLOAD_DIR      "download"
      SOURCE_DIR        "${CMAKE_BINARY_DIR}/src/openmp"
      BINARY_DIR        "build"
      CONFIGURE_COMMAND ""
      BUILD_COMMAND     ""
      INSTALL_COMMAND   ""
      TEST_COMMAND      ""
    )
  endif()
endif()

ExternalProject_Get_Property(llvm-openmp SOURCE_DIR BINARY_DIR)
file(WRITE llvm-openmp-paths.cmake
"set(LLVMOPENMP_SOURCE_DIR \"${SOURCE_DIR}\")
set(LLVMOPENMP_BINARY_DIR \"${BINARY_DIR}\")
")
