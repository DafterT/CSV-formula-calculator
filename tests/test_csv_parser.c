#include "csv_parser.h"
#include "evaluator.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef bool (*TestFn)(void);

typedef struct {
    const char *name;
    TestFn run;
} TestCase;

static bool test_fail(int line, const char *message)
{
    printf("    line %d: %s\n", line, message);
    return false;
}

#define EXPECT_TRUE(value) if (!(value)) return test_fail(__LINE__, "expected true: " #value)
#define EXPECT_FALSE(value) if ((value)) return test_fail(__LINE__, "expected false: " #value)
#define EXPECT_EQ_INT64(expected, actual) if ((int64_t)(expected) != (int64_t)(actual)) return test_fail(__LINE__, "int64 mismatch: " #actual)
#define EXPECT_EQ_CODE(expected, actual) if ((expected) != (actual)) return test_fail(__LINE__, "error code mismatch: " #actual)

static bool parse_text(const char *csv, Table *table, CsvError *error)
{
    FILE *file = tmpfile();
    bool ok = false;

    if (file == NULL) {
        return false;
    }

    if (fputs(csv, file) == EOF) {
        fclose(file);
        return false;
    }

    rewind(file);
    ok = csv_parse_stream(file, table, error);
    fclose(file);
    return ok;
}

static bool expect_error(const char *csv, CsvErrorCode code)
{
    Table table;
    CsvError error;
    bool ok = parse_text(csv, &table, &error);

    EXPECT_FALSE(ok);
    EXPECT_EQ_CODE(code, error.code);
    table_free(&table);
    return true;
}

static bool parse_and_evaluate(const char *csv, Table *table, CsvError *error)
{
    if (!parse_text(csv, table, error)) {
        return false;
    }

    return table_evaluate(table, error);
}

static bool expect_cell_value(const Table *table, size_t row, size_t column, int64_t value)
{
    const Cell *cell = table_cell_at_const(table, row, column);

    EXPECT_TRUE(cell != NULL);
    EXPECT_EQ_INT64(value, cell->value);
    return true;
}

static bool valid_basic_values(void)
{
    const char *csv = ",A,B,Cell\n1,1,0,1\n2,2,=A1+Cell30,0\n30,0,=B1+A1,5\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_and_evaluate(csv, &table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, 1));
    EXPECT_TRUE(expect_cell_value(&table, 1U, 1U, 6));
    EXPECT_TRUE(expect_cell_value(&table, 2U, 1U, 1));

    table_free(&table);
    return true;
}

static bool valid_out_of_order_rows(void)
{
    const char *csv = ",A,B\n30,2,=A1+1\n1,5,=A30+1\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_and_evaluate(csv, &table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 1U, 6));
    EXPECT_TRUE(expect_cell_value(&table, 1U, 1U, 3));

    table_free(&table);
    return true;
}

static bool valid_crlf(void)
{
    const char *csv = ",A,B\r\n1,1,2\r\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_text(csv, &table, &error));

    table_free(&table);
    return true;
}

static bool valid_no_final_newline(void)
{
    const char *csv = ",A\n1,10";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_and_evaluate(csv, &table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, 10));

    table_free(&table);
    return true;
}

static bool valid_negative_cell(void)
{
    const char *csv = ",A\n1,-10\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_and_evaluate(csv, &table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, -10));

    table_free(&table);
    return true;
}

static bool valid_integer_boundaries(void)
{
    const char *csv = ",A,B,C\n1,9223372036854775807,-9223372036854775808,-0\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_and_evaluate(csv, &table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, INT64_MAX));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 1U, INT64_MIN));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 2U, 0));

    table_free(&table);
    return true;
}

static bool valid_max_row_number(void)
{
    const char *csv = ",A\n9223372036854775807,1\n";
    Table table;
    CsvError error;
    size_t index = 99U;

    EXPECT_TRUE(parse_text(csv, &table, &error));
    EXPECT_TRUE(table_find_row(&table, INT64_MAX, &index));
    EXPECT_EQ_INT64(0, index);

    table_free(&table);
    return true;
}

static bool valid_formula_values(void)
{
    const char *csv = ",A,B,C\n1,=1+2,=A1+1,=-1+2\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_and_evaluate(csv, &table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, 3));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 1U, 4));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 2U, 1));

    table_free(&table);
    return true;
}

