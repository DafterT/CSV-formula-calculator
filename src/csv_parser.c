#include "csv_parser.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} FieldBuffer;

typedef enum {
    CSV_DELIM_COMMA,
    CSV_DELIM_LINE,
    CSV_DELIM_EOF
} CsvDelimiter;

typedef struct {
    FILE *stream;
    Table *table;
    CsvError *error;
    FieldBuffer field;
} Parser;

static void clear_error(CsvError *error)
{
    if (error == NULL) {
        return;
    }

    error->code = CSV_ERROR_NONE;
    error->line = 0U;
    error->field = 0U;
    error->message[0] = '\0';
}

static bool set_error(CsvError *error, CsvErrorCode code, size_t line, size_t field, const char *format, ...)
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

static bool field_buffer_init(FieldBuffer *buffer)
{
    buffer->capacity = 64U;
    buffer->length = 0U;
    buffer->data = malloc(buffer->capacity);

    if (buffer->data == NULL) {
        buffer->capacity = 0U;
        return false;
    }

    buffer->data[0] = '\0';
    return true;
}

static void field_buffer_free(FieldBuffer *buffer)
{
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = 0U;
    buffer->capacity = 0U;
}

static void field_buffer_clear(FieldBuffer *buffer)
{
    buffer->length = 0U;
    if (buffer->data != NULL) {
        buffer->data[0] = '\0';
    }
}

static bool field_buffer_append(FieldBuffer *buffer, char ch)
{
    char *grown = NULL;
    size_t new_capacity = buffer->capacity;

    if (buffer->length + 1U < buffer->capacity) {
        buffer->data[buffer->length] = ch;
        buffer->length++;
        buffer->data[buffer->length] = '\0';
        return true;
    }

    if (new_capacity == 0U) {
        new_capacity = 64U;
    }

    while (buffer->length + 1U >= new_capacity) {
        if (new_capacity > (SIZE_MAX / 2U)) {
            return false;
        }
        new_capacity *= 2U;
    }

    grown = realloc(buffer->data, new_capacity);
    if (grown == NULL) {
        return false;
    }

    buffer->data = grown;
    buffer->capacity = new_capacity;
    buffer->data[buffer->length] = ch;
    buffer->length++;
    buffer->data[buffer->length] = '\0';
    return true;
}

static bool read_field(Parser *parser, size_t line, size_t field, CsvDelimiter *delimiter)
{
    int ch = 0;

    field_buffer_clear(&parser->field);

    while ((ch = fgetc(parser->stream)) != EOF) {
        if (ch == ',') {
            *delimiter = CSV_DELIM_COMMA;
            return true;
        }

        if (ch == '\n') {
            *delimiter = CSV_DELIM_LINE;
            return true;
        }

        if (ch == '\r') {
            int next = fgetc(parser->stream);

            if (next == '\n') {
                *delimiter = CSV_DELIM_LINE;
                return true;
            }

            if (next == EOF && ferror(parser->stream)) {
                return set_error(parser->error, CSV_ERROR_IO, line, field, "I/O error while reading CSV");
            }

            return set_error(parser->error, CSV_ERROR_MALFORMED, line, field, "bare carriage return at line %zu field %zu", line, field);
        }

        if (ch == '"') {
            return set_error(parser->error, CSV_ERROR_MALFORMED, line, field, "quotes are not supported at line %zu field %zu", line, field);
        }

        if (!field_buffer_append(&parser->field, (char)ch)) {
            return set_error(parser->error, CSV_ERROR_OUT_OF_MEMORY, line, field, "out of memory while reading field");
        }
    }

    if (ferror(parser->stream)) {
        return set_error(parser->error, CSV_ERROR_IO, line, field, "I/O error while reading CSV");
    }

    *delimiter = CSV_DELIM_EOF;
    return true;
}

static bool is_valid_column_name(const char *name)
{
    size_t index = 0U;

    if (name[0] == '\0') {
        return false;
    }

    while (name[index] != '\0') {
        unsigned char ch = (unsigned char)name[index];

        if (!(isalpha(ch) || ch == '_')) {
            return false;
        }
        index++;
    }

    return true;
}

static bool has_column(const Table *table, const char *name)
{
    size_t index = 0U;

    while (index < table->column_count) {
        if (strcmp(table->columns[index].name, name) == 0) {
            return true;
        }
        index++;
    }

    return false;
}

static bool has_row(const Table *table, int64_t number)
{
    size_t index = 0U;

    while (index < table->row_count) {
        if (table->rows[index].number == number) {
            return true;
        }
        index++;
    }

    return false;
}

