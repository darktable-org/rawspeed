rawspeed_add_executable(rsbench main.cpp)

target_link_libraries(rsbench rawspeed)
target_link_libraries(rsbench rawspeed_bench)

rawspeed_add_test(NAME utilities/rsbench COMMAND rsbench --help)
set_tests_properties(utilities/rsbench PROPERTIES LABELS "benchmark;dummy")
if(WIN32)
  set_tests_properties(utilities/rsbench PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:rawspeed_bench>")
endif()

add_dependencies(benchmarks rsbench)
