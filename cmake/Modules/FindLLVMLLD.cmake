find_program(LLVMLLD_EXECUTABLE NAMES ld.lld lld lld-7 lld-6.0 lld-5.0 lld-4.0 lld-3.9)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMLLD
  DEFAULT_MSG
  LLVMLLD_EXECUTABLE)

SET_PACKAGE_PROPERTIES(LLVMLLD PROPERTIES
  URL https://lld.llvm.org/
  DESCRIPTION "the LLVM Linker"
)

set(LLVMLLD_INCREMENTAL_CACHE_PATH "${CMAKE_BINARY_DIR}/clang-thinlto-cache"
    CACHE PATH "The location of clang's ThinLTO Incremental cache." FORCE)

# For unoptimized {A+UB}SAN build with all debug info - fresh cache is ~492Mb.
# For -O3 + {A+UB}SAN build with all debug info - fresh cache is ~320Mb.
set(LLVMLLD_INCREMENTAL_CACHE_CACHE_SIZE_BYTES "1g"
      CACHE STRING "The maximum size for the ThinLTO Incremental cache directory, X{,k,m,g} bytes" FORCE)

set(LLVMLLD_INCREMENTAL_LDFLAGS
    "-Wl,--thinlto-cache-dir=\"${LLVMLLD_INCREMENTAL_CACHE_PATH}\" -Wl,--thinlto-cache-policy,cache_size_bytes=${LLVMLLD_INCREMENTAL_CACHE_CACHE_SIZE_BYTES}"
    CACHE STRING "(Clang only) Add -flto=thin flag to the compile and link command lines, enabling link-time optimization." FORCE)
