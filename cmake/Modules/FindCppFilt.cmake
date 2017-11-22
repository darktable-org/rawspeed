find_program(CPPFILT_EXECUTABLE
  NAMES c++filt
  DOC "The c++filt executable"
  )

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CppFilt
  DEFAULT_MSG
  CPPFILT_EXECUTABLE)

add_executable(c++filt IMPORTED GLOBAL)
set_property(TARGET c++filt PROPERTY IMPORTED_LOCATION "${CPPFILT_EXECUTABLE}")

SET_PACKAGE_PROPERTIES(CppFilt PROPERTIES
  URL https://sourceware.org/binutils/docs/binutils/c_002b_002bfilt.html
  DESCRIPTION "Demangler for C++ symbols"
  PURPOSE "Used for demangling of symbols in llvm-cov reports"
)
