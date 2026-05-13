cmake_minimum_required(VERSION 3.16)

project(csvreader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

add_library(csv_core
    src/csv_parser.c
    src/error.c
    src/evaluator.c
    src/formula.c
    src/int_parse.c
    src/table.c
)

target_include_directories(csv_core PUBLIC src)

add_executable(csvreader src/main.c)
target_link_libraries(csvreader PRIVATE csv_core)

enable_testing()

function(add_unit_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE csv_core)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

foreach(test_name csv_parser formula_parser evaluator)
    add_unit_test(${test_name}_tests tests/test_${test_name}.c)
endforeach()

set(CLI_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/input")
set(CLI_EXPECTED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/expected")
set(CLI_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_test.cmake")

function(add_cli_test test_name mode)
    add_test(NAME ${test_name}
        COMMAND ${CMAKE_COMMAND}
            "-DCSVREADER=$<TARGET_FILE:csvreader>"
            "-DMODE=${mode}"
            ${ARGN}
            -P "${CLI_RUNNER}"
    )
endfunction()

function(add_cli_success case_name)
    add_cli_test(cli_success_${case_name}_stdout expect_success_stdout
        "-DINPUT=${CLI_INPUT_DIR}/valid/${case_name}.csv"
        "-DEXPECTED_STDOUT=${CLI_EXPECTED_DIR}/valid/${case_name}.csv"
    )
endfunction()

function(add_cli_error input_file)
    get_filename_component(case_name "${input_file}" NAME_WE)
    add_cli_test(cli_error_${case_name}_stderr expect_error_stderr
        "-DINPUT=${input_file}"
    )
endfunction()

foreach(case_name basic dependencies out_of_order)
    add_cli_success(${case_name})
endforeach()

add_cli_test(cli_error_missing_input_stderr expect_usage_stderr)

file(GLOB cli_error_inputs CONFIGURE_DEPENDS "${CLI_INPUT_DIR}/invalid/*.csv")
foreach(input_file IN LISTS cli_error_inputs)
    add_cli_error("${input_file}")
endforeach()
