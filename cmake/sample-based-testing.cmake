message(STATUS "Looking for sample set for sample-based testing")
message(STATUS "Looking for sample set in ${REFERENCE_SAMPLE_ARCHIVE}")
if(NOT (EXISTS "${REFERENCE_SAMPLE_ARCHIVE}"
    AND EXISTS "${REFERENCE_SAMPLE_ARCHIVE}/filelist.sha1"
    AND EXISTS "${REFERENCE_SAMPLE_ARCHIVE}/timestamp.txt"))
  message(SEND_ERROR "Did not find sample set for sample-based testing! Either pass correct path in REFERENCE_SAMPLE_ARCHIVE, or disable ENABLE_SAMPLEBASED_TESTING.")
endif()

message(STATUS "Found sample set in ${REFERENCE_SAMPLE_ARCHIVE}")

file(STRINGS "${REFERENCE_SAMPLE_ARCHIVE}/filelist.sha1" _REFERENCE_SAMPLES ENCODING UTF-8)

set(REFERENCE_SAMPLES)
set(REFERENCE_SAMPLE_HASHES)

foreach(STR ${_REFERENCE_SAMPLES})
  string(SUBSTRING "${STR}" 40 -1 SAMPLENAME)
  string(STRIP "${SAMPLENAME}" SAMPLENAME)
  set(SAMPLENAME "${REFERENCE_SAMPLE_ARCHIVE}/${SAMPLENAME}")

  if(NOT EXISTS "${SAMPLENAME}")
    message(SEND_ERROR "The sample \"${SAMPLENAME}\" does not exist!")
  endif()

  list(APPEND REFERENCE_SAMPLES "${SAMPLENAME}")
  list(APPEND REFERENCE_SAMPLE_HASHES "${SAMPLENAME}.hash")
  list(APPEND REFERENCE_SAMPLE_HASHES "${SAMPLENAME}.hash.failed")
endforeach()

add_custom_target(rstest-create)
add_custom_command(TARGET rstest-create
  COMMAND rstest -c ${REFERENCE_SAMPLES}
  WORKING_DIRECTORY "${REFERENCE_SAMPLE_ARCHIVE}"
  COMMENT "Running rstest on all the samples in the sample set to generate the missing hashes"
  VERBATIM)

add_custom_target(rstest-recreate)
add_custom_command(TARGET rstest-recreate
  COMMAND rstest -c -f ${REFERENCE_SAMPLES}
  WORKING_DIRECTORY "${REFERENCE_SAMPLE_ARCHIVE}"
  COMMENT "Running rstest on all the samples in the sample set to [re]generate all the hashes"
  VERBATIM)

add_custom_target(rstest-test) # hashes must exist beforehand
add_custom_command(TARGET rstest-test
  COMMAND rstest ${REFERENCE_SAMPLES}
  WORKING_DIRECTORY "${REFERENCE_SAMPLE_ARCHIVE}"
  COMMENT "Running rstest on all the samples in the sample set to check for regressions"
  VERBATIM)

add_custom_target(rstest-check) # hashes should exist beforehand if you want to check for regressions
add_custom_command(TARGET rstest-check
  COMMAND rstest -f ${REFERENCE_SAMPLES}
  WORKING_DIRECTORY "${REFERENCE_SAMPLE_ARCHIVE}"
  COMMENT "Trying to decode all the samples in the sample set"
  VERBATIM)

add_custom_target(rstest-clean)
add_custom_command(TARGET rstest-clean
  COMMAND "${CMAKE_COMMAND}" -E remove ${REFERENCE_SAMPLE_HASHES}
  WORKING_DIRECTORY "${REFERENCE_SAMPLE_ARCHIVE}"
  COMMENT "Removing *.hash, *.hash.failed for the each sample in the set"
  VERBATIM)
