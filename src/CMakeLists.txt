add_library(rawspeed STATIC)

include(src-dependencies)

include(memory-align-alloc)
include(thread-local)

CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/rawspeedconfig.h")
target_include_directories(rawspeed PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(rawspeed SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/")

add_subdirectory(librawspeed)

if(BUILD_TOOLS)
  add_subdirectory(utilities)
endif()

install(TARGETS rawspeed DESTINATION lib)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/librawspeed" DESTINATION include
        FILES_MATCHING PATTERN "*.h")

