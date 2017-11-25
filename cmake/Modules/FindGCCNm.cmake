find_program(GCCNM_EXECUTABLE NAMES gcc-nm)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GCCNm
  DEFAULT_MSG
  GCCNM_EXECUTABLE)

SET_PACKAGE_PROPERTIES(GCCNm PROPERTIES
  URL https://sourceware.org/binutils/docs/binutils/nm.html
  DESCRIPTION "list GCC bitcode and object file’s symbol table"
  PURPOSE "A wrapper around ar adding the --plugin option"
)
