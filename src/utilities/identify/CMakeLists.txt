set(rsidentify "rs-identify")
if(DEFINED RAWSPEED_BINARY_PREFIX)
  set(rsidentify "${RAWSPEED_BINARY_PREFIX}-${rsidentify}")
endif()

add_executable(${rsidentify} rawspeed-identify.cpp)
target_compile_definitions(${rsidentify}
  PRIVATE -DRS_CAMERAS_XML_PATH="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/rawspeed/cameras.xml"
)

target_link_libraries(${rsidentify} rawspeed)

if(WITH_OPENMP AND OPENMP_FOUND AND TARGET OpenMP::OpenMP)
  target_link_libraries(${rsidentify} OpenMP::OpenMP)
endif()

if(BUILD_TESTING)
  add_test(NAME utilities/${rsidentify} COMMAND ${rsidentify})
endif()

install(TARGETS ${rsidentify} DESTINATION ${CMAKE_INSTALL_BINDIR})