static bool parse_unsigned_int64_strict(const char *text, int64_t *value, bool *overflow)
{
    char *end = NULL;
    intmax_t parsed = 0;
    size_t index = 0U;

    *overflow = false;

    if (text[0] == '\0' || text[0] == '+' || text[0] == '-') {
        return false;
    }

    while (text[index] != '\0') {
        if (!isdigit((unsigned char)text[index])) {
            return false;
        }
        index++;
    }

    errno = 0;
    parsed = strtoimax(text, &end, 10);
    if (errno == ERANGE || parsed > INT64_MAX) {
        *overflow = true;
        return false;
    }

    if (end == NULL || *end != '\0') {
        return false;
    }

    *value = (int64_t)parsed;
    return true;
}

static bool parse_signed_int64_strict(const char *text, int64_t *value, bool *overflow)
{
    char *end = NULL;
    intmax_t parsed = 0;
    size_t index = 0U;

    *overflow = false;

    if (text[0] == '\0' || text[0] == '+') {
        return false;
    }

    if (text[0] == '-') {
        if (text[1] == '\0') {
            return false;
        }
        index = 1U;
    }

    while (text[index] != '\0') {
        if (!isdigit((unsigned char)text[index])) {
            return false;
        }
        index++;
    }

    errno = 0;
    parsed = strtoimax(text, &end, 10);
    if (errno == ERANGE || parsed < INT64_MIN || parsed > INT64_MAX) {
        *overflow = true;
        return false;
    }

    if (end == NULL || *end != '\0') {
        return false;
    }

    *value = (int64_t)parsed;
    return true;
}

static bool add_column(Parser *parser, size_t line, size_t field)
{
    const char *name = parser->field.data;

    if (!is_valid_column_name(name)) {
        return set_error(parser->error, CSV_ERROR_INVALID_HEADER, line, field, "invalid column name at line %zu field %zu", line, field);
    }

    if (has_column(parser->table, name)) {
        return set_error(parser->error, CSV_ERROR_DUPLICATE_COLUMN, line, field, "duplicate column '%s'", name);
    }

    if (!table_add_column(parser->table, name, field)) {
        return set_error(parser->error, CSV_ERROR_OUT_OF_MEMORY, line, field, "out of memory while adding column");
    }

    return true;
}

static bool parse_header(Parser *parser, CsvDelimiter *header_delimiter)
{
    CsvDelimiter delimiter = CSV_DELIM_EOF;
    size_t field = 1U;
    const size_t line = 1U;

    if (!read_field(parser, line, field, &delimiter)) {
        return false;
    }

    if (delimiter == CSV_DELIM_EOF && parser->field.length == 0U) {
        return set_error(parser->error, CSV_ERROR_EMPTY_FILE, line, field, "empty file");
    }

    if (parser->field.length != 0U) {
        return set_error(parser->error, CSV_ERROR_INVALID_HEADER, line, field, "first header cell must be empty");
    }

    if (delimiter != CSV_DELIM_COMMA) {
        return set_error(parser->error, CSV_ERROR_INVALID_HEADER, line, field, "header must contain at least one column");
    }

    field = 2U;
    while (delimiter == CSV_DELIM_COMMA) {
        if (!read_field(parser, line, field, &delimiter)) {
            return false;
        }

        if (!add_column(parser, line, field)) {
            return false;
        }

        field++;
    }

    *header_delimiter = delimiter;
    return true;
}

static bool parse_row_number(Parser *parser, size_t line, size_t field, int64_t *row_number)
{
    bool overflow = false;

    if (!parse_unsigned_int64_strict(parser->field.data, row_number, &overflow)) {
        if (overflow) {
            return set_error(parser->error, CSV_ERROR_INTEGER_OVERFLOW, line, field, "row number overflow at line %zu field %zu", line, field);
        }
        return set_error(parser->error, CSV_ERROR_INVALID_ROW_NUMBER, line, field, "invalid row number at line %zu field %zu", line, field);
    }

    if (*row_number == 0) {
        return set_error(parser->error, CSV_ERROR_INVALID_ROW_NUMBER, line, field, "row number must be positive at line %zu", line);
    }

    if (has_row(parser->table, *row_number)) {
        return set_error(parser->error, CSV_ERROR_DUPLICATE_ROW, line, field, "duplicate row number %lld", (long long)*row_number);
    }

    return true;
}

static bool add_cell(Parser *parser, size_t line, size_t field)
{
    int64_t value = 0;
    bool overflow = false;

    if (parser->field.data[0] == '\0') {
        return set_error(parser->error, CSV_ERROR_INVALID_CELL, line, field, "empty cell at line %zu field %zu", line, field);
    }

    if (parser->field.data[0] == '=') {
        ParsedFormula formula;

        if (!formula_parse_text(parser->field.data, &formula, parser->error, line, field)) {
            return false;
        }

        if (!table_add_formula_cell(parser->table, &formula, line, field)) {
            formula_free(&formula);
            return set_error(parser->error, CSV_ERROR_OUT_OF_MEMORY, line, field, "out of memory while adding formula");
        }
        return true;
    }

    if (!parse_signed_int64_strict(parser->field.data, &value, &overflow)) {
        if (overflow) {
            return set_error(parser->error, CSV_ERROR_INTEGER_OVERFLOW, line, field, "integer overflow at line %zu field %zu", line, field);
        }
        return set_error(parser->error, CSV_ERROR_INVALID_CELL, line, field, "invalid integer cell at line %zu field %zu", line, field);
    }

    if (!table_add_number_cell(parser->table, value, line, field)) {
        return set_error(parser->error, CSV_ERROR_OUT_OF_MEMORY, line, field, "out of memory while adding cell");
    }

    return true;
}

