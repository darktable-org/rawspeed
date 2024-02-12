if(DEFINED RAWSPEED_PAGESIZE)
  return()
endif()

message(STATUS "Trying to query CPU page size")

if(BINARY_PACKAGE_BUILD)
  message(STATUS "Performing binary package build, using hardcoded value.")
  set(RAWSPEED_PAGESIZE 4096)
else()
  try_run(RAWSPEED_PAGESIZE_EXITCODE RAWSPEED_PAGESIZE_COMPILED
    "${CMAKE_BINARY_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/cpu-page-size.cpp"
    COMPILE_OUTPUT_VARIABLE  RAWSPEED_PAGESIZE_COMPILE_OUTPUT
    RUN_OUTPUT_VARIABLE RAWSPEED_PAGESIZE_RUN_OUTPUT)

  if(NOT RAWSPEED_PAGESIZE_COMPILED OR NOT RAWSPEED_PAGESIZE_EXITCODE EQUAL 0)
    message(SEND_ERROR "Failed to query CPU page size:\n${RAWSPEED_PAGESIZE_COMPILE_OUTPUT}\n${RAWSPEED_PAGESIZE_RUN_OUTPUT}")
    return()
  endif()

  string(STRIP "${RAWSPEED_PAGESIZE_RUN_OUTPUT}" RAWSPEED_PAGESIZE)
endif()

message(STATUS "Deciding that the CPU page size is ${RAWSPEED_PAGESIZE} bytes")

if(RAWSPEED_PAGESIZE EQUAL 0)
  unset(RAWSPEED_PAGESIZE)
  message(SEND_ERROR "Detected page size is zero!")
endif()

set(RAWSPEED_PAGESIZE ${RAWSPEED_PAGESIZE} CACHE INTERNAL "")
