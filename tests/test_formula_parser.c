#include "formula.h"

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

static bool expect_number(const FormulaArg *arg, int64_t value)
{
    EXPECT_EQ_INT64(FORMULA_ARG_NUMBER, arg->kind);
    EXPECT_EQ_INT64(value, arg->as.number);
    return true;
}

static bool expect_reference(const FormulaArg *arg, const char *column_name, int64_t row_number)
{
    EXPECT_EQ_INT64(FORMULA_ARG_REFERENCE, arg->kind);
    EXPECT_STREQ(column_name, arg->as.ref.column_name);
    EXPECT_EQ_INT64(row_number, arg->as.ref.row_number);
    EXPECT_FALSE(arg->as.ref.resolved);
    EXPECT_EQ_SIZE(SIZE_MAX, arg->as.ref.row_index);
    EXPECT_EQ_SIZE(SIZE_MAX, arg->as.ref.column_index);
    return true;
}

static bool parse_formula(const char *text, ParsedFormula *formula, CsvError *error)
{
    return formula_parse_text(text, formula, error, 2U, 3U);
}

static bool expect_error(const char *text, CsvErrorCode code)
{
    ParsedFormula formula;
    CsvError error;
    bool ok = parse_formula(text, &formula, &error);

    EXPECT_FALSE(ok);
    EXPECT_EQ_CODE(code, error.code);
    formula_free(&formula);
    return true;
}

static bool formula_refs_add(void)
{
    ParsedFormula formula;
    CsvError error;

    EXPECT_TRUE(parse_formula("=A1+Cell30", &formula, &error));
    EXPECT_TRUE(expect_reference(&formula.left, "A", 1));
    EXPECT_EQ_INT64(FORMULA_OP_ADD, formula.op);
    EXPECT_TRUE(expect_reference(&formula.right, "Cell", 30));
    formula_free(&formula);
    return true;
}

static bool formula_ref_ref(void)
{
    ParsedFormula formula;
    CsvError error;

    EXPECT_TRUE(parse_formula("=B1+A1", &formula, &error));
    EXPECT_TRUE(expect_reference(&formula.left, "B", 1));
    EXPECT_EQ_INT64(FORMULA_OP_ADD, formula.op);
    EXPECT_TRUE(expect_reference(&formula.right, "A", 1));
    formula_free(&formula);
    return true;
}

static bool formula_number_mul_ref(void)
{
    ParsedFormula formula;
    CsvError error;

    EXPECT_TRUE(parse_formula("=10*A2", &formula, &error));
    EXPECT_TRUE(expect_number(&formula.left, 10));
    EXPECT_EQ_INT64(FORMULA_OP_MUL, formula.op);
    EXPECT_TRUE(expect_reference(&formula.right, "A", 2));
    formula_free(&formula);
    return true;
}

static bool formula_ref_div_number(void)
{
    ParsedFormula formula;
    CsvError error;

    EXPECT_TRUE(parse_formula("=A1/2", &formula, &error));
    EXPECT_TRUE(expect_reference(&formula.left, "A", 1));
    EXPECT_EQ_INT64(FORMULA_OP_DIV, formula.op);
    EXPECT_TRUE(expect_number(&formula.right, 2));
    formula_free(&formula);
    return true;
}

static bool formula_negative_left_number(void)
{
    ParsedFormula formula;
    CsvError error;

    EXPECT_TRUE(parse_formula("=-10+A1", &formula, &error));
    EXPECT_TRUE(expect_number(&formula.left, -10));
    EXPECT_EQ_INT64(FORMULA_OP_ADD, formula.op);
    EXPECT_TRUE(expect_reference(&formula.right, "A", 1));
    formula_free(&formula);
    return true;
}

static bool formula_negative_right_number(void)
{
    ParsedFormula formula;
    CsvError error;

    EXPECT_TRUE(parse_formula("=A1*-2", &formula, &error));
    EXPECT_TRUE(expect_reference(&formula.left, "A", 1));
    EXPECT_EQ_INT64(FORMULA_OP_MUL, formula.op);
    EXPECT_TRUE(expect_number(&formula.right, -2));
    formula_free(&formula);
    return true;
}

static bool formula_sub_negative_number(void)
{
    ParsedFormula formula;
    CsvError error;

    EXPECT_TRUE(parse_formula("=A1--2", &formula, &error));
    EXPECT_TRUE(expect_reference(&formula.left, "A", 1));
    EXPECT_EQ_INT64(FORMULA_OP_SUB, formula.op);
    EXPECT_TRUE(expect_number(&formula.right, -2));
    formula_free(&formula);
    return true;
}

