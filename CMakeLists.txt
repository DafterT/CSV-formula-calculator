cmake_minimum_required(VERSION 3.16)

project(csvreader C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(csv_core
    src/csv_parser.c
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
