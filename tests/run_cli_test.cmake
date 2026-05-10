if(NOT DEFINED CSVREADER)
    message(FATAL_ERROR "CSVREADER is required")
endif()

if(NOT DEFINED MODE)
    message(FATAL_ERROR "MODE is required")
endif()

if(MODE STREQUAL "expect_success_stdout")
    if(NOT DEFINED INPUT)
        message(FATAL_ERROR "INPUT is required")
    endif()
    if(NOT DEFINED EXPECTED_STDOUT)
        message(FATAL_ERROR "EXPECTED_STDOUT is required")
    endif()

    execute_process(
        COMMAND "${CSVREADER}" "${INPUT}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )

    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "csvreader failed for ${INPUT}: ${stderr}")
    endif()
    if(NOT "${stderr}" STREQUAL "")
        message(FATAL_ERROR "expected empty stderr for ${INPUT}, got: ${stderr}")
    endif()

    file(READ "${EXPECTED_STDOUT}" expected_stdout)
    if(NOT "${stdout}" STREQUAL "${expected_stdout}")
        message(FATAL_ERROR "stdout mismatch for ${INPUT}\nExpected:\n${expected_stdout}\nActual:\n${stdout}")
    endif()
elseif(MODE STREQUAL "expect_error_stderr")
    if(NOT DEFINED INPUT)
        message(FATAL_ERROR "INPUT is required")
    endif()

    execute_process(
        COMMAND "${CSVREADER}" "${INPUT}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )

    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "expected non-zero exit for ${INPUT}")
    endif()
    if(NOT "${stdout}" STREQUAL "")
        message(FATAL_ERROR "expected empty stdout for ${INPUT}, got: ${stdout}")
    endif()
    if(NOT "${stderr}" MATCHES "^error:")
        message(FATAL_ERROR "expected stderr to start with 'error:' for ${INPUT}, got: ${stderr}")
    endif()
elseif(MODE STREQUAL "expect_usage_stderr")
    execute_process(
        COMMAND "${CSVREADER}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE stdout
        ERROR_VARIABLE stderr
    )

    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "expected non-zero exit for missing input")
    endif()
    if(NOT "${stdout}" STREQUAL "")
        message(FATAL_ERROR "expected empty stdout for missing input, got: ${stdout}")
    endif()
    if(NOT "${stderr}" MATCHES "^error: usage:")
        message(FATAL_ERROR "expected usage error, got: ${stderr}")
    endif()
else()
    message(FATAL_ERROR "unknown MODE: ${MODE}")
endif()
