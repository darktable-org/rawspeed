find_program(FIND_PATH NAMES find)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Find
  DEFAULT_MSG
  FIND_PATH)

SET_PACKAGE_PROPERTIES(Find PROPERTIES
  URL https://www.gnu.org/software/findutils/
  DESCRIPTION "Search for files in a directory hierarchy"
  PURPOSE "Used to find specific files at cmake build execution time"
)
