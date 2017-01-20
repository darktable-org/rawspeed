FILE(GLOB TIFF_SOURCES
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
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${TIFF_SOURCES}" PARENT_SCOPE)
