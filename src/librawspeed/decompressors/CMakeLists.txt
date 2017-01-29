FILE(GLOB DECOMPRESSOR_SOURCES
  "HasselbladDecompressor.cpp" # IWYU breaks
  "HasselbladDecompressor.h"
  "HuffmanTable.h"
  "LJpegDecompressor.cpp"
  "LJpegDecompressor.h"
  "LJpegPlain.cpp" # IWYU breaks
  "LJpegPlain.h"
  "NikonDecompressor.cpp"
  "NikonDecompressor.h"
  "PentaxDecompressor.cpp"
  "PentaxDecompressor.h"
  "UncompressedDecompressor.cpp"
  "UncompressedDecompressor.h"
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${DECOMPRESSOR_SOURCES}" PARENT_SCOPE)
