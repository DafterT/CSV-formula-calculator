cmake_minimum_required(VERSION 3.16)

project(csvreader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(csv_core
    src/csv_parser.c
    src/error.c
    src/evaluator.c
    src/formula.c
    src/table.c
)

target_include_directories(csv_core PUBLIC src)
target_compile_options(csv_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(csvreader src/main.c)
target_link_libraries(csvreader PRIVATE csv_core)
target_compile_options(csvreader PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()

add_executable(csv_parser_tests tests/test_csv_parser.c)
target_link_libraries(csv_parser_tests PRIVATE csv_core)
target_compile_options(csv_parser_tests PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME csv_parser_tests COMMAND csv_parser_tests)

add_executable(formula_parser_tests tests/test_formula_parser.c)
target_link_libraries(formula_parser_tests PRIVATE csv_core)
target_compile_options(formula_parser_tests PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME formula_parser_tests COMMAND formula_parser_tests)

add_executable(evaluator_tests tests/test_evaluator.c)
target_link_libraries(evaluator_tests PRIVATE csv_core)
target_compile_options(evaluator_tests PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME evaluator_tests COMMAND evaluator_tests)

set(CLI_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/input")
set(CLI_EXPECTED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/expected")
set(CLI_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_test.cmake")

function(add_cli_success_stdout case_name)
    add_test(
        NAME cli_success_${case_name}_stdout
        COMMAND ${CMAKE_COMMAND}
            -DCSVREADER=$<TARGET_FILE:csvreader>
            -DINPUT=${CLI_INPUT_DIR}/valid/${case_name}.csv
            -DEXPECTED_STDOUT=${CLI_EXPECTED_DIR}/valid/${case_name}.csv
            -DMODE=expect_success_stdout
            -P ${CLI_RUNNER}
    )
endfunction()

function(add_cli_error_stderr input_file)
    get_filename_component(case_name "${input_file}" NAME_WE)
    add_test(
        NAME cli_error_${case_name}_stderr
        COMMAND ${CMAKE_COMMAND}
            -DCSVREADER=$<TARGET_FILE:csvreader>
            -DINPUT=${input_file}
            -DMODE=expect_error_stderr
            -P ${CLI_RUNNER}
    )
endfunction()

foreach(case_name IN ITEMS basic dependencies out_of_order)
    add_cli_success_stdout(${case_name})
endforeach()

add_test(
    NAME cli_error_missing_input_stderr
    COMMAND ${CMAKE_COMMAND}
        -DCSVREADER=$<TARGET_FILE:csvreader>
        -DMODE=expect_usage_stderr
        -P ${CLI_RUNNER}
)

file(GLOB cli_error_inputs CONFIGURE_DEPENDS "${CLI_INPUT_DIR}/invalid/*.csv")
foreach(input_file IN LISTS cli_error_inputs)
    add_cli_error_stderr("${input_file}")
endforeach()
