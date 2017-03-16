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

# as per http://lars-lab.jpl.nasa.gov/JPL_Coding_Standard_C.pdf
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wtraditional)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wshadow)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wpointer-arith)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wcast-qual)
# CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wcast-align)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wstrict-prototypes)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wmissing-prototypes)
# CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wconversion)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-unused-parameter)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-attribute=noreturn)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-attribute=const)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-attribute=pure)

if(UNIX OR APPLE)
  # on windows, resuts in bogus false-positive varnings
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-attribute=format)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wmissing-format-attribute)
endif()

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-final-types)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-final-methods)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wsuggest-override)

CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-error=suggest-final-types)
CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-error=suggest-final-methods)

if(NOT SPECIAL_BUILD)
  # should be < 64Kb
  math(EXPR MAX_MEANINGFUL_SIZE 4*1024)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wstack-usage=${MAX_MEANINGFUL_SIZE})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wframe-larger-than=${MAX_MEANINGFUL_SIZE})

  # as small as possible, but 1Mb+ is ok.
  math(EXPR MAX_MEANINGFUL_SIZE 32*1024)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wlarger-than=${MAX_MEANINGFUL_SIZE})
endif()
