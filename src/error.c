#include "error.h"

#include <stdarg.h>
#include <stdio.h>

void csv_error_clear(CsvError *error)
{
    if (error == NULL) {
        return;
    }

    error->code = CSV_ERROR_NONE;
    error->line = 0U;
    error->field = 0U;
    error->message[0] = '\0';
}

bool csv_error_set(CsvError *error, CsvErrorCode code, size_t line, size_t field, const char *format, ...)
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
