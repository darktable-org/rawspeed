add_executable(rsbench main.cpp)
target_link_libraries(rsbench rawspeed)
target_link_libraries(rsbench rawspeed_benchmark)
add_dependencies(benchmarks rsbench)
