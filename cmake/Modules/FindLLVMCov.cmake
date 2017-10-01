find_program(LLVMCOV_PATH NAMES llvm-cov)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMCov
  DEFAULT_MSG
  LLVMCOV_PATH)

SET_PACKAGE_PROPERTIES(LLVMCov PROPERTIES
  URL https://llvm.org/docs/CommandGuide/llvm-cov.html
  DESCRIPTION "Tool to show code coverage information"
  PURPOSE "Used for rendering *.profdata into HTML coverage report"
)
