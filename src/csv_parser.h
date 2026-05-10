#ifndef CSVREADER_CSV_PARSER_H
#define CSVREADER_CSV_PARSER_H

#include "error.h"
#include "table.h"

#include <stdbool.h>
#include <stdio.h>

bool csv_parse_stream(FILE *stream, Table *table, CsvError *error);
bool csv_parse_file(const char *path, Table *table, CsvError *error);

#endif
