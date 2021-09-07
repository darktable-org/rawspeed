include(CheckCXXSymbolExists)

# C++17 has std::aligned_alloc :(

CHECK_CXX_SYMBOL_EXISTS(malloc cstdlib HAVE_MALLOC)
CHECK_CXX_SYMBOL_EXISTS(free   cstdlib HAVE_FREE)
if(NOT (HAVE_MALLOC AND HAVE_FREE))
  message(SEND_ERROR "Can't even find plain malloc() / free() !")
endif()

CHECK_CXX_SYMBOL_EXISTS(aligned_alloc cstdlib HAVE_ALIGNED_ALLOC)
if(HAVE_ALIGNED_ALLOC)
  return()
endif()

CHECK_CXX_SYMBOL_EXISTS(posix_memalign cstdlib HAVE_POSIX_MEMALIGN)
if(HAVE_POSIX_MEMALIGN)
  return()
endif()

CHECK_CXX_SYMBOL_EXISTS(_aligned_malloc malloc.h HAVE_ALIGNED_MALLOC)
CHECK_CXX_SYMBOL_EXISTS(_aligned_free   malloc.h HAVE_ALIGNED_FREE)
if(HAVE_ALIGNED_MALLOC AND HAVE_ALIGNED_FREE)
  return()
endif()

message(SEND_ERROR "Can't find any aligned malloc() implementation!")
