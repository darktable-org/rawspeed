add_executable(rsbench main.cpp)

target_link_libraries(rsbench rawspeed)
target_link_libraries(rsbench rawspeed_benchmark)

if(WITH_OPENMP AND OPENMP_FOUND AND TARGET OpenMP::OpenMP)
  target_link_libraries(rsbench OpenMP::OpenMP)
endif()

add_dependencies(benchmarks rsbench)
