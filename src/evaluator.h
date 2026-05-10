#ifndef CSVREADER_EVALUATOR_H
#define CSVREADER_EVALUATOR_H

#include "error.h"
#include "table.h"

#include <stdbool.h>

bool table_resolve_references(Table *table, CsvError *error);
bool table_evaluate(Table *table, CsvError *error);

#endif
