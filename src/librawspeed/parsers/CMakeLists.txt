FILE(GLOB SOURCES
  "CiffParser.cpp"
  "CiffParser.h"
  "CiffParserException.h"
  "FiffParser.cpp"
  "FiffParser.h"
  "FiffParserException.h"
  "IsoMParser.cpp"
  "IsoMParser.h"
  "IsoMParserException.h"
  "RawParser.cpp"
  "RawParser.h"
  "RawParserException.h"
  "TiffParser.cpp"
  "TiffParser.h"
  "TiffParserException.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
