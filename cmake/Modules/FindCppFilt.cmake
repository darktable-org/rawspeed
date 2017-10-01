find_program(CPPFILT_PATH NAMES c++filt)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CppFilt
  DEFAULT_MSG
  CPPFILT_PATH)

SET_PACKAGE_PROPERTIES(CppFilt PROPERTIES
  URL https://sourceware.org/binutils/docs/binutils/c_002b_002bfilt.html
  DESCRIPTION "Demangler for C++ symbols"
  PURPOSE "Used for demangling of symbols in llvm-cov reports"
)
