set(rsidentify "rs-identify")
if(DEFINED RAWSPEED_BINARY_PREFIX)
  set(rsidentify "${RAWSPEED_BINARY_PREFIX}-${rsidentify}")
endif()

add_executable(${rsidentify} rawspeed-identify.cpp)
target_compile_definitions(${rsidentify}
  PRIVATE -DRS_CAMERAS_XML_PATH="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/rawspeed/cameras.xml"
)

set_target_properties(${rsidentify} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${rsidentify} rawspeed_static)
target_include_directories(${rsidentify} PUBLIC "${CONFIG_INCLUDE_PATH}")

if(BUILD_TESTING)
  add_test(NAME utilities/${rsidentify} COMMAND ${rsidentify})
endif()

install(TARGETS ${rsidentify} DESTINATION ${CMAKE_INSTALL_BINDIR})
