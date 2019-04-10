file(STRINGS "${RAWSPEED_REFERENCE_SAMPLE_ARCHIVE}/filelist.sha1" _REFERENCE_SAMPLES ENCODING UTF-8)

set(REFERENCE_SAMPLES)
set(REFERENCE_SAMPLE_HASHES)

foreach(STR ${_REFERENCE_SAMPLES})
  # There are two schemes:
  #   <40-char SHA1><space><space><filename>      <- read in text mode
  #   <40-char SHA1><space><asterisk><filename>   <- read in binary mode
  # We ignore read mode, so it becomes:
  #   <40-char SHA1><char><char><filename>
  string(SUBSTRING "${STR}" 0 40 SAMPLEHASH)
  string(SUBSTRING "${STR}" 42 -1 SAMPLENAME)
  set(FULLSAMPLENAME "${RAWSPEED_REFERENCE_SAMPLE_ARCHIVE}/${SAMPLENAME}")

  # We have already verified that the entry is valid.

  if(NOT TEST_SUITE_BENCHMARKING_ONLY)
    if(NOT EXISTS "${FULLSAMPLENAME}.hash")
      message(SEND_ERROR "The reference hash for sample \"${FULLSAMPLENAME}\" does not exist!")
    endif()

    llvm_test_run("\"${SAMPLENAME}\"" WORKDIR "${RAWSPEED_REFERENCE_SAMPLE_ARCHIVE}")
    llvm_add_test("${CMAKE_CURRENT_BINARY_DIR}/rstest/${SAMPLENAME}.${SAMPLEHASH}.test" $<TARGET_FILE:rstest>)
  endif()
  if(TARGET rsbench)
    # WARNING: filename *MUST* be first argument!
    llvm_test_run("\"${SAMPLENAME}\"" WORKDIR "${RAWSPEED_REFERENCE_SAMPLE_ARCHIVE}")
    llvm_add_test("${CMAKE_CURRENT_BINARY_DIR}/rsbench/${SAMPLENAME}.${SAMPLEHASH}.test" $<TARGET_FILE:rsbench>)
  endif()
endforeach()

add_subdirectory(rsbench)

file(COPY lit.local.cfg DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
