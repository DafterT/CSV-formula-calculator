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

static bool parse_and_evaluate(const char *csv, Table *table, CsvError *error)
{
    if (!parse_text(csv, table, error)) {
        return false;
    }

    return table_evaluate(table, error);
}

static bool parse_fixture_file(const char *path, Table *table, CsvError *error)
{
    char parent_path[256];

    if (csv_parse_file(path, table, error)) {
        return true;
    }

    if (snprintf(parent_path, sizeof(parent_path), "../%s", path) >= (int)sizeof(parent_path)) {
        return false;
    }

    return csv_parse_file(parent_path, table, error);
}

static bool expect_error(const char *csv, CsvErrorCode code)
{
    Table table;
    CsvError error;
    bool ok = parse_and_evaluate(csv, &table, &error);

    EXPECT_FALSE(ok);
    EXPECT_EQ_CODE(code, error.code);
    table_free(&table);
    return true;
}

static bool expect_cell_value(const Table *table, size_t row, size_t column, int64_t value)
{
    const Cell *cell = table_cell_at_const(table, row, column);

    EXPECT_TRUE(cell != NULL);
    EXPECT_EQ_INT64(EVAL_DONE, cell->state);
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
    EXPECT_TRUE(expect_cell_value(&table, 0U, 1U, 0));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 2U, 1));
    EXPECT_TRUE(expect_cell_value(&table, 1U, 0U, 2));
    EXPECT_TRUE(expect_cell_value(&table, 1U, 1U, 6));
    EXPECT_TRUE(expect_cell_value(&table, 1U, 2U, 0));
    EXPECT_TRUE(expect_cell_value(&table, 2U, 0U, 0));
    EXPECT_TRUE(expect_cell_value(&table, 2U, 1U, 1));
    EXPECT_TRUE(expect_cell_value(&table, 2U, 2U, 5));

    table_free(&table);
    return true;
}

static bool valid_dependency_values(void)
{
    const char *csv = ",A,B\n1,10,=A1+5\n2,=B1*2,3\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_and_evaluate(csv, &table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, 10));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 1U, 15));
    EXPECT_TRUE(expect_cell_value(&table, 1U, 0U, 30));
    EXPECT_TRUE(expect_cell_value(&table, 1U, 1U, 3));

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

static bool valid_negative_literals(void)
{
    const char *csv = ",A,B,C\n1,=-1+2,=A1--2,=B1*-2\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_and_evaluate(csv, &table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, 1));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 1U, 3));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 2U, -6));

    table_free(&table);
    return true;
}

static bool valid_deep_dependency_chain_fixture(void)
{
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_fixture_file("tests/input/stress/deep_chain.csv", &table, &error));
    EXPECT_TRUE(table_evaluate(&table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, 500000));
    EXPECT_TRUE(expect_cell_value(&table, 499999U, 0U, 1));

    table_free(&table);
    return true;
}

static bool valid_wide_column_dependency_chain_fixture(void)
{
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_fixture_file("tests/input/stress/wide_columns.csv", &table, &error));
    EXPECT_TRUE(table_evaluate(&table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, 500000));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 499999U, 1));

    table_free(&table);
    return true;
}

static bool valid_reference_resolution_values(void)
{
    const char *csv = ",B,A\n2,=A1+B1,3\n1,5,7\n";
    Table table;
    CsvError error;

    EXPECT_TRUE(parse_and_evaluate(csv, &table, &error));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 0U, 12));
    EXPECT_TRUE(expect_cell_value(&table, 0U, 1U, 3));
    EXPECT_TRUE(expect_cell_value(&table, 1U, 0U, 5));
    EXPECT_TRUE(expect_cell_value(&table, 1U, 1U, 7));

    table_free(&table);
    return true;
}

static bool table_lookup_requires_build(void)
{
    Table table;
    size_t index = 99U;

    table_init(&table);
    EXPECT_TRUE(table_add_column(&table, "A", 2U));
    EXPECT_TRUE(table_add_row(&table, 1, 2U, NULL));
    EXPECT_TRUE(table_add_number_cell(&table, 10, 2U, 2U));
    EXPECT_FALSE(table_find_row(&table, 1, &index));
    EXPECT_FALSE(table_find_column(&table, "A", &index));

    table_free(&table);
    return true;
}

static bool table_cell_at_rejects_incomplete_row(void)
{
    Table table;

    table_init(&table);
    EXPECT_TRUE(table_add_column(&table, "A", 2U));
    EXPECT_TRUE(table_add_column(&table, "B", 3U));
    EXPECT_TRUE(table_add_row(&table, 1, 2U, NULL));
    EXPECT_TRUE(table_add_number_cell(&table, 10, 2U, 2U));
    EXPECT_TRUE(table_cell_at(&table, 0U, 0U) != NULL);
    EXPECT_TRUE(table_cell_at(&table, 0U, 1U) == NULL);
    EXPECT_TRUE(table_cell_at_const(&table, 0U, 1U) == NULL);

    table_free(&table);
    return true;
}

