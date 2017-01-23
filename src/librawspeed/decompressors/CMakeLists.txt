FILE(GLOB DECOMPRESSOR_SOURCES
  "HasselbladDecompressor.cpp" # IWYU breaks
  "HasselbladDecompressor.h"
  "LJpegDecompressor.cpp"
  "LJpegDecompressor.h"
  "NikonDecompressor.cpp"
  "NikonDecompressor.h"
  "PentaxDecompressor.cpp"
  "PentaxDecompressor.h"
  "LJpegPlain.cpp" # IWYU breaks
  "LJpegPlain.h"
  "HuffmanTable.h"
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${DECOMPRESSOR_SOURCES}" PARENT_SCOPE)
