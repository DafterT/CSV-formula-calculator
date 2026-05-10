#include "csv_parser.h"

#include <inttypes.h>
#include <stdio.h>

static char formula_op_char(FormulaOp op)
{
    switch (op) {
        case FORMULA_OP_ADD:
            return '+';
        case FORMULA_OP_SUB:
            return '-';
        case FORMULA_OP_MUL:
            return '*';
        case FORMULA_OP_DIV:
            return '/';
        default:
            return '?';
    }
}

static void print_formula_arg(const FormulaArg *arg)
{
    if (arg->kind == FORMULA_ARG_REFERENCE) {
        printf("%s%" PRId64, arg->as.ref.column_name, arg->as.ref.row_number);
    } else {
        printf("%" PRId64, arg->as.number);
    }
}

static void print_formula(const ParsedFormula *formula)
{
    putchar('=');
    print_formula_arg(&formula->left);
    putchar(formula_op_char(formula->op));
    print_formula_arg(&formula->right);
}

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
            if (cell->kind == CELL_FORMULA) {
                print_formula(&cell->formula);
            } else {
                printf("%" PRId64, cell->value);
            }
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

    print_table(&table);
    table_free(&table);
    return 0;
}
