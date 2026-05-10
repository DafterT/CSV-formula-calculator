#include "table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool set_table_error(CsvError *error, CsvErrorCode code, size_t line, size_t field, const char *message)
{
    if (error == NULL) {
        return false;
    }

    error->code = code;
    error->line = line;
    error->field = field;
    (void)snprintf(error->message, sizeof(error->message), "%s", message);
    return false;
}

static int compare_row_lookup(const void *left, const void *right)
{
    const RowLookup *left_row = left;
    const RowLookup *right_row = right;

    if (left_row->number < right_row->number) {
        return -1;
    }

    if (left_row->number > right_row->number) {
        return 1;
    }

    return 0;
}

static int compare_column_lookup(const void *left, const void *right)
{
    const ColumnLookup *left_column = left;
    const ColumnLookup *right_column = right;

    return strcmp(left_column->name, right_column->name);
}

static char *copy_string(const char *text)
{
    size_t length = strlen(text);
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1U);
    return copy;
}

static void *grow_array(void *items, size_t item_size, size_t *capacity, size_t needed)
{
    size_t new_capacity = *capacity;
    void *grown = NULL;

    if (needed <= *capacity) {
        return items;
    }

    if (new_capacity == 0U) {
        new_capacity = 4U;
    }

    while (new_capacity < needed) {
        if (new_capacity > (SIZE_MAX / 2U)) {
            return NULL;
        }
        new_capacity *= 2U;
    }

    if (new_capacity > (SIZE_MAX / item_size)) {
        return NULL;
    }

    grown = realloc(items, new_capacity * item_size);
    if (grown == NULL) {
        return NULL;
    }

    *capacity = new_capacity;
    return grown;
}

void table_init(Table *table)
{
    table->columns = NULL;
    table->column_count = 0U;
    table->column_capacity = 0U;

    table->rows = NULL;
    table->row_count = 0U;
    table->row_capacity = 0U;

    table->cells = NULL;
    table->cell_count = 0U;
    table->cell_capacity = 0U;

    table->row_lookup = NULL;
    table->column_lookup = NULL;
}

void table_free(Table *table)
{
    size_t index = 0U;

    if (table == NULL) {
        return;
    }

    while (index < table->column_count) {
        free(table->columns[index].name);
        index++;
    }

    index = 0U;
    while (index < table->cell_count) {
        if (table->cells[index].kind == CELL_FORMULA) {
            formula_free(&table->cells[index].formula);
        }
        index++;
    }

    free(table->columns);
    free(table->rows);
    free(table->cells);
    free(table->row_lookup);
    free(table->column_lookup);

    table_init(table);
}

bool table_add_column(Table *table, const char *name, size_t source_field)
{
    Column *columns = NULL;
    char *name_copy = copy_string(name);

    if (name_copy == NULL) {
        return false;
    }

    columns = grow_array(
        table->columns,
        sizeof(table->columns[0]),
        &table->column_capacity,
        table->column_count + 1U
    );
    if (columns == NULL) {
        free(name_copy);
        return false;
    }

    table->columns = columns;
    table->columns[table->column_count].name = name_copy;
    table->columns[table->column_count].source_field = source_field;
    table->column_count++;
    return true;
}

bool table_add_row(Table *table, int64_t number, size_t source_line, size_t *row_index)
{
    Row *rows = grow_array(
        table->rows,
        sizeof(table->rows[0]),
        &table->row_capacity,
        table->row_count + 1U
    );

    if (rows == NULL) {
        return false;
    }

    table->rows = rows;
    table->rows[table->row_count].number = number;
    table->rows[table->row_count].first_cell = table->cell_count;
    table->rows[table->row_count].source_line = source_line;

    if (row_index != NULL) {
        *row_index = table->row_count;
    }

    table->row_count++;
    return true;
}

bool table_add_number_cell(Table *table, int64_t value, size_t source_line, size_t source_field)
{
    Cell *cells = grow_array(
        table->cells,
        sizeof(table->cells[0]),
        &table->cell_capacity,
        table->cell_count + 1U
    );

    if (cells == NULL) {
        return false;
    }

    table->cells = cells;
    table->cells[table->cell_count].kind = CELL_NUMBER;
    table->cells[table->cell_count].state = EVAL_DONE;
    table->cells[table->cell_count].value = value;
    memset(&table->cells[table->cell_count].formula, 0, sizeof(table->cells[table->cell_count].formula));
    table->cells[table->cell_count].source_line = source_line;
    table->cells[table->cell_count].source_field = source_field;
    table->cell_count++;
    return true;
}

