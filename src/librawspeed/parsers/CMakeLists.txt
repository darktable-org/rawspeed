rawspeed_add_library(rawspeed_parsers OBJECT)
set_target_properties(rawspeed_parsers PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "CiffParser.cpp"
  "CiffParser.h"
  "CiffParserException.cpp"
  "CiffParserException.h"
  "FiffParser.cpp"
  "FiffParser.h"
  "FiffParserException.cpp"
  "FiffParserException.h"
  "RawParser.cpp"
  "RawParser.h"
  "RawParserException.cpp"
  "RawParserException.h"
  "TiffParser.cpp"
  "TiffParser.h"
  "TiffParserException.cpp"
  "TiffParserException.h"
)

target_sources(rawspeed_parsers PRIVATE
  ${SOURCES}
)

target_include_directories(rawspeed_parsers PUBLIC "${RAWSPEED_BINARY_DIR}/src")
target_include_directories(rawspeed_parsers SYSTEM PUBLIC "${RAWSPEED_SOURCE_DIR}/src/external")
target_include_directories(rawspeed_parsers PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(TARGET RawSpeed::OpenMP_CXX)
  target_link_libraries(rawspeed_parsers PUBLIC RawSpeed::OpenMP_CXX)
endif()

target_link_libraries(rawspeed PRIVATE rawspeed_parsers)
