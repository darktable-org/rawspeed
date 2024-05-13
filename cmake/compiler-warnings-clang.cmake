include(CheckCXXCompilerFlagAndEnableIt)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  return()
endif()

set (CLANG_WARNING_FLAGS
  "all"
  "extra"
  "everything"
)

set (CLANG_DISABLED_WARNING_FLAGS
  "c++98-compat"
  "c++98-compat-pedantic"
  "c++20-extensions"
  "padded"
  "switch-default"
  "switch-enum"
  "unused-parameter"
  "sign-conversion" # FIXME: should enable this.
)

# Yes, these have to be *re-enabled* after CLANG_DISABLED_WARNING_FLAGS.
set (CLANG_REENABLED_WARNING_FLAGS
  "extra-semi"
)

if(NOT (UNIX OR APPLE))
  # bogus warnings about std functions...
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "used-but-marked-unused")
  # just don't care.
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "nonportable-system-include-path")
endif()

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17)
  # Clang 16 used to point at the variable declaration in the diagnostics,
  # and not at the pointer arithmetic itself.
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "unsafe-buffer-usage")
endif()

foreach(warning ${CLANG_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-W${warning})
endforeach()

foreach(warning ${CLANG_DISABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-${warning})
endforeach()

foreach(warning ${CLANG_REENABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-W${warning})
endforeach()
