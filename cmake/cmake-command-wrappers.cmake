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
