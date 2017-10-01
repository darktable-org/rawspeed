find_program(LCOV_PATH NAMES lcov)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LCov
  DEFAULT_MSG
  LCOV_PATH)

SET_PACKAGE_PROPERTIES(LCov PROPERTIES
  URL http://ltp.sourceforge.net/coverage/lcov.php
  DESCRIPTION "A graphical GCOV front-end"
  PURPOSE "Used for collection of line coverage info"
)