static bool formula_underscore_column(void)
{
    ParsedFormula formula;
    CsvError error;

    EXPECT_TRUE(parse_formula("=_A1+B2", &formula, &error));
    EXPECT_TRUE(expect_reference(&formula.left, "_A", 1));
    EXPECT_EQ_INT64(FORMULA_OP_ADD, formula.op);
    EXPECT_TRUE(expect_reference(&formula.right, "B", 2));
    formula_free(&formula);
    return true;
}

static bool formula_two_numbers(void)
{
    ParsedFormula formula;
    CsvError error;

    EXPECT_TRUE(parse_formula("=1+2", &formula, &error));
    EXPECT_TRUE(expect_number(&formula.left, 1));
    EXPECT_EQ_INT64(FORMULA_OP_ADD, formula.op);
    EXPECT_TRUE(expect_number(&formula.right, 2));
    formula_free(&formula);
    return true;
}

static bool formula_unknown_reference_syntax(void)
{
    ParsedFormula formula;
    CsvError error;

    EXPECT_TRUE(parse_formula("=C1+1", &formula, &error));
    EXPECT_TRUE(expect_reference(&formula.left, "C", 1));
    EXPECT_EQ_INT64(FORMULA_OP_ADD, formula.op);
    EXPECT_TRUE(expect_number(&formula.right, 1));
    formula_free(&formula);
    return true;
}

static bool invalid_empty_formula(void) { return expect_error("=", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_missing_operator(void) { return expect_error("=A1", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_missing_right_arg(void) { return expect_error("=A1+", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_plus_number(void) { return expect_error("=+10+A1", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_zero_reference_row(void) { return expect_error("=A0+1", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_reference_without_row(void) { return expect_error("=A+1", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_number_then_letter(void) { return expect_error("=1A+2", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_unknown_operator(void) { return expect_error("=A1^B1", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_trailing_formula(void) { return expect_error("=A1+B1+1", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_space_before_operator(void) { return expect_error("=A1 +B1", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_space_after_operator(void) { return expect_error("=A1+ B1", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_space_after_equal(void) { return expect_error("= A1+B1", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_trailing_space(void) { return expect_error("=A1+B1 ", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_space_before_number(void) { return expect_error("=A1/ 2", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_formula_overflow(void) { return expect_error("=A1+999999999999999999999999999999", CSV_ERROR_INTEGER_OVERFLOW); }
static bool invalid_negative_reference_row(void) { return expect_error("=A-1+2", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_operator_without_arg(void) { return expect_error("=A1/", CSV_ERROR_INVALID_FORMULA); }
static bool invalid_double_negative_number(void) { return expect_error("=--1+A1", CSV_ERROR_INVALID_FORMULA); }

int main(void)
{
    const TestCase tests[] = {
        {"formula_refs_add", formula_refs_add},
        {"formula_ref_ref", formula_ref_ref},
        {"formula_number_mul_ref", formula_number_mul_ref},
        {"formula_ref_div_number", formula_ref_div_number},
        {"formula_negative_left_number", formula_negative_left_number},
        {"formula_negative_right_number", formula_negative_right_number},
        {"formula_sub_negative_number", formula_sub_negative_number},
        {"formula_underscore_column", formula_underscore_column},
        {"formula_two_numbers", formula_two_numbers},
        {"formula_unknown_reference_syntax", formula_unknown_reference_syntax},
        {"invalid_empty_formula", invalid_empty_formula},
        {"invalid_missing_operator", invalid_missing_operator},
        {"invalid_missing_right_arg", invalid_missing_right_arg},
        {"invalid_plus_number", invalid_plus_number},
        {"invalid_zero_reference_row", invalid_zero_reference_row},
        {"invalid_reference_without_row", invalid_reference_without_row},
        {"invalid_number_then_letter", invalid_number_then_letter},
        {"invalid_unknown_operator", invalid_unknown_operator},
        {"invalid_trailing_formula", invalid_trailing_formula},
        {"invalid_space_before_operator", invalid_space_before_operator},
        {"invalid_space_after_operator", invalid_space_after_operator},
        {"invalid_space_after_equal", invalid_space_after_equal},
        {"invalid_trailing_space", invalid_trailing_space},
        {"invalid_space_before_number", invalid_space_before_number},
        {"invalid_formula_overflow", invalid_formula_overflow},
        {"invalid_negative_reference_row", invalid_negative_reference_row},
        {"invalid_operator_without_arg", invalid_operator_without_arg},
        {"invalid_double_negative_number", invalid_double_negative_number}
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
