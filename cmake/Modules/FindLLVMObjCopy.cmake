find_program(LLVMOBJCOPY_EXECUTABLE NAMES llvm-objcopy llvm-objcopy-8 llvm-objcopy-7 llvm-objcopy-6.0 llvm-objcopy-5.0 llvm-objcopy-4.0 llvm-objcopy-3.9)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMObjCopy
  DEFAULT_MSG
  LLVMOBJCOPY_EXECUTABLE)

SET_PACKAGE_PROPERTIES(LLVMObjCopy PROPERTIES
  DESCRIPTION "llvm objcopy utility"
)
