add_executable(rstest rstest.cpp md5.cpp)
set_target_properties(rstest PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(rstest rawspeed_test)

if(BUILD_TESTING)
  add_executable(md5 md5.cpp)
  target_compile_definitions(md5
    PRIVATE -DENABLE_SELFTEST
  )
  add_test(NAME utilities/rstest/md5 COMMAND md5)

  add_test(NAME utilities/rstest COMMAND rstest)
endif()
