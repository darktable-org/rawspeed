find_program(GCCRANLIB_EXECUTABLE NAMES gcc-ranlib)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GCCRanLib
  DEFAULT_MSG
  GCCRANLIB_EXECUTABLE)

SET_PACKAGE_PROPERTIES(GCCRanLib PROPERTIES
  URL https://sourceware.org/binutils/docs/binutils/ranlib.html
  DESCRIPTION "generate index for GCC archive"
  PURPOSE "A wrapper around ar adding the --plugin option"
)
