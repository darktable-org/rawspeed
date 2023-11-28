FILE(GLOB SOURCES
  "CiffParser.cpp"
  "CiffParser.h"
  "CiffParserException.h"
  "FiffParser.cpp"
  "FiffParser.h"
  "FiffParserException.h"
  "RawParser.cpp"
  "RawParser.h"
  "RawParserException.cpp"
  "RawParserException.h"
  "TiffParser.cpp"
  "TiffParser.h"
  "TiffParserException.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
