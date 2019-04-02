# Creates a new executable build target. Use this instead of `add_executable`.
if(NOT COMMAND rawspeed_add_executable)
  function(rawspeed_add_executable target)
    add_executable(${target} ${ARGN})
  endfunction()
endif() # NOT COMMAND rawspeed_add_executable

# Creates a new library build target. Use this instead of `add_library`.
if(NOT COMMAND rawspeed_add_library)
  function(rawspeed_add_library target)
    add_library(${target} ${ARGN})
  endfunction()
endif() # NOT COMMAND rawspeed_add_library

# Creates test. Use this instead of `add_test`.
# WARNING: do NOT create multiple tests for same COMMAND that only differ in arg
if(NOT COMMAND rawspeed_add_test)
  function(rawspeed_add_test)
    add_test(${ARGN})
  endfunction()
endif() # NOT COMMAND rawspeed_add_test
