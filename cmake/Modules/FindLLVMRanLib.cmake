find_program(LLVMRANLIB_EXECUTABLE NAMES llvm-ranlib llvm-ranlib-7 llvm-ranlib-6.0 llvm-ranlib-5.0 llvm-ranlib-4.0 llvm-ranlib-3.9)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMRanLib
  DEFAULT_MSG
  LLVMRANLIB_EXECUTABLE)

SET_PACKAGE_PROPERTIES(LLVMRanLib PROPERTIES
  DESCRIPTION "generate index for LLVM archive"
)
