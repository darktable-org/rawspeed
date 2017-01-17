cmake_minimum_required(VERSION 3.0)

project(googletest NONE)

# Download and unpack googletest at configure time
configure_file(${CMAKE_SOURCE_DIR}/cmake/Modules/GoogleTest.cmake.in ${CMAKE_BINARY_DIR}/googletest/CMakeLists.txt @ONLY)

execute_process(
  COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest
)

if(result)
  message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest
)

if(result)
  message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

if(NOT (UNIX OR APPLE))
  set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)
endif()

set(CMAKE_C_FLAGS_SAVE "${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS_SAVE "${CMAKE_CXX_FLAGS}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")

set(CMAKE_CXX_CLANG_TIDY_SAVE "${CMAKE_CXX_CLANG_TIDY}")
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE_SAVE "${CMAKE_CXX_INCLUDE_WHAT_YOU_USE}")

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_BINARY_DIR}/googletest/googletest-src
                 ${CMAKE_BINARY_DIR}/googletest/googletest-build)

set_target_properties(gtest PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:gtest,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(gtest_main PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:gtest_main,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(gmock PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:gmock,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(gmock_main PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:gmock_main,INTERFACE_INCLUDE_DIRECTORIES>)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS_SAVE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_SAVE}")

set(CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY_SAVE}")
set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${CMAKE_CXX_INCLUDE_WHAT_YOU_USE_SAVE}")
