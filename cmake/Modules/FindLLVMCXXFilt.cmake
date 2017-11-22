find_program(LLVMCXXFilt_EXECUTABLE
  NAMES llvm-cxxfilt llvm-cxxfilt-6.0 llvm-cxxfilt-5.0 llvm-cxxfilt-4.0
  DOC "The llvm-cxxfilt executable"
  )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVMCXXFilt
  DEFAULT_MSG
  LLVMCXXFilt_EXECUTABLE)

add_executable(llvm-cxxfilt IMPORTED GLOBAL)
set_property(TARGET llvm-cxxfilt PROPERTY IMPORTED_LOCATION "${LLVMCXXFilt_EXECUTABLE}")

SET_PACKAGE_PROPERTIES(LLVMCXXFilt PROPERTIES
  DESCRIPTION "LLVM demangler for C++ symbols"
  PURPOSE "Used for demangling of symbols in llvm-cov reports"
)
