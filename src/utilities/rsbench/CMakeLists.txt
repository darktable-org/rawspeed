rawspeed_add_executable(rsbench main.cpp)

target_link_libraries(rsbench rawspeed)
target_link_libraries(rsbench rawspeed_bench)

rawspeed_add_test(NAME utilities/rsbench COMMAND rsbench --help)
set_tests_properties(utilities/rsbench PROPERTIES LABELS "benchmark;dummy")

add_dependencies(benchmarks rsbench)
