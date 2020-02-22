set (DISABLED_WARNING_FLAGS
  "missing-prototypes"
  "missing-variable-declarations"
  "suggest-attribute=const"
  "suggest-override"
  "used-but-marked-unused"
)

foreach(warning ${DISABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-${warning})
endforeach()

if(NOT RAWSPEED_SPECIAL_BUILD)
  # should be < 64Kb
  math(EXPR MAX_MEANINGFUL_SIZE 4*1024)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wstack-usage=${MAX_MEANINGFUL_SIZE})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wframe-larger-than=${MAX_MEANINGFUL_SIZE})

  # as small as possible, but 1Mb+ is ok.
  math(EXPR MAX_MEANINGFUL_SIZE 32*1024)
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wlarger-than=${MAX_MEANINGFUL_SIZE})
endif()

set(CMAKE_CXX_CLANG_TIDY_SAVE "${CMAKE_CXX_CLANG_TIDY}")

unset(CMAKE_CXX_CLANG_TIDY)

add_subdirectory(librawspeed)

set(CMAKE_CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY_SAVE}")
