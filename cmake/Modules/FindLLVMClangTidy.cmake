find_program(CLANGTIDY_PATH NAMES "$ENV{CLANG_TIDY}" clang-tidy)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMClangTidy
  DEFAULT_MSG
  CLANGTIDY_PATH)

SET_PACKAGE_PROPERTIES(LLVMClangTidy PROPERTIES
  URL https://clang.llvm.org/extra/clang-tidy/
  DESCRIPTION "a clang-based C++ “linter” tool"
  PURPOSE "Used for enforcing some quality level for the source code"
)