static bool invalid_unknown_reference_after_parse(void)
{
    const char *csv = ",A\n1,=C1+1\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_text(csv, &table, &error));
    EXPECT_FALSE(table_evaluate(&table, &error));
    EXPECT_EQ_CODE(CSV_ERROR_INVALID_REFERENCE, error.code);

    table_free(&table);
    return true;
}

static bool valid_formula_negative_number_value(void)
{
    const char *csv = ",A\n1,=-1+2\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_and_evaluate(csv, &table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, 1));

    table_free(&table);
    return true;
}

static bool valid_parse_builds_lookups(void)
{
    const char *csv = ",B,A\n2,5,7\n";
    Table table;
    CsvError error;
    size_t index = 99U;

    EXPECT_TRUE(parse_text(csv, &table, &error));
    EXPECT_TRUE(table_find_row(&table, 2, &index));
    EXPECT_EQ_INT64(0, index);
    EXPECT_TRUE(table_find_column(&table, "A", &index));
    EXPECT_EQ_INT64(1, index);

    table_free(&table);
    return true;
}

static bool invalid_empty_file(void) { return expect_error("", CSV_ERROR_EMPTY_FILE); }
static bool invalid_header_only(void) { return expect_error(",A\n", CSV_ERROR_INVALID_HEADER); }
static bool invalid_header_first_cell_not_empty(void) { return expect_error("X,A\n1,2\n", CSV_ERROR_INVALID_HEADER); }
static bool invalid_no_columns(void) { return expect_error("\n1\n", CSV_ERROR_INVALID_HEADER); }
static bool invalid_empty_column_name(void) { return expect_error(",A,\n1,1,2\n", CSV_ERROR_INVALID_HEADER); }
static bool invalid_bad_column_digit(void) { return expect_error(",A1\n1,1\n", CSV_ERROR_INVALID_HEADER); }
static bool invalid_bad_column_space(void) { return expect_error(",A \n1,1\n", CSV_ERROR_INVALID_HEADER); }
static bool invalid_bad_column_dash(void) { return expect_error(",A-B\n1,1\n", CSV_ERROR_INVALID_HEADER); }
static bool invalid_duplicate_column(void) { return expect_error(",A,A\n1,1,2\n", CSV_ERROR_DUPLICATE_COLUMN); }
static bool invalid_missing_row_number(void) { return expect_error(",A\n,1\n", CSV_ERROR_INVALID_ROW_NUMBER); }
static bool invalid_zero_row_number(void) { return expect_error(",A\n0,1\n", CSV_ERROR_INVALID_ROW_NUMBER); }
static bool invalid_negative_row_number(void) { return expect_error(",A\n-1,1\n", CSV_ERROR_INVALID_ROW_NUMBER); }
static bool invalid_plus_row_number(void) { return expect_error(",A\n+10,1\n", CSV_ERROR_INVALID_ROW_NUMBER); }
static bool invalid_row_number_overflow(void) { return expect_error(",A\n999999999999999999999999999999,1\n", CSV_ERROR_INTEGER_OVERFLOW); }
static bool invalid_row_number_overflow_suffix(void) { return expect_error(",A\n9223372036854775808abc,1\n", CSV_ERROR_INVALID_ROW_NUMBER); }
static bool invalid_duplicate_row(void) { return expect_error(",A\n1,1\n1,2\n", CSV_ERROR_DUPLICATE_ROW); }
static bool invalid_too_few_fields(void) { return expect_error(",A,B\n1,1\n", CSV_ERROR_MALFORMED); }
static bool invalid_too_many_fields(void) { return expect_error(",A\n1,1,2\n", CSV_ERROR_MALFORMED); }
static bool invalid_empty_cell(void) { return expect_error(",A\n1,\n", CSV_ERROR_INVALID_CELL); }
static bool invalid_bad_integer_cell(void) { return expect_error(",A\n1,abc\n", CSV_ERROR_INVALID_CELL); }
static bool invalid_plus_integer_cell(void) { return expect_error(",A\n1,+10\n", CSV_ERROR_INVALID_CELL); }
static bool invalid_integer_overflow(void) { return expect_error(",A\n1,999999999999999999999999999999\n", CSV_ERROR_INTEGER_OVERFLOW); }
static bool invalid_integer_max_plus_one(void) { return expect_error(",A\n1,9223372036854775808\n", CSV_ERROR_INTEGER_OVERFLOW); }
static bool invalid_integer_min_minus_one(void) { return expect_error(",A\n1,-9223372036854775809\n", CSV_ERROR_INTEGER_OVERFLOW); }
static bool invalid_integer_overflow_suffix(void) { return expect_error(",A\n1,9223372036854775808abc\n", CSV_ERROR_INVALID_CELL); }
static bool invalid_quote(void) { return expect_error(",A\n1,\"1\"\n", CSV_ERROR_MALFORMED); }
static bool invalid_bare_cr(void) { return expect_error(",A\r1,1\n", CSV_ERROR_MALFORMED); }
static bool invalid_empty_line(void) { return expect_error(",A\n\n1,1\n", CSV_ERROR_MALFORMED); }
static bool invalid_formula_empty(void) { return expect_error(",A\n1,=\n", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_formula_spaces(void) { return expect_error(",A\n1,=A1 +1\n", CSV_ERROR_INVALID_FORMULA); }

int main(void)
{
    const TestCase tests[] = {
        {"valid_basic_values", valid_basic_values},
        {"valid_out_of_order_rows", valid_out_of_order_rows},
        {"valid_crlf", valid_crlf},
        {"valid_no_final_newline", valid_no_final_newline},
        {"valid_negative_cell", valid_negative_cell},
        {"valid_integer_boundaries", valid_integer_boundaries},
        {"valid_max_row_number", valid_max_row_number},
        {"valid_formula_values", valid_formula_values},
        {"invalid_unknown_reference_after_parse", invalid_unknown_reference_after_parse},
        {"valid_formula_negative_number_value", valid_formula_negative_number_value},
        {"valid_parse_builds_lookups", valid_parse_builds_lookups},
        {"invalid_empty_file", invalid_empty_file},
        {"invalid_header_only", invalid_header_only},
        {"invalid_header_first_cell_not_empty", invalid_header_first_cell_not_empty},
        {"invalid_no_columns", invalid_no_columns},
        {"invalid_empty_column_name", invalid_empty_column_name},
        {"invalid_bad_column_digit", invalid_bad_column_digit},
        {"invalid_bad_column_space", invalid_bad_column_space},
        {"invalid_bad_column_dash", invalid_bad_column_dash},
        {"invalid_duplicate_column", invalid_duplicate_column},
        {"invalid_missing_row_number", invalid_missing_row_number},
        {"invalid_zero_row_number", invalid_zero_row_number},
        {"invalid_negative_row_number", invalid_negative_row_number},
        {"invalid_plus_row_number", invalid_plus_row_number},
        {"invalid_row_number_overflow", invalid_row_number_overflow},
        {"invalid_row_number_overflow_suffix", invalid_row_number_overflow_suffix},
        {"invalid_duplicate_row", invalid_duplicate_row},
        {"invalid_too_few_fields", invalid_too_few_fields},
        {"invalid_too_many_fields", invalid_too_many_fields},
        {"invalid_empty_cell", invalid_empty_cell},
        {"invalid_bad_integer_cell", invalid_bad_integer_cell},
        {"invalid_plus_integer_cell", invalid_plus_integer_cell},
        {"invalid_integer_overflow", invalid_integer_overflow},
        {"invalid_integer_max_plus_one", invalid_integer_max_plus_one},
        {"invalid_integer_min_minus_one", invalid_integer_min_minus_one},
        {"invalid_integer_overflow_suffix", invalid_integer_overflow_suffix},
        {"invalid_quote", invalid_quote},
        {"invalid_bare_cr", invalid_bare_cr},
        {"invalid_empty_line", invalid_empty_line},
        {"invalid_formula_empty", invalid_formula_empty},
        {"invalid_formula_spaces", invalid_formula_spaces}
    };
    size_t count = sizeof(tests) / sizeof(tests[0]);
    size_t index = 0U;
    size_t failed = 0U;

    while (index < count) {
        bool ok = tests[index].run();

        if (ok) {
            printf("[PASS] %s\n", tests[index].name);
        } else {
            printf("[FAIL] %s\n", tests[index].name);
            failed++;
        }
        index++;
    }

    if (failed > 0U) {
        printf("%zu test(s) failed\n", failed);
        return 1;
    }

    printf("%zu test(s) passed\n", count);
    return 0;
}