static bool table_build_lookups_rejects_incomplete_table(void)
{
    Table table;
    CsvError error;
    size_t index = 99U;

    table_init(&table);
    csv_error_clear(&error);
    EXPECT_TRUE(table_add_column(&table, "A", 2U));
    EXPECT_TRUE(table_add_column(&table, "B", 3U));
    EXPECT_TRUE(table_add_row(&table, 1, 2U, NULL));
    EXPECT_TRUE(table_add_number_cell(&table, 10, 2U, 2U));
    EXPECT_FALSE(table_build_lookups(&table, &error));
    EXPECT_EQ_CODE(CSV_ERROR_MALFORMED, error.code);
    EXPECT_FALSE(table_find_row(&table, 1, &index));

    table_free(&table);
    return true;
}

static bool invalid_incomplete_table_evaluate(void)
{
    Table table;
    CsvError error;

    table_init(&table);
    csv_error_clear(&error);
    EXPECT_TRUE(table_add_column(&table, "A", 2U));
    EXPECT_TRUE(table_add_column(&table, "B", 3U));
    EXPECT_TRUE(table_add_row(&table, 1, 2U, NULL));
    EXPECT_TRUE(table_add_number_cell(&table, 10, 2U, 2U));
    EXPECT_FALSE(table_evaluate(&table, &error));
    EXPECT_EQ_CODE(CSV_ERROR_MALFORMED, error.code);

    table_free(&table);
    return true;
}

static bool invalid_unknown_column(void) { return expect_error(",A\n1,=B1+1\n", CSV_ERROR_INVALID_REFERENCE); }
static bool invalid_unknown_row(void) { return expect_error(",A\n1,=A2+1\n", CSV_ERROR_INVALID_REFERENCE); }
static bool invalid_division_by_zero_literal(void) { return expect_error(",A\n1,=1/0\n", CSV_ERROR_DIVISION_BY_ZERO); }
static bool invalid_division_by_zero_reference(void) { return expect_error(",A,B\n1,0,=10/A1\n", CSV_ERROR_DIVISION_BY_ZERO); }
static bool invalid_self_cycle(void) { return expect_error(",A\n1,=A1+1\n", CSV_ERROR_CYCLIC_DEPENDENCY); }
static bool invalid_right_side_self_cycle(void) { return expect_error(",A\n1,=1+A1\n", CSV_ERROR_CYCLIC_DEPENDENCY); }
static bool invalid_two_cell_cycle(void) { return expect_error(",A,B\n1,=B1+1,=A1+1\n", CSV_ERROR_CYCLIC_DEPENDENCY); }
static bool invalid_long_cycle(void) { return expect_error(",A,B,C\n1,=B1+1,=C1+1,=A1+1\n", CSV_ERROR_CYCLIC_DEPENDENCY); }
static bool invalid_add_overflow(void) { return expect_error(",A\n1,=9223372036854775807+1\n", CSV_ERROR_INTEGER_OVERFLOW); }
static bool invalid_sub_overflow(void) { return expect_error(",A\n1,=-9223372036854775808-1\n", CSV_ERROR_INTEGER_OVERFLOW); }
static bool invalid_mul_overflow(void) { return expect_error(",A\n1,=9223372036854775807*2\n", CSV_ERROR_INTEGER_OVERFLOW); }
static bool invalid_div_overflow(void) { return expect_error(",A\n1,=-9223372036854775808/-1\n", CSV_ERROR_INTEGER_OVERFLOW); }

int main(void)
{
    const TestCase tests[] = {
        {"valid_basic_values", valid_basic_values},
        {"valid_dependency_values", valid_dependency_values},
        {"valid_out_of_order_rows", valid_out_of_order_rows},
        {"valid_negative_literals", valid_negative_literals},
        {"valid_deep_dependency_chain_fixture", valid_deep_dependency_chain_fixture},
        {"valid_wide_column_dependency_chain_fixture", valid_wide_column_dependency_chain_fixture},
        {"valid_reference_resolution_values", valid_reference_resolution_values},
        {"table_lookup_requires_build", table_lookup_requires_build},
        {"table_cell_at_rejects_incomplete_row", table_cell_at_rejects_incomplete_row},
        {"table_build_lookups_rejects_incomplete_table", table_build_lookups_rejects_incomplete_table},
        {"invalid_incomplete_table_evaluate", invalid_incomplete_table_evaluate},
        {"invalid_unknown_column", invalid_unknown_column},
        {"invalid_unknown_row", invalid_unknown_row},
        {"invalid_division_by_zero_literal", invalid_division_by_zero_literal},
        {"invalid_division_by_zero_reference", invalid_division_by_zero_reference},
        {"invalid_self_cycle", invalid_self_cycle},
        {"invalid_right_side_self_cycle", invalid_right_side_self_cycle},
        {"invalid_two_cell_cycle", invalid_two_cell_cycle},
        {"invalid_long_cycle", invalid_long_cycle},
        {"invalid_add_overflow", invalid_add_overflow},
        {"invalid_sub_overflow", invalid_sub_overflow},
        {"invalid_mul_overflow", invalid_mul_overflow},
        {"invalid_div_overflow", invalid_div_overflow}
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
