#include "csv_parser.h"

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
#define EXPECT_EQ_SIZE(expected, actual) if ((size_t)(expected) != (size_t)(actual)) return test_fail(__LINE__, "size mismatch: " #actual)
#define EXPECT_EQ_INT64(expected, actual) if ((int64_t)(expected) != (int64_t)(actual)) return test_fail(__LINE__, "int64 mismatch: " #actual)
#define EXPECT_EQ_CODE(expected, actual) if ((expected) != (actual)) return test_fail(__LINE__, "error code mismatch: " #actual)
#define EXPECT_STREQ(expected, actual) if (strcmp((expected), (actual)) != 0) return test_fail(__LINE__, "string mismatch: " #actual)

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

static bool valid_basic_shape(void)
{
    const char *csv = ",A,B,Cell\n1,1,0,1\n2,2,=A1+Cell30,0\n30,0,=B1+A1,5\n";
    Table table;
    CsvError error;
    const Cell *cell = NULL;

    EXPECT_TRUE(parse_text(csv, &table, &error));
    EXPECT_EQ_SIZE(3U, table.column_count);
    EXPECT_EQ_SIZE(3U, table.row_count);
    EXPECT_EQ_SIZE(9U, table.cell_count);
    EXPECT_STREQ("A", table.columns[0].name);
    EXPECT_STREQ("B", table.columns[1].name);
    EXPECT_STREQ("Cell", table.columns[2].name);
    EXPECT_EQ_INT64(1, table.rows[0].number);
    EXPECT_EQ_INT64(2, table.rows[1].number);
    EXPECT_EQ_INT64(30, table.rows[2].number);

    cell = table_cell_at_const(&table, 1U, 1U);
    EXPECT_TRUE(cell != NULL);
    EXPECT_EQ_INT64(CELL_FORMULA, cell->kind);
    EXPECT_STREQ("=A1+Cell30", cell->formula);
    EXPECT_TRUE(table.row_lookup != NULL);
    EXPECT_TRUE(table.column_lookup != NULL);

    table_free(&table);
    return true;
}

static bool valid_out_of_order_rows(void)
{
    const char *csv = ",A\n30,5\n1,6\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_text(csv, &table, &error));
    EXPECT_EQ_INT64(30, table.rows[0].number);
    EXPECT_EQ_INT64(1, table.rows[1].number);

    table_free(&table);
    return true;
}

static bool valid_crlf(void)
{
    const char *csv = ",A,B\r\n1,1,2\r\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_text(csv, &table, &error));
    EXPECT_EQ_SIZE(2U, table.column_count);
    EXPECT_EQ_SIZE(1U, table.row_count);

    table_free(&table);
    return true;
}

static bool valid_no_final_newline(void)
{
    const char *csv = ",A\n1,10";
    Table table;
    CsvError error;
    const Cell *cell = NULL;

    EXPECT_TRUE(parse_text(csv, &table, &error));
    cell = table_cell_at_const(&table, 0U, 0U);
    EXPECT_TRUE(cell != NULL);
    EXPECT_EQ_INT64(10, cell->value);

    table_free(&table);
    return true;
}

static bool valid_negative_cell(void)
{
    const char *csv = ",A\n1,-10\n";
    Table table;
    CsvError error;
    const Cell *cell = NULL;

    EXPECT_TRUE(parse_text(csv, &table, &error));
    cell = table_cell_at_const(&table, 0U, 0U);
    EXPECT_TRUE(cell != NULL);
    EXPECT_EQ_INT64(-10, cell->value);

    table_free(&table);
    return true;
}

static bool valid_formula_raw(void)
{
    const char *csv = ",A,B\n1,=A1+Cell30,=\n";
    Table table;
    CsvError error;
    const Cell *cell = NULL;

    EXPECT_TRUE(parse_text(csv, &table, &error));
    cell = table_cell_at_const(&table, 0U, 0U);
    EXPECT_TRUE(cell != NULL);
    EXPECT_STREQ("=A1+Cell30", cell->formula);
    cell = table_cell_at_const(&table, 0U, 1U);
    EXPECT_TRUE(cell != NULL);
    EXPECT_STREQ("=", cell->formula);

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
static bool invalid_duplicate_row(void) { return expect_error(",A\n1,1\n1,2\n", CSV_ERROR_DUPLICATE_ROW); }
static bool invalid_too_few_fields(void) { return expect_error(",A,B\n1,1\n", CSV_ERROR_MALFORMED); }
static bool invalid_too_many_fields(void) { return expect_error(",A\n1,1,2\n", CSV_ERROR_MALFORMED); }
static bool invalid_empty_cell(void) { return expect_error(",A\n1,\n", CSV_ERROR_INVALID_CELL); }
static bool invalid_bad_integer_cell(void) { return expect_error(",A\n1,abc\n", CSV_ERROR_INVALID_CELL); }
static bool invalid_plus_integer_cell(void) { return expect_error(",A\n1,+10\n", CSV_ERROR_INVALID_CELL); }
static bool invalid_integer_overflow(void) { return expect_error(",A\n1,999999999999999999999999999999\n", CSV_ERROR_INTEGER_OVERFLOW); }
static bool invalid_quote(void) { return expect_error(",A\n1,\"1\"\n", CSV_ERROR_MALFORMED); }
static bool invalid_bare_cr(void) { return expect_error(",A\r1,1\n", CSV_ERROR_MALFORMED); }
static bool invalid_empty_line(void) { return expect_error(",A\n\n1,1\n", CSV_ERROR_MALFORMED); }

int main(void)
{
    const TestCase tests[] = {
        {"valid_basic_shape", valid_basic_shape},
        {"valid_out_of_order_rows", valid_out_of_order_rows},
        {"valid_crlf", valid_crlf},
        {"valid_no_final_newline", valid_no_final_newline},
        {"valid_negative_cell", valid_negative_cell},
        {"valid_formula_raw", valid_formula_raw},
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
        {"invalid_duplicate_row", invalid_duplicate_row},
        {"invalid_too_few_fields", invalid_too_few_fields},
        {"invalid_too_many_fields", invalid_too_many_fields},
        {"invalid_empty_cell", invalid_empty_cell},
        {"invalid_bad_integer_cell", invalid_bad_integer_cell},
        {"invalid_plus_integer_cell", invalid_plus_integer_cell},
        {"invalid_integer_overflow", invalid_integer_overflow},
        {"invalid_quote", invalid_quote},
        {"invalid_bare_cr", invalid_bare_cr},
        {"invalid_empty_line", invalid_empty_line}
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
