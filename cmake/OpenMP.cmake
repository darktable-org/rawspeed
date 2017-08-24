if(WITH_OPENMP)
  message(STATUS "Looking for OpenMP")
  find_package(OpenMP)
  if(OPENMP_FOUND)
    message(STATUS "Looking for OpenMP - found")

    if(NOT TARGET OpenMP::OpenMP)
      add_library(OpenMP::OpenMP INTERFACE IMPORTED)
      set_property(TARGET OpenMP::OpenMP PROPERTY INTERFACE_COMPILE_OPTIONS "${OpenMP_CXX_FLAGS}")
      set_property(TARGET OpenMP::OpenMP PROPERTY INTERFACE_LINK_LIBRARIES "${OpenMP_CXX_FLAGS}")
    endif()

    set_package_properties(OpenMP PROPERTIES
                           TYPE OPTIONAL
                           PURPOSE "Used for parallelization of tools (NOT library!)")
  else()
    message(WARNING "Looking for OpenMP - failed. utilities will not use openmp-based parallelization")
  endif()
else()
  message(STATUS "OpenMP is disabled, utilities will not use openmp-based parallelization")
endif()
