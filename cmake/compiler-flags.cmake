include(CheckCXXCompilerFlag)
include(CpuMarch)
include(CheckCXXCompilerFlagAndEnableIt)

CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
if(NOT COMPILER_SUPPORTS_CXX11)
        message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# always debug info
add_definitions(-g3)
add_definitions(-ggdb3)

# should be fixed IMHO
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-fno-strict-aliasing)

# assertions
if(CMAKE_BUILD_TYPE MATCHES "^[Re][Ee][Ll][Ee][Aa][Ss][Ee]$")
  add_definitions(-DNDEBUG)
else()
  add_definitions(-DDEBUG)
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

SET(CMAKE_CXX_FLAGS_COVERAGE
    "-g -O0 -fprofile-arcs -ftest-coverage"
    CACHE STRING "Flags used by the C++ compiler during coverage builds."
    FORCE )
SET(CMAKE_C_FLAGS_COVERAGE
    "-g -O0 -fprofile-arcs -ftest-coverage"
    CACHE STRING "Flags used by the C compiler during coverage builds."
    FORCE )
SET(CMAKE_EXE_LINKER_FLAGS_COVERAGE
    ""
    CACHE STRING "Flags used for linking binaries during coverage builds."
    FORCE )
SET(CMAKE_SHARED_LINKER_FLAGS_COVERAGE
    ""
    CACHE STRING "Flags used by the shared libraries linker during coverage builds."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_COVERAGE
    CMAKE_C_FLAGS_COVERAGE
    CMAKE_EXE_LINKER_FLAGS_COVERAGE
    CMAKE_SHARED_LINKER_FLAGS_COVERAGE )

SET(CMAKE_CXX_FLAGS_ASAN
    "-fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-common -U_FORTIFY_SOURCE -fsanitize=address -fstack-protector-strong"
    CACHE STRING "Flags used by the C++ compiler during ASAN builds."
    FORCE )
SET(CMAKE_C_FLAGS_ASAN
    "-fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-common -U_FORTIFY_SOURCE -fsanitize=address -fstack-protector-strong"
    CACHE STRING "Flags used by the C compiler during ASAN builds."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_ASAN
    CMAKE_C_FLAGS_ASAN )

set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
