#include "csv_parser.h"
#include "evaluator.h"

#include <inttypes.h>
#include <stdio.h>

static void print_table(const Table *table)
{
    size_t column = 0U;
    size_t row = 0U;

    putchar(',');
    while (column < table->column_count) {
        if (column > 0U) {
            putchar(',');
        }
        fputs(table->columns[column].name, stdout);
        column++;
    }
    putchar('\n');

    while (row < table->row_count) {
        printf("%" PRId64, table->rows[row].number);

        column = 0U;
        while (column < table->column_count) {
            const Cell *cell = table_cell_at_const(table, row, column);

            putchar(',');
            printf("%" PRId64, cell->value);
            column++;
        }
        putchar('\n');
        row++;
    }
}

int main(int argc, char **argv)
{
    Table table;
    CsvError error;

    if (argc != 2) {
        fprintf(stderr, "error: usage: %s file.csv\n", argv[0]);
        return 1;
    }

    if (!csv_parse_file(argv[1], &table, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        return 1;
    }

    if (!table_evaluate(&table, &error)) {
        fprintf(stderr, "error: %s\n", error.message);
        table_free(&table);
        return 1;
    }

    print_table(&table);
    table_free(&table);
    return 0;
}
