if(USE_XMLLINT)
  find_package(XMLLINT)
endif()

if(USE_XMLLINT AND XMLLINT_FOUND)
  include(run-xmllint)
  check_xml(${CMAKE_CURRENT_SOURCE_DIR}/cameras.xml ${CMAKE_CURRENT_SOURCE_DIR}/cameras.xsd)
endif()

if(WITH_PUGIXML)
  install(FILES cameras.xml showcameras.xsl DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/rawspeed)
endif()
