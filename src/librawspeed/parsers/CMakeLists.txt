FILE(GLOB PARSER_SOURCES
  "CiffParser.cpp"
  "CiffParser.h"
  "CiffParserException.h"
  "FiffParser.cpp"
  "FiffParser.h"
  "FiffParserException.h"
  "RawParser.cpp"
  "RawParser.h"
  "RawParserException.h"
  "TiffParser.cpp"
  "TiffParser.h"
  "TiffParserException.h"
  "X3fParser.cpp"
  "X3fParser.h"
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${PARSER_SOURCES}" PARENT_SCOPE)
