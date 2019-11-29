rawspeed_add_executable(rsbench main.cpp)

target_link_libraries(rsbench rawspeed)
target_link_libraries(rsbench rawspeed_bench)

if(TARGET RawSpeed::OpenMP_CXX)
  target_link_libraries(rsbench RawSpeed::OpenMP_CXX)
endif()

rawspeed_add_test(NAME utilities/rsbench COMMAND rsbench --help)

add_dependencies(benchmarks rsbench)
