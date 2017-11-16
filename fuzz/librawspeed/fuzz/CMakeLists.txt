FILE(GLOB RAWSPEED_FUZZ_SOURCES
  "RawSpeed.cpp"
)

target_sources(rawspeed_fuzz PRIVATE
  ${RAWSPEED_FUZZ_SOURCES}
)
