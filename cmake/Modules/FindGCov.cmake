if(DEFINED GCOV_PATH)
  return()
endif()

message(STATUS "Looking for gcov tool")

if(DEFINED ENV{GCOV})
  find_program(GCOV_PATH NAMES "$ENV{GCOV}")
else()
  find_program(GCOV_PATH NAMES gcov)
endif()

if(NOT GCOV_PATH)
  message(FATAL_ERROR "Looking for gcov tool - not found")
else()
  message(STATUS "Looking for gcov - found (${GCOV_PATH})")
endif()
