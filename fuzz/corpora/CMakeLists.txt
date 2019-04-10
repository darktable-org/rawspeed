file(GLOB_RECURSE CORPORA_ROOTS FOLLOW_SYMLINKS "${CMAKE_CURRENT_SOURCE_DIR}/**/timestamp.txt")
foreach(TOP_CORPORA_DIR ${CORPORA_ROOTS})
  get_filename_component(TOP_CORPORA_DIRS "${TOP_CORPORA_DIR}" DIRECTORY)
  file(GLOB CORPORAS FOLLOW_SYMLINKS "${TOP_CORPORA_DIRS}/*")

  foreach(CORPORA ${CORPORAS})
    if(IS_DIRECTORY "${CORPORA}")
      get_filename_component(dirname "${CORPORA}" NAME)
      list(FIND ALL_FUZZERS "${dirname}" IsFuzzerName)

      if(NOT IsFuzzerName EQUAL -1)
        # get all the files
        file(GLOB_RECURSE CORPUSES RELATIVE "${CORPORA}" FOLLOW_SYMLINKS "${CORPORA}/*")

        set(test "corpora/${dirname}")
        rawspeed_add_test(NAME ${test}
          COMMAND ${dirname} ${CORPUSES}
          WORKING_DIRECTORY "${CORPORA}")
        set_tests_properties(${test} PROPERTIES DEPENDS ${dirname})
        set_tests_properties(${test} PROPERTIES TIMEOUT 300)
      endif()
    endif()
  endforeach()
endforeach()
