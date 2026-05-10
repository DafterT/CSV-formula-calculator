#ifndef CSVREADER_TABLE_H
#define CSVREADER_TABLE_H

#include "formula.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CELL_NUMBER,
    CELL_FORMULA
} CellKind;

typedef enum {
    EVAL_NOT_VISITED,
    EVAL_VISITING,
    EVAL_DONE
} EvalState;

typedef struct {
    CellKind kind;       /* Тип ячейки: число или формула. */
    EvalState state;     /* Состояние для будущего вычисления зависимостей. */
    int64_t value;       /* Число или будущий результат формулы. */
    ParsedFormula formula; /* Разобранная формула для CELL_FORMULA. */
    size_t source_line;  /* Строка исходного CSV для диагностики. */
    size_t source_field; /* Поле исходного CSV для диагностики. */
} Cell;

typedef struct {
    char *name;          /* Собственная копия имени колонки. */
    size_t source_field; /* Поле header, где объявлена колонка. */
} Column;

typedef struct {
    int64_t number;      /* Номер строки из первого столбца. */
    size_t first_cell;   /* Смещение первой ячейки строки во flat-массиве. */
    size_t source_line;  /* Номер строки во входном CSV. */
} Row;

typedef struct {
    int64_t number;      /* Номер строки для будущего быстрого поиска. */
    size_t row_index;    /* Индекс строки в Table.rows. */
} RowLookup;

typedef struct {
    const char *name;    /* Указатель на имя из Table.columns. */
    size_t column_index; /* Индекс колонки в Table.columns. */
} ColumnLookup;

typedef struct {
    Column *columns;
    size_t column_count;
    size_t column_capacity;

    Row *rows;
    size_t row_count;
    size_t row_capacity;

    Cell *cells;         /* Flat-массив: row.first_cell + column_index. */
    size_t cell_count;
    size_t cell_capacity;

    RowLookup *row_lookup;
    ColumnLookup *column_lookup;
} Table;

void table_init(Table *table);
void table_free(Table *table);

bool table_add_column(Table *table, const char *name, size_t source_field);
bool table_add_row(Table *table, int64_t number, size_t source_line, size_t *row_index);
bool table_add_number_cell(Table *table, int64_t value, size_t source_line, size_t source_field);
bool table_add_formula_cell(Table *table, ParsedFormula *formula, size_t source_line, size_t source_field);
bool table_build_lookups(Table *table, CsvError *error);
bool table_find_row(const Table *table, int64_t number, size_t *row_index);
bool table_find_column(const Table *table, const char *name, size_t *column_index);

size_t table_cell_index(const Table *table, size_t row_index, size_t column_index);
Cell *table_cell_at(Table *table, size_t row_index, size_t column_index);
const Cell *table_cell_at_const(const Table *table, size_t row_index, size_t column_index);

#endif
