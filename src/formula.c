#include "formula.h"
#include "int_parse.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *cursor;
    CsvError *error;
    size_t source_line;
    size_t source_field;
} FormulaParser;

static void formula_init(ParsedFormula *formula)
{
    memset(formula, 0, sizeof(*formula));
}

static bool is_column_char(int ch)
{
    unsigned char value = (unsigned char)ch;

    return isalpha(value) || value == '_';
}

static bool is_digit_char(int ch)
{
    return isdigit((unsigned char)ch) != 0;
}

static char *copy_range(const char *start, size_t length)
{
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static bool parse_number(FormulaParser *parser, FormulaArg *arg)
{
    int64_t value = 0;
    size_t consumed = 0U;
    IntParseResult result = int_parse_signed_int64(parser->cursor, false, &value, &consumed);

    if (result == INT_PARSE_INVALID) {
        return csv_error_set(
            parser->error,
            CSV_ERROR_INVALID_FORMULA,
            parser->source_line,
            parser->source_field,
            "invalid formula at line %zu field %zu",
            parser->source_line,
            parser->source_field
        );
    }

    if (result == INT_PARSE_OVERFLOW) {
        return csv_error_set(
            parser->error,
            CSV_ERROR_INTEGER_OVERFLOW,
            parser->source_line,
            parser->source_field,
            "integer overflow in formula at line %zu field %zu",
            parser->source_line,
            parser->source_field
        );
    }

    arg->kind = FORMULA_ARG_NUMBER;
    arg->as.number = value;
    parser->cursor += consumed;
    return true;
}

static bool parse_row_number(FormulaParser *parser, int64_t *row_number)
{
    size_t consumed = 0U;
    IntParseResult result = int_parse_positive_row_number(parser->cursor, false, row_number, &consumed);

    if (result == INT_PARSE_INVALID) {
        return csv_error_set(
            parser->error,
            CSV_ERROR_INVALID_FORMULA,
            parser->source_line,
            parser->source_field,
            "invalid cell reference syntax at line %zu field %zu",
            parser->source_line,
            parser->source_field
        );
    }

    if (result == INT_PARSE_OVERFLOW) {
        return csv_error_set(
            parser->error,
            CSV_ERROR_INTEGER_OVERFLOW,
            parser->source_line,
            parser->source_field,
            "integer overflow in formula at line %zu field %zu",
            parser->source_line,
            parser->source_field
        );
    }

    parser->cursor += consumed;
    return true;
}

static bool parse_reference(FormulaParser *parser, FormulaArg *arg)
{
    const char *name_start = parser->cursor;
    const char *current = parser->cursor;
    size_t name_length = 0U;
    int64_t row_number = 0;
    char *column_name = NULL;

    while (is_column_char(*current)) {
        current++;
    }

    name_length = (size_t)(current - name_start);
    if (!is_digit_char(*current)) {
        return csv_error_set(
            parser->error,
            CSV_ERROR_INVALID_FORMULA,
            parser->source_line,
            parser->source_field,
            "invalid cell reference syntax at line %zu field %zu",
            parser->source_line,
            parser->source_field
        );
    }

    parser->cursor = current;
    if (!parse_row_number(parser, &row_number)) {
        return false;
    }

    column_name = copy_range(name_start, name_length);
    if (column_name == NULL) {
        return csv_error_set(
            parser->error,
            CSV_ERROR_OUT_OF_MEMORY,
            parser->source_line,
            parser->source_field,
            "out of memory while parsing formula at line %zu field %zu",
            parser->source_line,
            parser->source_field
        );
    }

    arg->kind = FORMULA_ARG_REFERENCE;
    arg->as.ref.column_name = column_name;
    arg->as.ref.row_number = row_number;
    arg->as.ref.row_index = SIZE_MAX;
    arg->as.ref.column_index = SIZE_MAX;
    arg->as.ref.resolved = false;
    return true;
}

static bool parse_arg(FormulaParser *parser, FormulaArg *arg)
{
    if (*parser->cursor == '-' || is_digit_char(*parser->cursor)) {
        return parse_number(parser, arg);
    }

    if (is_column_char(*parser->cursor)) {
        return parse_reference(parser, arg);
    }

    return csv_error_set(
        parser->error,
        CSV_ERROR_INVALID_FORMULA,
        parser->source_line,
        parser->source_field,
        "invalid formula at line %zu field %zu",
        parser->source_line,
        parser->source_field
    );
}

static bool parse_operator(FormulaParser *parser, FormulaOp *op)
{
    switch (*parser->cursor) {
        case '+':
            *op = FORMULA_OP_ADD;
            parser->cursor++;
            return true;
        case '-':
            *op = FORMULA_OP_SUB;
            parser->cursor++;
            return true;
        case '*':
            *op = FORMULA_OP_MUL;
            parser->cursor++;
            return true;
        case '/':
            *op = FORMULA_OP_DIV;
            parser->cursor++;
            return true;
        default:
            return csv_error_set(
                parser->error,
                CSV_ERROR_INVALID_FORMULA,
                parser->source_line,
                parser->source_field,
                "invalid formula at line %zu field %zu",
                parser->source_line,
                parser->source_field
            );
    }
}

void formula_free(ParsedFormula *formula)
{
    if (formula == NULL) {
        return;
    }

    if (formula->left.kind == FORMULA_ARG_REFERENCE) {
        free(formula->left.as.ref.column_name);
    }

    if (formula->right.kind == FORMULA_ARG_REFERENCE) {
        free(formula->right.as.ref.column_name);
    }

    formula_init(formula);
}

bool formula_parse_text(
    const char *text,
    ParsedFormula *formula,
    CsvError *error,
    size_t source_line,
    size_t source_field
)
{
    FormulaParser parser;

    formula_init(formula);

    csv_error_clear(error);

    parser.cursor = text;
    parser.error = error;
    parser.source_line = source_line;
    parser.source_field = source_field;

    if (text == NULL || *parser.cursor != '=') {
        return csv_error_set(
            parser.error,
            CSV_ERROR_INVALID_FORMULA,
            parser.source_line,
            parser.source_field,
            "invalid formula at line %zu field %zu",
            source_line,
            source_field
        );
    }

    parser.cursor++;

    if (!parse_arg(&parser, &formula->left) ||
        !parse_operator(&parser, &formula->op) ||
        !parse_arg(&parser, &formula->right) ||
        *parser.cursor != '\0') {
        if (*parser.cursor != '\0' && error != NULL && error->code == CSV_ERROR_NONE) {
            (void)csv_error_set(
                parser.error,
                CSV_ERROR_INVALID_FORMULA,
                parser.source_line,
                parser.source_field,
                "invalid formula at line %zu field %zu",
                source_line,
                source_field
            );
        }
        formula_free(formula);
        if (error != NULL && error->code == CSV_ERROR_NONE) {
            (void)csv_error_set(
                parser.error,
                CSV_ERROR_INVALID_FORMULA,
                parser.source_line,
                parser.source_field,
                "invalid formula at line %zu field %zu",
                source_line,
                source_field
            );
        }
        return false;
    }

    return true;
}
