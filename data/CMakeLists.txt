if(USE_XMLLINT)
  find_program(Xmllint_BIN xmllint)
  if(${Xmllint_BIN} STREQUAL "Xmllint_BIN-NOTFOUND")
    message(SEND_ERROR "Missing xmllint")
    set(USE_XMLLINT OFF)
  else(${Xmllint_BIN} STREQUAL "Xmllint_BIN-NOTFOUND")
    mark_as_advanced(Xmllint_BIN)
    message(STATUS "Found xmllint")
  endif(${Xmllint_BIN} STREQUAL "Xmllint_BIN-NOTFOUND")
endif(USE_XMLLINT)

if(USE_XMLLINT)
  add_custom_target(
    validate_cameras_xml ALL
    COMMAND ${Xmllint_BIN} --nonet --valid --noout --schema ${CMAKE_CURRENT_SOURCE_DIR}/cameras.xsd ${CMAKE_CURRENT_SOURCE_DIR}/cameras.xml
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cameras.xml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking validity of data/cameras.xml"
  )
endif(USE_XMLLINT)

install(FILES cameras.xml showcameras.xsl DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/darktable/rawspeed)

if(USE_XMLLINT)
  add_dependencies(rawspeed validate_cameras_xml)
endif(USE_XMLLINT)
