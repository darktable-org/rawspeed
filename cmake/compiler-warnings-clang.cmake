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
  "c99-extensions"
  "conversion"
  "covered-switch-default"
  "deprecated"
  "double-promotion"
  "embedded-directive"
  "exit-time-destructors"
  "float-equal"
  "global-constructors"
  "gnu-zero-variadic-macro-arguments"
  "missing-noreturn"
  "missing-variable-declarations"
  "old-style-cast"
  "padded"
  "sign-conversion"
  "switch-enum"
  "undefined-func-template"
  "unreachable-code"
  "unreachable-code-break"
  "unreachable-code-return"
  "unused-exception-parameter"
  "unused-macros"
  "unused-parameter"
  "used-but-marked-unused"
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
