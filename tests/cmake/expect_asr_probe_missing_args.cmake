if(NOT DEFINED SHATV_ASR_PROBE OR SHATV_ASR_PROBE STREQUAL "")
    message(FATAL_ERROR "SHATV_ASR_PROBE is required")
endif()

execute_process(
    COMMAND "${SHATV_ASR_PROBE}"
    RESULT_VARIABLE probe_result
    OUTPUT_VARIABLE probe_stdout
    ERROR_VARIABLE probe_stderr
)

if(probe_result EQUAL 0)
    message(FATAL_ERROR "ASR probe succeeded without required arguments")
endif()

string(CONCAT probe_output "${probe_stdout}" "${probe_stderr}")
if(NOT probe_output MATCHES "missing required --model-dir")
    message(FATAL_ERROR "ASR probe did not report the missing model-dir error. Output: ${probe_output}")
endif()
