find_program(GCCAR_EXECUTABLE NAMES gcc-ar)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GCCAr
  DEFAULT_MSG
  GCCAR_EXECUTABLE)

SET_PACKAGE_PROPERTIES(GCCAr PROPERTIES
  URL https://sourceware.org/binutils/docs/binutils/ar.html
  DESCRIPTION "create, modify, and extract from archives"
  PURPOSE "A wrapper around ar adding the --plugin option"
)
