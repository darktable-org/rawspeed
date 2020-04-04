find_program(LLVMPROFDATA_PATH NAMES llvm-profdata llvm-profdata-10 llvm-profdata-9 llvm-profdata-8 llvm-profdata-7 llvm-profdata-6.0 llvm-profdata-5.0 llvm-profdata-4.0 llvm-profdata-3.9)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMProfData
  DEFAULT_MSG
  LLVMPROFDATA_PATH)

SET_PACKAGE_PROPERTIES(LLVMProfData PROPERTIES
  URL https://llvm.org/docs/CommandGuide/llvm-profdata.html
  DESCRIPTION "Profile data tool"
  PURPOSE "Used for preprocessing *.profraw files into *.profdata"
)
