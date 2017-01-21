add_executable(darktable-rs-identify rawspeed-identify.cpp)
target_compile_definitions(darktable-rs-identify
  PRIVATE -DRS_CAMERAS_XML_PATH="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/darktable/rawspeed/cameras.xml"
)

set_target_properties(darktable-rs-identify PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(darktable-rs-identify rawspeed_static)
target_include_directories(darktable-rs-identify PUBLIC "${CONFIG_INCLUDE_PATH}")

install(TARGETS darktable-rs-identify DESTINATION ${CMAKE_INSTALL_BINDIR})
