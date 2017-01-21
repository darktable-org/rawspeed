if(BUILD_TESTING)
  # want GTEST_ADD_TESTS() macro. NOT THE ACTUAL MODULE!
  include(GTEST_ADD_TESTS)

  # for the actual gtest:

  # at least in debian, they are the package only installs their source code,
  # so if one wants to use them, he needs to compile them in-tree
  include(GoogleTest)
endif()

if(WITH_PTHREADS)
  set(CMAKE_THREAD_PREFER_PTHREAD 1)
  find_package(Threads REQUIRED)
  if(NOT CMAKE_USE_PTHREADS_INIT)
    message(SEND_ERROR "Did not found POSIX Threads! Either make it find pthreads, or pass -DWITH_PTHREADS=OFF to disable threading.")
  endif()
else()
  message(STATUS "PThread-based threading is disabled. Not searching for PThreads")
  add_definitions(-DNO_PTHREAD)
endif()

if(WITH_OPENMP)
  find_package(OpenMP)
endif()

find_package(Pugixml 1.2 REQUIRED)
include_directories(SYSTEM ${Pugixml_INCLUDE_DIRS})

find_package(JPEG REQUIRED)
include_directories(SYSTEM ${JPEG_INCLUDE_DIRS})

if (WITH_ZLIB)
  find_package(ZLIB REQUIRED)
  if(NOT ZLIB_FOUND)
    message(SEND_ERROR "Did not find ZLIB! Either make it find ZLIB, or pass -DWITH_ZLIB=OFF to disable ZLIB.")
  endif()
  add_definitions(-DHAVE_ZLIB)
  include_directories(SYSTEM ${ZLIB_INCLUDE_DIRS})
else()
  message(STATUS "ZLIB is disabled, DNG deflate support won't be available.")
endif()

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.h")
set(CONFIG_INCLUDE_PATH "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(librawspeed)

if(BUILD_TOOLS)
  add_subdirectory(utilities)
endif()
