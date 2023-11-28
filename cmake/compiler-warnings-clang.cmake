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
  "double-promotion"
  "gnu-zero-variadic-macro-arguments"
  "old-style-cast"
  "padded"
  "switch-enum"
  "unused-parameter"
  "unsafe-buffer-usage" # FIXME: really want this. to be reenabled.
  "sign-conversion" # FIXME: should enable this.
)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  list (APPEND CLANG_DISABLED_WARNING_FLAGS
    "shorten-64-to-32"
  )
endif()

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

foreach(warning ${CLANG_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-W${warning})
endforeach()

foreach(warning ${CLANG_DISABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-${warning})
endforeach()

foreach(warning ${CLANG_REENABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-W${warning})
endforeach()
