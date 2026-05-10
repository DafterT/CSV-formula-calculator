#ifndef CSVREADER_ERROR_H
#define CSVREADER_ERROR_H

#include <stddef.h>

typedef enum {
    CSV_ERROR_NONE,
    CSV_ERROR_IO,
    CSV_ERROR_OUT_OF_MEMORY,
    CSV_ERROR_EMPTY_FILE,
    CSV_ERROR_MALFORMED,
    CSV_ERROR_INVALID_HEADER,
    CSV_ERROR_DUPLICATE_COLUMN,
    CSV_ERROR_INVALID_ROW_NUMBER,
    CSV_ERROR_DUPLICATE_ROW,
    CSV_ERROR_INVALID_CELL,
    CSV_ERROR_INTEGER_OVERFLOW,
    CSV_ERROR_INVALID_FORMULA
} CsvErrorCode;

typedef struct {
    CsvErrorCode code;    /* Код ошибки для проверок и будущей обработки. */
    size_t line;          /* Номер строки CSV, где найдена ошибка. */
    size_t field;         /* Номер поля CSV в этой строке. */
    char message[256];    /* Готовый текст для вывода после префикса error:. */
} CsvError;

#endif
