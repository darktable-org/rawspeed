FILE(GLOB SOURCES
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
  "X3fParserException.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
