include(CheckCXXCompilerFlag)
include(CpuMarch)
include(CheckCXXCompilerFlagAndEnableIt)

# yes, need to keep both the CMAKE_CXX_FLAGS and CMAKE_CXX_STANDARD.
# with just the CMAKE_CXX_STANDARD, try_compile() breaks:
#   https://gitlab.kitware.com/cmake/cmake/issues/16456
# with just the CMAKE_CXX_FLAGS, 'bundled' pugixml breaks tests
#   https://github.com/darktable-org/rawspeed/issues/112#issuecomment-321517003

message(STATUS "Checking for -std=c++17 support")
CHECK_CXX_COMPILER_FLAG("-std=c++17" COMPILER_SUPPORTS_CXX17)
if(NOT COMPILER_SUPPORTS_CXX17)
  message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++17 support. Please use a different C++ compiler.")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
  message(STATUS "Checking for -std=c++17 support - works")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#target_compile_features(rawspeed PUBLIC cxx_std_17)

include(debug-info)

if(RAWSPEED_RELEASE_BUILD)
  # want assertions in all but Release build type.
  add_definitions(-DNDEBUG)
elseif(NOT (RAWSPEED_RELWITHDEBINFO_BUILD OR RAWSPEED_FUZZ_BUILD))
  # if not Release/RelWithDebInfo/Fuzz build, enable extra debug mode
  add_definitions(-DDEBUG)

  # all this does not work with integer sanitizer
  # add_definitions(-D_GLIBCXX_ASSERTIONS)
  # add_definitions(-D_GLIBCXX_DEBUG)
  # add_definitions(-D_GLIBCXX_DEBUG_PEDANTIC)

  add_definitions(-D_GLIBCXX_SANITIZE_VECTOR)
endif()

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

IF(NOT APPLE AND NOT WIN32)
  set(linkerflags "-Wl,--as-needed")
ELSE()
  set(linkerflags "")
ENDIF()
# NOT CMAKE_STATIC_LINKER_FLAGS
SET(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} ${linkerflags}"
    )
SET(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} ${linkerflags}"
    )
SET(CMAKE_MODULE_LINKER_FLAGS
    "${CMAKE_MODULE_LINKER_FLAGS} ${linkerflags}"
    )

if(RAWSPEED_ENABLE_LTO)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    include(llvm-toolchain)
    set(lto_compile "-flto=thin -fforce-emit-vtables -fwhole-program-vtables -fstrict-vtable-pointers")
    set(lto_link "-flto=thin -fuse-ld=\"${LLVMLLD_EXECUTABLE}\" ${LLVMLLD_INCREMENTAL_LDFLAGS}")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    include(gcc-toolchain)
    set(lto_compile "-flto")
    set(lto_link "-flto")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(lto_compile "-flto=thin -fstrict-vtable-pointers")
    set(lto_link "-flto=thin")
  endif()

  set(CMAKE_C_FLAGS
      "${CMAKE_C_FLAGS} ${lto_compile}"
      )
  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} ${lto_compile}"
      )
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} ${lto_link}"
      )
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} ${lto_link}"
      )
  set(CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS} ${lto_link}"
      )
endif()

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(coverage_compilation "-fprofile-instr-generate=\"default-%m-%p.profraw\" -fcoverage-mapping")
  set(coverage_link "")
