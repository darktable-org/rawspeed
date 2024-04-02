rawspeed_add_library(rawspeed_adt OBJECT)
set_target_properties(rawspeed_adt PROPERTIES LINKER_LANGUAGE CXX)

FILE(GLOB SOURCES
  "AlignedAllocator.h"
  "Array1DRef.h"
  "Array2DRef.h"
  "Bit.h"
  "BitIterator.h"
  "Casts.h"
  "CoalescingOutputIterator.h"
  "CroppedArray1DRef.h"
  "CroppedArray2DRef.h"
  "DefaultInitAllocatorAdaptor.h"
  "Invariant.h"
  "Mutex.h"
  "NORangesSet.h"
  "NotARational.h"
  "PartitioningOutputIterator.h"
  "Point.h"
  "Range.h"
  "VariableLengthLoad.h"
  "iterator_range.h"
)

target_sources(rawspeed_adt PRIVATE
  ${SOURCES}
)

target_link_libraries(rawspeed PRIVATE rawspeed_adt)
