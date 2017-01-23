set(IWYU_IMP "${CMAKE_SOURCE_DIR}/cmake/iwyu.imp")

find_program(iwyu_path NAMES include-what-you-use iwyu)
if(NOT iwyu_path)
  message(FATAL_ERROR "Could not find the program include-what-you-use.")
else(NOT iwyu_path)
  set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${iwyu_path}" -Xiwyu --mapping_file=${IWYU_IMP})
endif()

find_program(iwyu_tool_path NAMES iwyu_tool iwyu_tool.py)
if(iwyu_tool_path)
  add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/iwyu.log"
    COMMAND "${iwyu_tool_path}" -v -p "${CMAKE_BINARY_DIR}"
            -- --mapping_file=${IWYU_IMP} 2>
            "${CMAKE_BINARY_DIR}/iwyu.log"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running include-what-you-use tool"
    VERBATIM
  )
  add_custom_target(iwyu
    DEPENDS "${CMAKE_BINARY_DIR}/iwyu.log"
    VERBATIM
  )
endif()

find_program(fix_includes_path NAMES fix_include fix_includes.py)
if(fix_includes_path)
  add_custom_target(iwyu_fix
    COMMAND "${fix_includes_path}" --noblank_lines --comments
            --safe_headers < "${CMAKE_BINARY_DIR}/iwyu.log" || true
    COMMAND ${CMAKE_COMMAND} -E remove "${CMAKE_BINARY_DIR}/iwyu.log"
    DEPENDS "${CMAKE_BINARY_DIR}/iwyu.log"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running include-what-you-use fix_includes tool"
    VERBATIM
  )
endif()

unset(IWYU_IMP)
