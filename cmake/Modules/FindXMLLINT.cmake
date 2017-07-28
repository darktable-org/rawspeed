find_program(Xmllint_BIN xmllint)
find_package_handle_standard_args(XMLLINT
  DEFAULT_MSG
  Xmllint_BIN)

SET_PACKAGE_PROPERTIES(XMLLINT PROPERTIES
  URL http://xmlsoft.org/
  DESCRIPTION "command line XML tool"
  PURPOSE "Used for validation of data/cameras.xml"
)
