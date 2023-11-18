FILE(GLOB SOURCES
  "AlignedAllocator.h"
  "Array1DRef.h"
  "Array2DRef.h"
  "BitIterator.h"
  "Casts.h"
  "CroppedArray1DRef.h"
  "CroppedArray2DRef.h"
  "DefaultInitAllocatorAdaptor.h"
  "Invariant.h"
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
