if(NOT BUILD_DOCS)
  return()
endif()

add_custom_target(docs)

message(STATUS "Looking for Doxygen")
message(STATUS "Looking for dot")
find_package(Doxygen REQUIRED)

if(NOT DOXYGEN_FOUND)
  message(SEND_ERROR "Looking for Doxygen - failed")
else()
  message(STATUS "Looking for Doxygen - found")
endif()

if(NOT DOXYGEN_DOT_FOUND)
  message(SEND_ERROR "Looking for dot - failed")
else()
  message(STATUS "Looking for dot - found")
endif()

if(NOT DOXYGEN_FOUND OR NOT DOXYGEN_DOT_FOUND)
  message(SEND_ERROR "Will not be able to build docs. You can pass -DBUILD_DOCS=OFF to disable build of documentation.")
  return()
else()
  message(STATUS "Will be able to build doxygen docs")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" @ONLY)

add_custom_target(doxygen
  "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
  COMMENT "Generating API documentation with Doxygen"
  VERBATIM
)

add_dependencies(docs doxygen)
