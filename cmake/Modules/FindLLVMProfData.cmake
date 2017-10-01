find_program(LLVMPROFDATA_PATH NAMES llvm-profdata)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMProfData
  DEFAULT_MSG
  LLVMPROFDATA_PATH)

SET_PACKAGE_PROPERTIES(LLVMProfData PROPERTIES
  URL https://llvm.org/docs/CommandGuide/llvm-profdata.html
  DESCRIPTION "Profile data tool"
  PURPOSE "Used for preprocessing *.profraw files into *.profdata"
)
