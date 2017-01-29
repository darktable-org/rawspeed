FILE(GLOB PARSER_SOURCES
  "CiffParser.cpp"
  "CiffParser.h"
  "CiffParserException.cpp"
  "CiffParserException.h"
  "RawParser.cpp"
  "RawParser.h"
  "TiffParser.cpp"
  "TiffParser.h"
  "TiffParserException.cpp"
  "TiffParserException.h"
  "FiffParser.cpp"
  "FiffParser.h"
  "FiffParserException.cpp"
  "FiffParserException.h"
  "X3fParser.cpp"
  "X3fParser.h"
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${PARSER_SOURCES}" PARENT_SCOPE)
