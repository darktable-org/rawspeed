FILE(GLOB SOURCES
  "AbstractDecompressor.h"
  "AbstractLJpegDecompressor.cpp"
  "AbstractLJpegDecompressor.h"
  "Cr2Decompressor.cpp"
  "Cr2Decompressor.h"
  "CrwDecompressor.cpp"
  "CrwDecompressor.h"
  "DeflateDecompressor.cpp"
  "DeflateDecompressor.h"
  "HasselbladDecompressor.cpp"
  "HasselbladDecompressor.h"
  "HuffmanTable.h"
  "JpegDecompressor.cpp"
  "JpegDecompressor.h"
  "LJpegDecompressor.cpp"
  "LJpegDecompressor.h"
  "NikonDecompressor.cpp"
  "NikonDecompressor.h"
  "PentaxDecompressor.cpp"
  "PentaxDecompressor.h"
  "UncompressedDecompressor.cpp"
  "UncompressedDecompressor.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
