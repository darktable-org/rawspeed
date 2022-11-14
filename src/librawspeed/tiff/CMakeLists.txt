FILE(GLOB SOURCES
  "TiffEntry.cpp"
  "TiffEntry.h"
  "TiffIFD.cpp"
  "TiffIFD.h"
  "TiffTag.h"
  "CiffEntry.cpp"
  "CiffEntry.h"
  "CiffIFD.cpp"
  "CiffIFD.h"
  "CiffTag.h"
  "IsoMBox.cpp"
  "IsoMBox.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
