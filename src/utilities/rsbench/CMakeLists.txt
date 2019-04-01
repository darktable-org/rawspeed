rawspeed_add_executable(rsbench main.cpp)

target_link_libraries(rsbench rawspeed)
target_link_libraries(rsbench rawspeed_bench)

add_dependencies(benchmarks rsbench)
