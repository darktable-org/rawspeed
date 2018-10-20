target_include_directories(rawspeed SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

FILE(GLOB SOURCES
  AddressSanitizer.h
  MemorySanitizer.h
  ThreadSafetyAnalysis.h
  gopro/vc5/table17.inc
)

target_sources(rawspeed PRIVATE
  ${SOURCES}
)
