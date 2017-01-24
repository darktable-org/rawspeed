add_executable(rstest rstest.cpp md5.cpp)
set_target_properties(rstest PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(rstest rawspeed_test)

if(BUILD_TESTING)
  add_test(NAME utilities/rstest COMMAND rstest)
endif()
