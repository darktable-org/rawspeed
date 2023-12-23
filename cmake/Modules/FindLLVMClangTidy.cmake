find_program(CLANGTIDY_PATH NAMES "$ENV{CLANG_TIDY}" clang-tidy)

if(CLANGTIDY_PATH)
  execute_process(COMMAND "${CLANGTIDY_PATH}" --version OUTPUT_VARIABLE CLANG_TIDY_VERSION)
  string(REGEX MATCH "(([0-9]+)\\.([0-9]+)\\.([0-9]+))"
         CLANG_TIDY_VERSION ${CLANG_TIDY_VERSION})
  set(CLANG_TIDY_VERSION_MAJOR ${CMAKE_MATCH_2})
  set(CLANG_TIDY_VERSION_MINOR ${CMAKE_MATCH_3})
  set(CLANG_TIDY_VERSION_PATCH ${CMAKE_MATCH_4})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMClangTidy
  REQUIRED_VARS CLANGTIDY_PATH CLANG_TIDY_VERSION
  VERSION_VAR CLANG_TIDY_VERSION)

SET_PACKAGE_PROPERTIES(LLVMClangTidy PROPERTIES
  URL https://clang.llvm.org/extra/clang-tidy/
  DESCRIPTION "a clang-based C++ “linter” tool"
  PURPOSE "Used for enforcing some quality level for the source code"
)