bool table_add_formula_cell(Table *table, ParsedFormula *formula, size_t source_line, size_t source_field)
{
    Cell *cells = NULL;

    cells = grow_array(
        table->cells,
        sizeof(table->cells[0]),
        &table->cell_capacity,
        table->cell_count + 1U
    );
    if (cells == NULL) {
        return false;
    }

    table->cells = cells;
    table->cells[table->cell_count].kind = CELL_FORMULA;
    table->cells[table->cell_count].state = EVAL_NOT_VISITED;
    table->cells[table->cell_count].value = 0;
    table->cells[table->cell_count].formula = *formula;
    table->cells[table->cell_count].source_line = source_line;
    table->cells[table->cell_count].source_field = source_field;
    table->cell_count++;
    return true;
}

static bool validate_unique_rows(const Table *table, CsvError *error)
{
    size_t index = 1U;

    while (index < table->row_count) {
        const RowLookup *previous = &table->row_lookup[index - 1U];
        const RowLookup *current = &table->row_lookup[index];

        if (previous->number == current->number) {
            const Row *row = &table->rows[current->row_index];
            char message[256];

            (void)snprintf(message, sizeof(message), "duplicate row number %lld", (long long)current->number);
            return set_table_error(error, CSV_ERROR_DUPLICATE_ROW, row->source_line, 1U, message);
        }

        index++;
    }

    return true;
}

static bool validate_unique_columns(const Table *table, CsvError *error)
{
    size_t index = 1U;

    while (index < table->column_count) {
        const ColumnLookup *previous = &table->column_lookup[index - 1U];
        const ColumnLookup *current = &table->column_lookup[index];

        if (strcmp(previous->name, current->name) == 0) {
            const Column *column = &table->columns[current->column_index];
            char message[256];

            (void)snprintf(message, sizeof(message), "duplicate column '%s'", current->name);
            return set_table_error(error, CSV_ERROR_DUPLICATE_COLUMN, 1U, column->source_field, message);
        }

        index++;
    }

    return true;
}

bool table_build_lookups(Table *table, CsvError *error)
{
    size_t index = 0U;

    free(table->row_lookup);
    free(table->column_lookup);
    table->row_lookup = NULL;
    table->column_lookup = NULL;

    if (table->row_count > 0U) {
        table->row_lookup = malloc(table->row_count * sizeof(table->row_lookup[0]));
        if (table->row_lookup == NULL) {
            return false;
        }
    }

    if (table->column_count > 0U) {
        table->column_lookup = malloc(table->column_count * sizeof(table->column_lookup[0]));
        if (table->column_lookup == NULL) {
            free(table->row_lookup);
            table->row_lookup = NULL;
            return false;
        }
    }

    while (index < table->row_count) {
        table->row_lookup[index].number = table->rows[index].number;
        table->row_lookup[index].row_index = index;
        index++;
    }

    index = 0U;
    while (index < table->column_count) {
        table->column_lookup[index].name = table->columns[index].name;
        table->column_lookup[index].column_index = index;
        index++;
    }

    if (table->row_count > 1U) {
        qsort(table->row_lookup, table->row_count, sizeof(table->row_lookup[0]), compare_row_lookup);
    }

    if (table->column_count > 1U) {
        qsort(table->column_lookup, table->column_count, sizeof(table->column_lookup[0]), compare_column_lookup);
    }

    if (!validate_unique_rows(table, error) ||
        !validate_unique_columns(table, error)) {
        return false;
    }

    return true;
}

bool table_find_row(const Table *table, int64_t number, size_t *row_index)
{
    size_t left = 0U;
    size_t right = table->row_count;

    while (left < right) {
        size_t middle = left + ((right - left) / 2U);
        int64_t middle_number = table->row_lookup[middle].number;

        if (middle_number == number) {
            *row_index = table->row_lookup[middle].row_index;
            return true;
        }

        if (middle_number < number) {
            left = middle + 1U;
        } else {
            right = middle;
        }
    }

    return false;
}

bool table_find_column(const Table *table, const char *name, size_t *column_index)
{
    size_t left = 0U;
    size_t right = table->column_count;

    while (left < right) {
        size_t middle = left + ((right - left) / 2U);
        int comparison = strcmp(table->column_lookup[middle].name, name);

        if (comparison == 0) {
            *column_index = table->column_lookup[middle].column_index;
            return true;
        }

        if (comparison < 0) {
            left = middle + 1U;
        } else {
            right = middle;
        }
    }

    return false;
}

size_t table_cell_index(const Table *table, size_t row_index, size_t column_index)
{
    return table->rows[row_index].first_cell + column_index;
}

Cell *table_cell_at(Table *table, size_t row_index, size_t column_index)
{
    if (row_index >= table->row_count || column_index >= table->column_count) {
        return NULL;
    }

    return &table->cells[table_cell_index(table, row_index, column_index)];
}

const Cell *table_cell_at_const(const Table *table, size_t row_index, size_t column_index)
{
    if (row_index >= table->row_count || column_index >= table->column_count) {
        return NULL;
    }

    return &table->cells[table_cell_index(table, row_index, column_index)];
}
