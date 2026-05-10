#include "evaluator.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

static bool set_error(CsvError *error, CsvErrorCode code, size_t line, size_t field, const char *format, ...)
{
    va_list args;

    if (error == NULL) {
        return false;
    }

    error->code = code;
    error->line = line;
    error->field = field;

    va_start(args, format);
    (void)vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);

    return false;
}

static bool checked_add_i64(int64_t left, int64_t right, int64_t *result)
{
    if ((right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) {
        return false;
    }

    *result = left + right;
    return true;
}

static bool checked_sub_i64(int64_t left, int64_t right, int64_t *result)
{
    if ((right < 0 && left > INT64_MAX + right) ||
        (right > 0 && left < INT64_MIN + right)) {
        return false;
    }

    *result = left - right;
    return true;
}

static bool checked_mul_i64(int64_t left, int64_t right, int64_t *result)
{
    if (left == 0 || right == 0) {
        *result = 0;
        return true;
    }

    if ((left == INT64_MIN && right == -1) ||
        (right == INT64_MIN && left == -1)) {
        return false;
    }

    if (left > 0) {
        if ((right > 0 && left > INT64_MAX / right) ||
            (right < 0 && right < INT64_MIN / left)) {
            return false;
        }
    } else {
        if ((right > 0 && left < INT64_MIN / right) ||
            (right < 0 && left < INT64_MAX / right)) {
            return false;
        }
    }

    *result = left * right;
    return true;
}

static bool checked_div_i64(int64_t left, int64_t right, int64_t *result)
{
    if (right == 0) {
        return false;
    }

    if (left == INT64_MIN && right == -1) {
        return false;
    }

    *result = left / right;
    return true;
}

static bool resolve_ref(Table *table, FormulaRef *ref, const Cell *source_cell, CsvError *error)
{
    size_t row_index = 0U;
    size_t column_index = 0U;

    if (ref->resolved) {
        return true;
    }

    if (!table_find_column(table, ref->column_name, &column_index) ||
        !table_find_row(table, ref->row_number, &row_index)) {
        return set_error(
            error,
            CSV_ERROR_INVALID_REFERENCE,
            source_cell->source_line,
            source_cell->source_field,
            "invalid reference at line %zu field %zu",
            source_cell->source_line,
            source_cell->source_field
        );
    }

    ref->row_index = row_index;
    ref->column_index = column_index;
    ref->resolved = true;
    return true;
}

static bool resolve_arg(Table *table, FormulaArg *arg, const Cell *source_cell, CsvError *error)
{
    if (arg->kind == FORMULA_ARG_NUMBER) {
        return true;
    }

    return resolve_ref(table, &arg->as.ref, source_cell, error);
}

bool table_resolve_references(Table *table, CsvError *error)
{
    size_t index = 0U;

    while (index < table->cell_count) {
        Cell *cell = &table->cells[index];

        if (cell->kind == CELL_FORMULA) {
            if (!resolve_arg(table, &cell->formula.left, cell, error) ||
                !resolve_arg(table, &cell->formula.right, cell, error)) {
                return false;
            }
        }

        index++;
    }

    return true;
}

static bool evaluate_cell(Table *table, Cell *cell, int64_t *value, CsvError *error);

static bool evaluate_arg(Table *table, const FormulaArg *arg, int64_t *value, CsvError *error)
{
    Cell *target = NULL;

    if (arg->kind == FORMULA_ARG_NUMBER) {
        *value = arg->as.number;
        return true;
    }

    target = table_cell_at(table, arg->as.ref.row_index, arg->as.ref.column_index);
    return evaluate_cell(table, target, value, error);
}

static bool apply_operator(const Cell *cell, int64_t left, int64_t right, int64_t *result, CsvError *error)
{
    bool ok = false;

    switch (cell->formula.op) {
        case FORMULA_OP_ADD:
            ok = checked_add_i64(left, right, result);
            break;
        case FORMULA_OP_SUB:
            ok = checked_sub_i64(left, right, result);
            break;
        case FORMULA_OP_MUL:
            ok = checked_mul_i64(left, right, result);
            break;
        case FORMULA_OP_DIV:
            if (right == 0) {
                return set_error(
                    error,
                    CSV_ERROR_DIVISION_BY_ZERO,
                    cell->source_line,
                    cell->source_field,
                    "division by zero at line %zu field %zu",
                    cell->source_line,
                    cell->source_field
                );
            }
            ok = checked_div_i64(left, right, result);
            break;
        default:
            ok = false;
            break;
    }

    if (!ok) {
        return set_error(
            error,
            CSV_ERROR_INTEGER_OVERFLOW,
            cell->source_line,
            cell->source_field,
            "integer overflow while evaluating formula at line %zu field %zu",
            cell->source_line,
            cell->source_field
        );
    }

    return true;
}

static bool evaluate_cell(Table *table, Cell *cell, int64_t *value, CsvError *error)
{
    int64_t left = 0;
    int64_t right = 0;
    int64_t result = 0;

    if (cell->state == EVAL_DONE) {
        *value = cell->value;
        return true;
    }

    if (cell->state == EVAL_VISITING) {
        return set_error(
            error,
            CSV_ERROR_CYCLIC_DEPENDENCY,
            cell->source_line,
            cell->source_field,
            "cyclic dependency at line %zu field %zu",
            cell->source_line,
            cell->source_field
        );
    }

    cell->state = EVAL_VISITING;
    if (!evaluate_arg(table, &cell->formula.left, &left, error) ||
        !evaluate_arg(table, &cell->formula.right, &right, error) ||
        !apply_operator(cell, left, right, &result, error)) {
        return false;
    }

    cell->value = result;
    cell->state = EVAL_DONE;
    *value = result;
    return true;
}

bool table_evaluate(Table *table, CsvError *error)
{
    size_t index = 0U;

    if (!table_resolve_references(table, error)) {
        return false;
    }

    while (index < table->cell_count) {
        Cell *cell = &table->cells[index];
        int64_t value = 0;

        if (!evaluate_cell(table, cell, &value, error)) {
            return false;
        }

        index++;
    }

    return true;
}