static bool parse_cells(Parser *parser, size_t line, CsvDelimiter *row_delimiter)
{
    size_t column = 0U;
    CsvDelimiter delimiter = CSV_DELIM_COMMA;

    while (column < parser->table->column_count) {
        size_t field = column + 2U;

        if (!read_field(parser, line, field, &delimiter)) {
            return false;
        }

        if (!add_cell(parser, line, field)) {
            return false;
        }

        if (column + 1U < parser->table->column_count && delimiter != CSV_DELIM_COMMA) {
            return set_error(parser->error, CSV_ERROR_MALFORMED, line, field, "too few fields at line %zu", line);
        }

        if (column + 1U == parser->table->column_count && delimiter == CSV_DELIM_COMMA) {
            return set_error(parser->error, CSV_ERROR_MALFORMED, line, field + 1U, "too many fields at line %zu", line);
        }

        column++;
    }

    *row_delimiter = delimiter;
    return true;
}

static bool parse_data_rows(Parser *parser, size_t start_line)
{
    size_t line = start_line;
    bool has_data_row = false;
    bool keep_reading = true;

    while (keep_reading) {
        CsvDelimiter delimiter = CSV_DELIM_EOF;
        int64_t row_number = 0;

        if (!read_field(parser, line, 1U, &delimiter)) {
            return false;
        }

        if (delimiter == CSV_DELIM_EOF && parser->field.length == 0U) {
            if (!has_data_row) {
                return set_error(parser->error, CSV_ERROR_INVALID_HEADER, line, 1U, "table must contain at least one data row");
            }
            return true;
        }

        if (delimiter == CSV_DELIM_LINE && parser->field.length == 0U) {
            return set_error(parser->error, CSV_ERROR_MALFORMED, line, 1U, "empty line at line %zu", line);
        }

        if (!parse_row_number(parser, line, 1U, &row_number)) {
            return false;
        }

        if (delimiter != CSV_DELIM_COMMA) {
            return set_error(parser->error, CSV_ERROR_MALFORMED, line, 2U, "too few fields at line %zu", line);
        }

        if (!table_add_row(parser->table, row_number, line, NULL)) {
            return set_error(parser->error, CSV_ERROR_OUT_OF_MEMORY, line, 1U, "out of memory while adding row");
        }

        if (!parse_cells(parser, line, &delimiter)) {
            return false;
        }

        has_data_row = true;

        if (delimiter == CSV_DELIM_EOF) {
            keep_reading = false;
        } else {
            line++;
        }
    }

    return true;
}

bool csv_parse_stream(FILE *stream, Table *table, CsvError *error)
{
    Parser parser;
    CsvDelimiter header_delimiter = CSV_DELIM_EOF;
    bool ok = false;

    clear_error(error);
    table_init(table);

    parser.stream = stream;
    parser.table = table;
    parser.error = error;

    if (!field_buffer_init(&parser.field)) {
        return set_error(error, CSV_ERROR_OUT_OF_MEMORY, 0U, 0U, "out of memory while initializing parser");
    }

    ok = parse_header(&parser, &header_delimiter);
    if (ok) {
        if (header_delimiter == CSV_DELIM_EOF) {
            ok = set_error(error, CSV_ERROR_INVALID_HEADER, 1U, parser.table->column_count + 1U, "table must contain at least one data row");
        } else {
            ok = parse_data_rows(&parser, 2U);
        }
    }

    if (ok && !table_build_lookups(table)) {
        ok = set_error(error, CSV_ERROR_OUT_OF_MEMORY, 0U, 0U, "out of memory while building lookup tables");
    }

    field_buffer_free(&parser.field);

    if (!ok) {
        table_free(table);
    }

    return ok;
}

bool csv_parse_file(const char *path, Table *table, CsvError *error)
{
    FILE *stream = fopen(path, "rb");
    bool ok = false;

    if (stream == NULL) {
        table_init(table);
        return set_error(error, CSV_ERROR_IO, 0U, 0U, "cannot open file '%s'", path);
    }

    ok = csv_parse_stream(stream, table, error);

    if (fclose(stream) != 0 && ok) {
        table_free(table);
        ok = set_error(error, CSV_ERROR_IO, 0U, 0U, "cannot close file '%s'", path);
    }

    return ok;
}
