include(CheckCXXCompilerFlagAndEnableIt)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  include(compiler-warnings-gcc)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  include(compiler-warnings-clang)
endif()

if(NOT (UNIX OR APPLE))
  # on windows, results in bogus false-positive warnings
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-format)
endif()

if(NOT SPECIAL_BUILD)
  # should be < 64Kb
  math(EXPR MAX_MEANINGFUL_SIZE 4*1024)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wstack-usage=${MAX_MEANINGFUL_SIZE})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wframe-larger-than=${MAX_MEANINGFUL_SIZE})

  # as small as possible, but 1Mb+ is ok.
  math(EXPR MAX_MEANINGFUL_SIZE 32*1024)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wlarger-than=${MAX_MEANINGFUL_SIZE})
endif()
