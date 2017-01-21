FILE(GLOB IO_SOURCES
  "BitPumpJPEG.h"
  "BitPumpMSB.h"
  "BitPumpMSB16.h"
  "BitPumpMSB32.h"
  "BitPumpPlain.h"
  "BitStream.h"
  "Buffer.cpp"
  "Buffer.h"
  "ByteStream.cpp"
  "ByteStream.h"
  "FileIOException.cpp"
  "FileIOException.h"
  "FileMap.h"
  "FileReader.cpp"
  "FileReader.h"
  "FileWriter.cpp"
  "FileWriter.h"
  "IOException.cpp"
  "IOException.h"
)

set(RAWSPEED_SOURCES "${RAWSPEED_SOURCES};${IO_SOURCES}" PARENT_SCOPE)
