# want C++14 support, openmp 4.0 support.

# GCC5 is both the first one with acceptable support for C++14, and OpenMP4.
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 5.0)
  message(FATAL_ERROR "GNU C compiler version ${CMAKE_C_COMPILER_VERSION} is too old. Need 5.0+")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
  message(FATAL_ERROR "GNU C++ compiler version ${CMAKE_CXX_COMPILER_VERSION} is too old. Need 5.0+")
endif()

# clang-3.5 is the first one with acceptable support for C++14.
if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 3.5)
  message(FATAL_ERROR "LLVM Clang C compiler version ${CMAKE_C_COMPILER_VERSION} is too old. Need 3.5+")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.5)
  message(FATAL_ERROR "LLVM Clang C++ compiler version ${CMAKE_CXX_COMPILER_VERSION} is too old. Need 3.5+")
endif()

# clang-3.9 is the first one with acceptable support for OpenMP4
if(WITH_OPENMP AND CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 3.9)
  message(FATAL_ERROR "LLVM Clang C compiler version ${CMAKE_C_COMPILER_VERSION} is too old. Need 3.9+ (or pass -DWITH_OPENMP=OFF to disable OpenMP support)")
endif()
if(WITH_OPENMP AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.9)
  message(FATAL_ERROR "LLVM Clang C++ compiler version ${CMAKE_CXX_COMPILER_VERSION} is too old. Need 3.9+ (or pass -DWITH_OPENMP=OFF to disable OpenMP support)")
endif()

# XCode 6.0 is the first one with acceptable support for C++14. (based on clang-3.5svn)
if(CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 6.0)
  message(FATAL_ERROR "XCode (Apple clang) C compiler version ${CMAKE_C_COMPILER_VERSION} is too old. Need 6.0+")
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 6.0)
  message(FATAL_ERROR "XCode (Apple clang) C++ compiler version ${CMAKE_CXX_COMPILER_VERSION} is too old. Need 6.0+")
endif()

# XCode 10.0 is the first one with support for OpenMP4 (based on clang-6.0svn)
if(WITH_OPENMP AND CMAKE_C_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 10.0)
  message(FATAL_ERROR "XCode (Apple clang) C compiler version ${CMAKE_C_COMPILER_VERSION} is too old. Need 10.0+ (or pass -DWITH_OPENMP=OFF to disable OpenMP support)")
endif()
if(WITH_OPENMP AND CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.0)
  message(FATAL_ERROR "XCode (Apple clang) C++ compiler version ${CMAKE_CXX_COMPILER_VERSION} is too old. Need 10.0+ (or pass -DWITH_OPENMP=OFF to disable OpenMP support)")
endif()

# C++17 ?

# if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 7)
#   message(WARNING "Support for GNU C compiler version ${CMAKE_C_COMPILER_VERSION} is soft-deprecated. Consider upgrading to 7+")
# endif()

# if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7)
#   message(WARNING "Support for GNU C++ compiler version ${CMAKE_CXX_COMPILER_VERSION} is soft-deprecated. Consider upgrading to 7+")
# endif()

# if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_VERSION VERSION_LESS 4.0)
#   message(WARNING "LLVM Clang C compiler version ${CMAKE_C_COMPILER_VERSION} is soft-deprecated. Consider upgrading to 4.0+")
# endif()

# if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.0)
#   message(WARNING "LLVM Clang C++ compiler version ${CMAKE_CXX_COMPILER_VERSION} is soft-deprecated. Consider upgrading to 4.0+")
# endif()
