include(CheckCXXCompilerFlagAndEnableIt)

if(UNIX OR APPLE)
  # want -Werror to be enabled automatically for me.
  # but on windows platform, there are warnings still
  add_definitions(-Werror)
endif()

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wall)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wformat)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wformat-security)

# cleanup this once we no longer need to support gcc-4.9
# disabled for now, see https://github.com/darktable-org/rawspeed/issues/32
if(CMAKE_CXX_COMPILER_ID AND NOT (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0))
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wshadow)
endif()

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wtype-limits)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wvla)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wold-style-declaration)

# should be < 64Kb
# FIXME: 4K
math(EXPR MAX_MEANINGFUL_SIZE 8*1024)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wframe-larger-than=${MAX_MEANINGFUL_SIZE})
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wstack-usage=${MAX_MEANINGFUL_SIZE})

# as small as possible, but 1Mb+ is ok.
math(EXPR MAX_MEANINGFUL_SIZE 16*1024)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wlarger-than=${MAX_MEANINGFUL_SIZE})
