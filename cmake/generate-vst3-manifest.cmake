# SPDX-License-Identifier: AGPL-3.0-only
get_filename_component(_deepfry_manifest_dir "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${_deepfry_manifest_dir}")
execute_process(COMMAND ${EMULATOR} "${HELPER_EXE}"
    OUTPUT_FILE "${OUTPUT_FILE}"
    ERROR_VARIABLE _deepfry_manifest_error
    RESULT_VARIABLE _deepfry_manifest_result
    TIMEOUT 120)
if(NOT _deepfry_manifest_result EQUAL 0)
    file(REMOVE "${OUTPUT_FILE}")
    message(FATAL_ERROR "VST3 manifest helper failed: ${_deepfry_manifest_error}")
endif()
file(READ "${OUTPUT_FILE}" _deepfry_manifest)
string(JSON _deepfry_manifest_version ERROR_VARIABLE _deepfry_json_error GET "${_deepfry_manifest}" Version)
if(_deepfry_json_error)
    file(REMOVE "${OUTPUT_FILE}")
    message(FATAL_ERROR "VST3 helper did not emit valid module metadata: ${_deepfry_json_error}")
endif()
# The upstream helper permits trailing commas. Canonicalize its parsed object
# so the distributed metadata is also accepted by strict JSON readers.
string(JSON _deepfry_manifest_normalized GET "${_deepfry_manifest}")
file(WRITE "${OUTPUT_FILE}" "${_deepfry_manifest_normalized}\n")
