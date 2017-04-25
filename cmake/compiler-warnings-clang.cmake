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
  "conversion"
  "covered-switch-default"
  "deprecated"
  "double-promotion"
  "exit-time-destructors"
  "global-constructors"
  "gnu-zero-variadic-macro-arguments"
  "old-style-cast"
  "padded"
  "sign-conversion"
  "switch-enum"
  "undefined-func-template"
  "unused-macros"
  "unused-parameter"
  "weak-vtables"
)

if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "unreachable-code")
endif()

foreach(warning ${CLANG_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-W${warning})
endforeach()

foreach(warning ${CLANG_DISABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-${warning})
endforeach()
