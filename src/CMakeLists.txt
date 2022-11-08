include(memory-align-alloc)
include(thread-local)
include(cpu-cache-line-size)
include(cpu-page-size)
include(cpu-large-page-size)

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/rawspeedconfig.h")
target_include_directories(rawspeed PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(external)

add_subdirectory(librawspeed)

if(BUILD_TOOLS)
  add_subdirectory(utilities)
endif()
