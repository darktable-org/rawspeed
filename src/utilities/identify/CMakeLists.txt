set(rsidentify "rs-identify")
if(DEFINED RAWSPEED_BINARY_PREFIX)
  set(rsidentify "${RAWSPEED_BINARY_PREFIX}-${rsidentify}")
endif()

rawspeed_add_executable(${rsidentify} rawspeed-identify.cpp)
target_compile_definitions(${rsidentify}
  PRIVATE -DRS_CAMERAS_XML_PATH="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/rawspeed/cameras.xml"
)

target_link_libraries(${rsidentify} rawspeed)

if(TARGET RawSpeed::OpenMP_CXX)
  target_link_libraries(${rsidentify} RawSpeed::OpenMP_CXX)
endif()

if(BUILD_TESTING)
  rawspeed_add_test(NAME utilities/${rsidentify} COMMAND ${rsidentify})
endif()

install(TARGETS ${rsidentify} DESTINATION ${CMAKE_INSTALL_BINDIR})
