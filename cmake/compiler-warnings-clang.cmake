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
  "zero-as-null-pointer-constant" # temporary
)

set(CMAKE_REQUIRED_FLAGS_ORIG "${CMAKE_REQUIRED_FLAGS}")
set(CMAKE_REQUIRED_FLAGS "-c -Wunreachable-code -Werror=unreachable-code")
# see https://reviews.llvm.org/D25321
# see https://github.com/darktable-org/rawspeed/issues/104
CHECK_CXX_SOURCE_COMPILES(
  "void foo() {
  return;
  __builtin_unreachable();
}"
  CLANG_CXX_FLAG_UNREACHABLE_CODE_WORKS
)
set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS_ORIG}")

if(NOT CLANG_CXX_FLAG_UNREACHABLE_CODE_WORKS)
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "unreachable-code")
endif()

if(NOT (UNIX OR APPLE))
  # bogus warnings about std functions...
  list(APPEND CLANG_DISABLED_WARNING_FLAGS "used-but-marked-unused")
endif()

foreach(warning ${CLANG_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-W${warning})
endforeach()

foreach(warning ${CLANG_DISABLED_WARNING_FLAGS})
  CHECK_CXX_COMPILER_FLAG_AND_ENABLE_IT(-Wno-${warning})
endforeach()
