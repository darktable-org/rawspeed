FILE(GLOB SOURCES
  "Cr2sRawInterpolator.cpp"
  "Cr2sRawInterpolator.h"
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
