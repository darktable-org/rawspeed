if(NOT DEFINED RAWSPEED_PAGESIZE)
  message(FATAL_ERROR "Should first run CPU page size detection")
endif()

if(DEFINED RAWSPEED_LARGEPAGESIZE)
  return()
endif()

message(STATUS "Trying to query CPU large page size")

if(BINARY_PACKAGE_BUILD)
  message(STATUS "Performing binary package build, using hardcoded value.")
  set(RAWSPEED_LARGEPAGESIZE ${RAWSPEED_PAGESIZE})
else()
  try_run(RAWSPEED_LARGEPAGESIZE_EXITCODE RAWSPEED_LARGEPAGESIZE_COMPILED
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/cpu-large-page-size.cpp"
    COMPILE_DEFINITIONS -DRAWSPEED_PAGESIZE=${RAWSPEED_PAGESIZE}
    COMPILE_OUTPUT_VARIABLE  RAWSPEED_LARGEPAGESIZE_COMPILE_OUTPUT
    RUN_OUTPUT_VARIABLE RAWSPEED_LARGEPAGESIZE_RUN_OUTPUT)

  if(NOT RAWSPEED_LARGEPAGESIZE_COMPILED OR NOT RAWSPEED_LARGEPAGESIZE_EXITCODE EQUAL 0)
    message(SEND_ERROR "Failed to query CPU large page size:\n${RAWSPEED_LARGEPAGESIZE_COMPILE_OUTPUT}\n${RAWSPEED_LARGEPAGESIZE_RUN_OUTPUT}")
    return()
  endif()

  string(STRIP "${RAWSPEED_LARGEPAGESIZE_RUN_OUTPUT}" RAWSPEED_LARGEPAGESIZE)
endif()

message(STATUS "Deciding that the CPU large page size is ${RAWSPEED_LARGEPAGESIZE} bytes")

if(RAWSPEED_LARGEPAGESIZE EQUAL 0)
  unset(RAWSPEED_LARGEPAGESIZE)
  message(SEND_ERROR "Detected large page size is zero!")
endif()

set(RAWSPEED_LARGEPAGESIZE ${RAWSPEED_LARGEPAGESIZE} CACHE INTERNAL "")
