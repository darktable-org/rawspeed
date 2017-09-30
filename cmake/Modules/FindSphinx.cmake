find_program(SPHINX_BUILD_PATH NAMES sphinx-build)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sphinx
  DEFAULT_MSG
  SPHINX_BUILD_PATH)

SET_PACKAGE_PROPERTIES(Sphinx PROPERTIES
  URL http://www.sphinx-doc.org/
  DESCRIPTION "Documentation generator"
  PURPOSE "Used for generating the textual documentation, used on web-site"
)
