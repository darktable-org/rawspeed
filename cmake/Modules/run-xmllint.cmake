macro (check_xml XML XSD)
  get_filename_component(FILENAME ${XML} NAME)

  set(TOUCH "${CMAKE_CURRENT_BINARY_DIR}/${FILENAME}.touch")

  set(TMPNAME "validate-${FILENAME}")

  add_custom_command(
    OUTPUT  ${TOUCH}
    COMMAND ${Xmllint_BIN} --nonet --valid --noout --schema ${XSD} ${XML}
    COMMAND ${CMAKE_COMMAND} -E touch ${TOUCH} # will be empty!
    DEPENDS ${XML}
    COMMENT "Checking validity of ${FILENAME}"
  )

  add_custom_target(
    ${TMPNAME}
    DEPENDS ${TOUCH} # will be empty!
    DEPENDS ${XML}
  )

  add_dependencies(check ${TMPNAME})
  add_dependencies(rawspeed ${TMPNAME})
endmacro (check_xml)
