find_program(LLVMNM_EXECUTABLE NAMES llvm-nm llvm-nm-8 llvm-nm-7 llvm-nm-6.0 llvm-nm-5.0 llvm-nm-4.0 llvm-nm-3.9)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMNm
  DEFAULT_MSG
  LLVMNM_EXECUTABLE)

SET_PACKAGE_PROPERTIES(LLVMNm PROPERTIES
  URL https://llvm.org/docs/CommandGuide/llvm-nm.html
  DESCRIPTION "list LLVM bitcode and object file’s symbol table"
)
