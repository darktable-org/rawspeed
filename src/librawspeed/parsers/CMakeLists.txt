FILE(GLOB PARSER_SOURCES
  "CiffParser.cpp"
  "CiffParser.h"
  "CiffParserException.h"
  "RawParser.cpp"
  "RawParser.h"
  "TiffParser.cpp"
  "TiffParser.h"
  "TiffParserException.h"
  "FiffParser.cpp"
  "FiffParser.h"
  "FiffParserException.h"
  "X3fParser.cpp"
  "X3fParser.h"
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${PARSER_SOURCES}" PARENT_SCOPE)
