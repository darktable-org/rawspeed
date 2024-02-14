if(RAWSPEED_COVERAGE_BUILD)
  # want all the symbols.
  rawspeed_add_library(rawspeed_test SHARED)
else()
  rawspeed_add_library(rawspeed_test STATIC)
endif()

target_link_libraries(rawspeed_test PUBLIC rawspeed)
target_link_libraries(rawspeed_test PUBLIC gtest gmock_main)
target_include_directories(rawspeed_test PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

function(add_rs_test src)
  get_filename_component(TESTNAME ${src} NAME_WE)
  rawspeed_add_executable(${TESTNAME} ${src})
  target_link_libraries(${TESTNAME} rawspeed)
  target_link_libraries(${TESTNAME} rawspeed_test)
  rawspeed_add_test(NAME ${TESTNAME}
    COMMAND ${TESTNAME}
    WORKING_DIRECTORY "$<TARGET_PROPERTY:rawspeed_test,BINARY_DIR>")
  set_tests_properties(${TESTNAME} PROPERTIES LABELS "unittest")
  add_dependencies(tests ${TESTNAME})
endfunction()

add_subdirectory(adt)
add_subdirectory(bitstreams)
add_subdirectory(codes)
add_subdirectory(common)
add_subdirectory(io)
add_subdirectory(metadata)
add_subdirectory(test)
