#ifndef CSVREADER_INT_PARSE_H
#define CSVREADER_INT_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    INT_PARSE_OK,
    INT_PARSE_INVALID,
    INT_PARSE_OVERFLOW
} IntParseResult;

IntParseResult int_parse_signed_int64(const char *text, bool require_end, int64_t *value, size_t *consumed);
IntParseResult int_parse_positive_row_number(const char *text, bool require_end, int64_t *row_number, size_t *consumed);

#endif
