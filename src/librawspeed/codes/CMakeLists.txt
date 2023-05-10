FILE(GLOB SOURCES
  "AbstractPrefixCode.h"
  "AbstractPrefixCodeDecoder.h"
  "BinaryPrefixTree.h"
  "DummyPrefixCodeDecoder.h"
  "HuffmanCode.h"
  "PrefixCode.h"
  "PrefixCodeDecoder.h"
  "PrefixCodeLUTDecoder.h"
  "PrefixCodeLookupDecoder.h"
  "PrefixCodeTreeDecoder.h"
  "PrefixCodeVectorDecoder.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
