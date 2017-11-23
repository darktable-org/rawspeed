find_program(XMLLINT_EXECUTABLE
  NAMES xmllint
  DOC "The xmllint executable")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(XMLLINT
  DEFAULT_MSG
  XMLLINT_EXECUTABLE)

add_executable(xmllint IMPORTED GLOBAL)
set_property(TARGET xmllint PROPERTY IMPORTED_LOCATION "${XMLLINT_EXECUTABLE}")

SET_PACKAGE_PROPERTIES(XMLLINT PROPERTIES
  URL http://xmlsoft.org/
  DESCRIPTION "command line XML tool"
  PURPOSE "Used for validation of data/cameras.xml"
)
