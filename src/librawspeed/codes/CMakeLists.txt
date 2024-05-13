rawspeed_add_library(rawspeed_codes OBJECT)
set_target_properties(rawspeed_codes PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "AbstractPrefixCode.h"
  "AbstractPrefixCodeDecoder.h"
  "AbstractPrefixCodeEncoder.h"
  "AbstractPrefixCodeTranscoder.h"
  "BinaryPrefixTree.h"
  "DummyPrefixCodeDecoder.h"
  "HuffmanCode.h"
  "PrefixCode.h"
  "PrefixCodeDecoder.h"
  "PrefixCodeLUTDecoder.h"
  "PrefixCodeLookupDecoder.h"
  "PrefixCodeTreeDecoder.h"
  "PrefixCodeVectorDecoder.h"
  "PrefixCodeVectorEncoder.h"
)

target_sources(rawspeed_codes PRIVATE
  ${SOURCES}
)

target_link_libraries(rawspeed PRIVATE rawspeed_codes)
