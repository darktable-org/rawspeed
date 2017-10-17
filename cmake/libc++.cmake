if(NOT RAWSPEED_USE_LIBCXX)
  return()
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES stdc++)
list(APPEND CMAKE_CXX_IMPLICIT_LINK_LIBRARIES c++)
list(REMOVE_DUPLICATES CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)

# Also remove incorrectly parsed -lto_library flag
# It wasn't present with Xcode 7.2 and appeared before 8.3 release
# cmake 3.7.2 doesn't understand this flag and thinks it's a library
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_LINK_LIBRARIES to_library)
