include(CheckCXXCompilerFlagAndEnableIt)

# want -Werror to be enabled automatically for me.
add_definitions(-Werror)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wall)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wformat=2)

if(NOT (UNIX OR APPLE))
  # on windows, resuts in bogus false-positive varnings
  add_definitions(-Wno-format)
endif()

# cleanup this once we no longer need to support gcc-4.9
# disabled for now, see https://github.com/darktable-org/rawspeed/issues/32
if(CMAKE_CXX_COMPILER_ID AND NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0))
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wshadow)
endif()

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wvla)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wextra-semi)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wextra)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-unused-parameter)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-attribute=noreturn)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-attribute=const)
# CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-attribute=format)
# CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wmissing-format-attribute)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-final-types)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-final-methods)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-error=suggest-final-types)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-error=suggest-final-methods)

# should be < 64Kb
math(EXPR MAX_MEANINGFUL_SIZE 4*1024)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wstack-usage=${MAX_MEANINGFUL_SIZE})

if(NOT (APPLE AND (CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang") AND CMAKE_BUILD_TYPE MATCHES "^[Cc][Oo][Vv][Ee][Rr][Aa][Gg][Ee]$"))
  # Apple XCode seems to generate HUGE stack/frames, much bigger than anything else.
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wframe-larger-than=${MAX_MEANINGFUL_SIZE})
endif()

# as small as possible, but 1Mb+ is ok.
math(EXPR MAX_MEANINGFUL_SIZE 32*1024)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wlarger-than=${MAX_MEANINGFUL_SIZE})
