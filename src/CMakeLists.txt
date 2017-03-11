include(src-dependencies)

include(compiler-warnings)

include(memory-align-alloc)
include(thread-local)

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/rawspeedconfig.h")
set(CONFIG_INCLUDE_PATH "${CMAKE_CURRENT_BINARY_DIR}")

add_subdirectory(librawspeed)

if(BUILD_TOOLS)
  add_subdirectory(utilities)
endif()

if(BUILD_FUZZERS)
  add_subdirectory(fuzzers)
endif()
