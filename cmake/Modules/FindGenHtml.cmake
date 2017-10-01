find_program(GENHTML_PATH NAMES genhtml)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GenHtml
  DEFAULT_MSG
  GENHTML_PATH)

SET_PACKAGE_PROPERTIES(GenHtml PROPERTIES
  URL http://ltp.sourceforge.net/coverage/lcov/genhtml.1.php
  DESCRIPTION " Generates HTML view from LCOV coverage data files"
  PURPOSE "Used for final rendering coverage reports into HTML"
)
