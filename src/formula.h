#ifndef CSVREADER_FORMULA_H
#define CSVREADER_FORMULA_H

#include "error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FORMULA_OP_ADD,
    FORMULA_OP_SUB,
    FORMULA_OP_MUL,
    FORMULA_OP_DIV
} FormulaOp;

typedef enum {
    FORMULA_ARG_NUMBER,
    FORMULA_ARG_REFERENCE
} FormulaArgKind;

typedef struct {
    char *column_name;
    int64_t row_number;
    size_t row_index;
    size_t column_index;
    bool resolved;
} FormulaRef;

typedef struct {
    FormulaArgKind kind;
    union {
        int64_t number;
        FormulaRef ref;
    } as;
} FormulaArg;

typedef struct {
    FormulaArg left;
    FormulaOp op;
    FormulaArg right;
} ParsedFormula;

bool formula_parse_text(
    const char *text,
    ParsedFormula *formula,
    CsvError *error,
    size_t source_line,
    size_t source_field
);

void formula_free(ParsedFormula *formula);

#endif
