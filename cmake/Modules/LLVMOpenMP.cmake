# Download and unpack LLVM OpenMP runtime library at configure time
set(LLVMOPENMP_PREFIX "${RAWSPEED_BINARY_DIR}/src/external/llvm-openmp")
configure_file(${RAWSPEED_SOURCE_DIR}/cmake/Modules/LLVMOpenMP.cmake.in ${LLVMOPENMP_PREFIX}/CMakeLists.txt @ONLY)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
  -DALLOW_DOWNLOADING_LLVMOPENMP=${ALLOW_DOWNLOADING_LLVMOPENMP} -DLLVMOPENMP_PATH:PATH=${LLVMOPENMP_PATH} .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${LLVMOPENMP_PREFIX}
)

if(result)
  message(FATAL_ERROR "CMake step for LLVM OpenMP runtime library failed: ${result}")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${LLVMOPENMP_PREFIX}
)

if(result)
  message(FATAL_ERROR "Build step for LLVM OpenMP runtime library failed: ${result}")
endif()

# We are building it separately from the LLVM itself.
set(OPENMP_STANDALONE_BUILD ON CACHE BOOL "" FORCE)

# We never want offloading support.
set(OPENMP_ENABLE_LIBOMPTARGET OFF CACHE BOOL "" FORCE)

# let's insist on static library.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LIBOMP_ENABLE_SHARED OFF CACHE BOOL "" FORCE)

set(CMAKE_C_FLAGS_SAVE "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_SAVE "${CMAKE_CXX_FLAGS}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

# libomp is UB-ridden.
set(no_sanitizers "-fno-sanitize=undefined,memory,integer")

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${no_sanitizers}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${no_sanitizers}")

set(CMAKE_CXX_CLANG_TIDY_SAVE "${CMAKE_CXX_CLANG_TIDY}")
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE_SAVE "${CMAKE_CXX_INCLUDE_WHAT_YOU_USE}")

unset(CMAKE_CXX_CLANG_TIDY)
unset(CMAKE_CXX_INCLUDE_WHAT_YOU_USE)

include(${LLVMOPENMP_PREFIX}/llvm-openmp-paths.cmake)

# Add llvm openmp directly to our build. This defines the omp target.
add_subdirectory(${LLVMOPENMP_SOURCE_DIR}
                 ${LLVMOPENMP_BINARY_DIR}
                 EXCLUDE_FROM_ALL)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_SAVE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_SAVE}")

set(CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY_SAVE}")
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${CMAKE_CXX_INCLUDE_WHAT_YOU_USE_SAVE}")
