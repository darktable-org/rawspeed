include(CheckCXXCompilerFlag)
include(CpuMarch)
include(CheckCXXCompilerFlagAndEnableIt)

message(STATUS "Checking for -std=c++11 support")
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(NOT COMPILER_SUPPORTS_CXX11)
        message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
message(STATUS "Checking for -std=c++11 support - works")

# always debug info
add_definitions(-g3)
add_definitions(-ggdb3)

# assertions
if(CMAKE_BUILD_TYPE MATCHES "^[Re][Ee][Ll][Ee][Aa][Ss][Ee]$")
  add_definitions(-DNDEBUG)
else()
  add_definitions(-DDEBUG)
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

set(coverage_compilation "-fprofile-arcs -ftest-coverage")
set(coverage_link "--coverage")
SET(CMAKE_CXX_FLAGS_COVERAGE
    "${coverage_compilation}"
    CACHE STRING "Flags used by the C++ compiler during coverage builds."
    FORCE )
SET(CMAKE_C_FLAGS_COVERAGE
    "${coverage_compilation}"
    CACHE STRING "Flags used by the C compiler during coverage builds."
     FORCE )
SET(CMAKE_EXE_LINKER_FLAGS_COVERAGE
    "${coverage_compilation} ${coverage_link}"
    CACHE STRING "Flags used for linking binaries during coverage builds."
    FORCE )
SET(CMAKE_SHARED_LINKER_FLAGS_COVERAGE
    "${coverage_compilation} ${coverage_link}"
    CACHE STRING "Flags used by the shared libraries linker during coverage builds."
    FORCE )
SET(CMAKE_SHARED_MODULE_FLAGS_COVERAGE
    "${coverage_compilation} ${coverage_link}"
    CACHE STRING "Flags used by the module linker during coverage builds."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_COVERAGE
    CMAKE_C_FLAGS_COVERAGE
    CMAKE_EXE_LINKER_FLAGS_COVERAGE
    CMAKE_SHARED_LINKER_FLAGS_COVERAGE
    CMAKE_SHARED_MODULE_FLAGS_COVERAGE )

set(SANITIZATION_BASE "-fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-common")
set(SANITIZATION_DEFAULTS "${SANITIZATION_BASE} -O1 -fstack-protector-strong")

set(asan "-fsanitize=address -U_FORTIFY_SOURCE")
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(asan "${asan} -fsanitize-address-use-after-scope")
endif()
SET(CMAKE_CXX_FLAGS_ASAN
    "${SANITIZATION_DEFAULTS} ${asan}"
    CACHE STRING "Flags used by the C++ compiler during ASAN builds."
    FORCE )
SET(CMAKE_C_FLAGS_ASAN
    "${SANITIZATION_DEFAULTS} ${asan}"
    CACHE STRING "Flags used by the C compiler during ASAN builds."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_ASAN
    CMAKE_C_FLAGS_ASAN )

set(ubsan "-fsanitize=undefined -fno-sanitize-recover=undefined")
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(ubsan "${ubsan} -fsanitize=integer -fno-sanitize-recover=integer")
endif()
SET(CMAKE_CXX_FLAGS_UBSAN
    "${SANITIZATION_DEFAULTS} ${ubsan}"
    CACHE STRING "Flags used by the C++ compiler during UBSAN builds."
    FORCE )
SET(CMAKE_C_FLAGS_UBSAN
    "${SANITIZATION_DEFAULTS} ${ubsan}"
    CACHE STRING "Flags used by the C compiler during UBSAN builds."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_UBSAN
    CMAKE_C_FLAGS_UBSAN )

set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