elseif(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(coverage_compilation "-fprofile-arcs -ftest-coverage")
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    set(coverage_link "--coverage")
  endif()
endif()

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
SET(CMAKE_MODULE_LINKER_FLAGS_COVERAGE
    "${coverage_compilation} ${coverage_link}"
    CACHE STRING "Flags used by the module linker during coverage builds."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_COVERAGE
    CMAKE_C_FLAGS_COVERAGE
    CMAKE_EXE_LINKER_FLAGS_COVERAGE
    CMAKE_SHARED_LINKER_FLAGS_COVERAGE
    CMAKE_MODULE_LINKER_FLAGS_COVERAGE )

# -fstack-protector-all
set(SANITIZATION_DEFAULTS "-O3 -fno-optimize-sibling-calls")

set(asan "-fsanitize=address -fno-omit-frame-pointer -fno-common -U_FORTIFY_SOURCE")
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(asan "${asan} -fsanitize-address-use-after-scope")
endif()

set(ubsan "-fsanitize=undefined -fno-sanitize-recover=undefined")
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(ubsan "${ubsan} -fsanitize=integer -fno-sanitize-recover=integer -fno-sanitize=unsigned-shift-base")
endif()

SET(CMAKE_CXX_FLAGS_SANITIZE
    "${SANITIZATION_DEFAULTS} ${asan} ${ubsan}"
    CACHE STRING "Flags used by the C++ compiler during sanitized (ASAN+UBSAN) builds."
    FORCE )
SET(CMAKE_C_FLAGS_SANITIZE
    "${SANITIZATION_DEFAULTS} ${asan} ${ubsan}"
    CACHE STRING "Flags used by the C compiler during sanitized (ASAN+UBSAN) builds."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_SANITIZE
    CMAKE_C_FLAGS_SANITIZE )

set(fuzz "-O3 -ffast-math")

if(NOT LIB_FUZZING_ENGINE)
  set(fuzz "${fuzz} ${asan} ${ubsan}")
  set(fuzz "${fuzz} -fsanitize=fuzzer-no-link")
else()
  # specialhandling: oss-fuzz provides all the needed flags already.
  message(STATUS "LIB_FUZZING_ENGINE override option is passed, not setting special compiler flags.")
endif()

set(fuzz "${fuzz} -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION")
set(fuzz "${fuzz} -ffunction-sections -fdata-sections")
set(fuzz_link "-Wl,--gc-sections")
SET(CMAKE_CXX_FLAGS_FUZZ
    "${fuzz}"
    CACHE STRING "Flags used by the C++ compiler during FUZZ builds."
    FORCE )
SET(CMAKE_C_FLAGS_FUZZ
    "${fuzz}"
    CACHE STRING "Flags used by the C compiler during FUZZ builds."
    FORCE )
SET(CMAKE_EXE_LINKER_FLAGS_FUZZ
    "${fuzz} ${fuzz_link}"
    CACHE STRING "Flags used for linking binaries during FUZZ builds."
    FORCE )
SET(CMAKE_SHARED_LINKER_FLAGS_FUZZ
    "${fuzz} ${fuzz_link}"
    CACHE STRING "Flags used by the shared libraries linker during FUZZ builds."
    FORCE )
SET(CMAKE_MODULE_LINKER_FLAGS_FUZZ
    "${fuzz} ${fuzz_link}"
    CACHE STRING "Flags used by the module linker during FUZZ builds."
    FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_FUZZ
    CMAKE_C_FLAGS_FUZZ
    CMAKE_EXE_LINKER_FLAGS_FUZZ
    CMAKE_SHARED_LINKER_FLAGS_FUZZ
    CMAKE_MODULE_LINKER_FLAGS_FUZZ )

set(ubsan "${SANITIZATION_DEFAULTS} -fsanitize=thread")
SET(CMAKE_CXX_FLAGS_TSAN
    "${ubsan}"
    CACHE STRING "Flags used by the C++ compiler during TSAN builds."
    FORCE )
SET(CMAKE_C_FLAGS_TSAN
    "${ubsan}"
    CACHE STRING "Flags used by the C compiler during TSAN builds."
    FORCE )
# SET(CMAKE_EXE_LINKER_FLAGS_TSAN
#     "-no-pie"
#     CACHE STRING "Flags used for linking binaries during TSAN builds."
#     FORCE )
# SET(CMAKE_SHARED_LINKER_FLAGS_TSAN
#     "-no-pie"
#     CACHE STRING "Flags used by the shared libraries linker during TSAN builds."
#     FORCE )
# SET(CMAKE_SHARED_MODULE_FLAGS_TSAN
#     "-no-pie"
#     CACHE STRING "Flags used by the module linker during TSAN builds."
#     FORCE )
MARK_AS_ADVANCED(
    CMAKE_CXX_FLAGS_TSAN
    CMAKE_C_FLAGS_TSAN
    CMAKE_EXE_LINKER_FLAGS_TSAN
    CMAKE_SHARED_LINKER_FLAGS_TSAN
    CMAKE_SHARED_MODULE_FLAGS_TSAN )

set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O2")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O2")

set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
