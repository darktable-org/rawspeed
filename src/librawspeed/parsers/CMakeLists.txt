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
  "TiffParserException.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
