# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#.rst:
# FindGTest
# ---------
#
# Locate the Google C++ Testing Framework.
#
# Deeper integration with CTest
# ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#
# If you would like each Google test to show up in CTest as a test you
# may use the following macro::
#
#     GTEST_ADD_TESTS(executable extra_args files...)
#
# ``executable``
#   the path to the test executable
# ``extra_args``
#   a list of extra arguments to be passed to executable enclosed in
#   quotes (or ``""`` for none)
# ``files...``
#   a list of source files to search for tests and test fixtures.  Or
#   ``AUTO`` to find them from executable target
#
# However, note that this macro will slow down your tests by running
# an executable for each test and test fixture.
#
# Example usage::
#
#      set(FooTestArgs --foo 1 --bar 2)
#      add_executable(FooTest FooUnitTest.cc)
#      GTEST_ADD_TESTS(FooTest "${FooTestArgs}" AUTO)

#
# Thanks to Daniel Blezek <blezek@gmail.com> for the GTEST_ADD_TESTS code

function(GTEST_ADD_TESTS executable extra_args)
    if(NOT ARGN)
        message(FATAL_ERROR "Missing ARGN: Read the documentation for GTEST_ADD_TESTS")
    endif()
    if(ARGN STREQUAL "AUTO")
        # obtain sources used for building that executable
        get_property(ARGN TARGET ${executable} PROPERTY SOURCES)
    endif()
    set(gtest_case_name_regex ".*\\( *([A-Za-z_0-9]+) *, *([A-Za-z_0-9]+) *\\).*")
    set(gtest_test_type_regex "(TYPED_TEST|TEST_?[FP]?)")
    foreach(source ${ARGN})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${source})
        file(READ "${source}" contents)
        string(REGEX MATCHALL "${gtest_test_type_regex} *\\(([A-Za-z_0-9 ,]+)\\)" found_tests ${contents})
        foreach(hit ${found_tests})
          string(REGEX MATCH "${gtest_test_type_regex}" test_type ${hit})

          # Parameterized tests have a different signature for the filter
          if("x${test_type}" STREQUAL "xTEST_P")
            string(REGEX REPLACE ${gtest_case_name_regex}  "*/\\1.\\2/*" test_name ${hit})
          elseif("x${test_type}" STREQUAL "xTEST_F" OR "x${test_type}" STREQUAL "xTEST")
            string(REGEX REPLACE ${gtest_case_name_regex} "\\1.\\2" test_name ${hit})
          elseif("x${test_type}" STREQUAL "xTYPED_TEST")
            string(REGEX REPLACE ${gtest_case_name_regex} "\\1/*.\\2" test_name ${hit})
          else()
            message(WARNING "Could not parse GTest ${hit} for adding to CTest.")
            continue()
          endif()
          add_test(NAME ${test_name} COMMAND ${executable} --gtest_filter=${test_name} ${extra_args})
        endforeach()
    endforeach()
endfunction()
