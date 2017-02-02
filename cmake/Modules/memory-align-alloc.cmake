include(CheckCXXSymbolExists)

# C++17 has std::aligned_alloc :(

CHECK_CXX_SYMBOL_EXISTS(malloc cstdlib HAVE_MALLOC)
CHECK_CXX_SYMBOL_EXISTS(free   cstdlib HAVE_FREE)
if(NOT (HAVE_MALLOC AND HAVE_FREE))
  message(SEND_ERROR "Can't even find plain malloc() / free() !")
endif()

CHECK_CXX_SYMBOL_EXISTS(posix_memalign cstdlib HAVE_POSIX_MEMALIGN)
if(HAVE_POSIX_MEMALIGN)
  add_definitions(-DHAVE_POSIX_MEMALIGN)
  return()
endif()

CHECK_CXX_SYMBOL_EXISTS(aligned_alloc cstdlib HAVE_ALIGNED_ALLOC)
if(HAVE_ALIGNED_ALLOC)
  add_definitions(-DHAVE_ALIGNED_ALLOC)
  return()
endif()

CHECK_CXX_SYMBOL_EXISTS(_mm_malloc xmmintrin.h HAVE_MM_MALLOC)
CHECK_CXX_SYMBOL_EXISTS(_mm_free   xmmintrin.h HAVE_MM_FREE)
if(HAVE_MM_MALLOC AND HAVE_MM_FREE)
  add_definitions(-DHAVE_MM_MALLOC)
  return()
endif()

CHECK_CXX_SYMBOL_EXISTS(_aligned_malloc malloc.h HAVE_ALIGNED_MALLOC)
CHECK_CXX_SYMBOL_EXISTS(_aligned_free   malloc.h HAVE_ALIGNED_FREE)
if(HAVE_ALIGNED_MALLOC AND HAVE_ALIGNED_FREE)
  add_definitions(-DHAVE_ALIGNED_MALLOC)
  return()
endif()

message(WARNING "Can't find any aligned malloc() implementation!")
