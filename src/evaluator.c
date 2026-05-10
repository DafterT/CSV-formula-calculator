#include "evaluator.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    Cell *cell;
} EvalFrame;

typedef struct {
    EvalFrame *items;
    size_t count;
    size_t capacity;
} EvalStack;

typedef enum {
    DEPENDENCY_DONE,
    DEPENDENCY_PUSHED,
    DEPENDENCY_ERROR
} DependencyResult;

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

static void eval_stack_free(EvalStack *stack)
{
    free(stack->items);
    stack->items = NULL;
    stack->count = 0U;
    stack->capacity = 0U;
}

static bool eval_stack_push(EvalStack *stack, Cell *cell, CsvError *error)
{
    EvalFrame *grown = NULL;
    size_t new_capacity = stack->capacity;

    if (stack->count == stack->capacity) {
        if (new_capacity == 0U) {
            new_capacity = 64U;
        } else {
            if (new_capacity > (SIZE_MAX / 2U)) {
                return csv_error_set(error, CSV_ERROR_OUT_OF_MEMORY, 0U, 0U, "out of memory while evaluating formulas");
            }
            new_capacity *= 2U;
        }

        if (new_capacity > (SIZE_MAX / sizeof(stack->items[0]))) {
            return csv_error_set(error, CSV_ERROR_OUT_OF_MEMORY, 0U, 0U, "out of memory while evaluating formulas");
        }

        grown = realloc(stack->items, new_capacity * sizeof(stack->items[0]));
        if (grown == NULL) {
            return csv_error_set(error, CSV_ERROR_OUT_OF_MEMORY, 0U, 0U, "out of memory while evaluating formulas");
        }

        stack->items = grown;
        stack->capacity = new_capacity;
    }

    stack->items[stack->count].cell = cell;
    stack->count++;
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
        return csv_error_set(
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

static Cell *cell_for_ref(Table *table, const FormulaRef *ref)
{
    return table_cell_at(table, ref->row_index, ref->column_index);
}

static bool arg_value(Table *table, const FormulaArg *arg, int64_t *value)
{
    Cell *target = NULL;

    if (arg->kind == FORMULA_ARG_NUMBER) {
        *value = arg->as.number;
        return true;
    }

    target = cell_for_ref(table, &arg->as.ref);
    if (target == NULL) {
        return false;
    }

    *value = target->value;
    return true;
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
                return csv_error_set(
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
        return csv_error_set(
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

static DependencyResult ensure_dependency_evaluated(Table *table, const FormulaRef *ref, EvalStack *stack, CsvError *error)
{
    Cell *target = NULL;

    target = cell_for_ref(table, ref);
    if (target == NULL) {
        csv_error_set(error, CSV_ERROR_INVALID_REFERENCE, 0U, 0U, "invalid resolved reference");
        return DEPENDENCY_ERROR;
    }

    if (target->state == EVAL_VISITING) {
        csv_error_set(
            error,
            CSV_ERROR_CYCLIC_DEPENDENCY,
            target->source_line,
            target->source_field,
            "cyclic dependency at line %zu field %zu",
            target->source_line,
            target->source_field
        );
        return DEPENDENCY_ERROR;
    }

    if (target->state == EVAL_NOT_VISITED) {
        if (!eval_stack_push(stack, target, error)) {
            return DEPENDENCY_ERROR;
        }
        return DEPENDENCY_PUSHED;
    }

    return DEPENDENCY_DONE;
}

static bool evaluate_cell(Table *table, Cell *root, int64_t *value, CsvError *error)
{
    EvalStack stack = {0};

    if (root->state == EVAL_DONE) {
        *value = root->value;
        return true;
    }

    if (!eval_stack_push(&stack, root, error)) {
        return false;
    }

    while (stack.count > 0U) {
        Cell *cell = stack.items[stack.count - 1U].cell;
        DependencyResult dependency = DEPENDENCY_DONE;

        if (cell->state == EVAL_DONE) {
            stack.count--;
            continue;
        }

        if (cell->state == EVAL_NOT_VISITED) {
            cell->state = EVAL_VISITING;
        }

        if (cell->formula.left.kind == FORMULA_ARG_REFERENCE) {
            dependency = ensure_dependency_evaluated(table, &cell->formula.left.as.ref, &stack, error);
            if (dependency == DEPENDENCY_ERROR) {
                eval_stack_free(&stack);
                return false;
            }
            if (dependency == DEPENDENCY_PUSHED) {
                continue;
            }
        }

        if (cell->formula.right.kind == FORMULA_ARG_REFERENCE) {
            dependency = ensure_dependency_evaluated(table, &cell->formula.right.as.ref, &stack, error);
            if (dependency == DEPENDENCY_ERROR) {
                eval_stack_free(&stack);
                return false;
            }
            if (dependency == DEPENDENCY_PUSHED) {
                continue;
            }
        }

        {
            int64_t left = 0;
            int64_t right = 0;
            int64_t result = 0;

            if (!arg_value(table, &cell->formula.left, &left) ||
                !arg_value(table, &cell->formula.right, &right) ||
                !apply_operator(cell, left, right, &result, error)) {
                eval_stack_free(&stack);
                return false;
            }

            cell->value = result;
            cell->state = EVAL_DONE;
            stack.count--;
        }
    }

    *value = root->value;
    eval_stack_free(&stack);
    return true;
}

bool table_evaluate(Table *table, CsvError *error)
{
    size_t index = 0U;

    if (table == NULL ||
        (table->row_count > 0U && table->row_lookup == NULL) ||
        (table->column_count > 0U && table->column_lookup == NULL)) {
        return csv_error_set(error, CSV_ERROR_MALFORMED, 0U, 0U, "table lookup tables are not built");
    }

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
