find_program(LLVMCOV_PATH NAMES llvm-cov llvm-cov-7 llvm-cov-6.0 llvm-cov-5.0 llvm-cov-4.0 llvm-cov-3.9)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMCov
  DEFAULT_MSG
  LLVMCOV_PATH)

SET_PACKAGE_PROPERTIES(LLVMCov PROPERTIES
  URL https://llvm.org/docs/CommandGuide/llvm-cov.html
  DESCRIPTION "Tool to show code coverage information"
  PURPOSE "Used for rendering *.profdata into HTML coverage report"
)
