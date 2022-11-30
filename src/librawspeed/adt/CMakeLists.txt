FILE(GLOB SOURCES
  "Array2DRef.h"
  "CroppedArray2DRef.h"
  "DefaultInitAllocatorAdaptor.h"
  "Mutex.h"
  "NORangesSet.h"
  "NotARational.h"
  "Point.h"
  "Range.h"
  "iterator_range.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
