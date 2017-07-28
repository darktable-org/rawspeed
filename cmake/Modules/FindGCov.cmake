if(DEFINED ENV{GCOV})
  find_program(GCOV_PATH NAMES "$ENV{GCOV}")
else()
  find_program(GCOV_PATH NAMES gcov)
endif()

find_package_handle_standard_args(GCov
  DEFAULT_MSG
  GCOV_PATH)

SET_PACKAGE_PROPERTIES(GCov PROPERTIES
  URL https://gcc.gnu.org/onlinedocs/gcc/Gcov.html
  DESCRIPTION "Coverage testing tool"
  PURPOSE "Used for preprocessing *.gcno files into *.gcov"
)
