execute_process(
    COMMAND "${SIZE_TOOL}" -A "${ELF_FILE}"
    OUTPUT_VARIABLE size_report
    ERROR_VARIABLE size_error
    RESULT_VARIABLE size_result)
if(NOT size_result EQUAL 0)
    message(FATAL_ERROR "size reporting failed: ${size_error}")
endif()
file(WRITE "${OUTPUT_DIR}/rts_s32k148_smoke.size.txt" "${size_report}")
message(STATUS "Wrote S32K148 RAM/ROM report: rts_s32k148_smoke.size.txt")
