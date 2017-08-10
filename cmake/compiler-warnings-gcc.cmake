include(CheckCXXCompilerFlagAndEnableIt)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  return()
endif()

set (GCC_WARNING_FLAGS
  "all"
  "extra"

  "cast-qual"
  "extra"
  "extra-semi"
  "format=2"
  "missing-prototypes"
  "old-style-casts"
  "pointer-arith"
  "strict-prototypes"
  "suggest-attribute=const"
  "suggest-attribute=noreturn"
  "suggest-attribute=pure"
  "suggest-final-methods"
  "suggest-final-types"
  "suggest-override"
  "traditional"
  "vla"
# "cast-align"
# "conversion"
)

# cleanup this once we no longer need to support gcc-4.9
# disabled for now, see https://github.com/darktable-org/rawspeed/issues/32
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
  list(APPEND GCC_WARNING_FLAGS "shadow")
endif()

if(UNIX OR APPLE)
  list(APPEND GCC_WARNING_FLAGS
    "missing-format-attribute"
    "suggest-attribute=format"
  )
endif()

set (GCC_DISABLED_WARNING_FLAGS
  "unused-parameter"
)

set (GCC_NOERROR_WARNING_FLAGS
  "suggest-final-methods"
  "suggest-final-types"
  "suggest-override"
  "suggest-attribute=noreturn"
)

foreach(warning ${GCC_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-W${warning})
endforeach()

foreach(warning ${GCC_DISABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-${warning})
endforeach()

foreach(warning ${GCC_NOERROR_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-error=${warning})
endforeach()
