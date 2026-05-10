#include "int_parse.h"

static bool is_digit_char(char ch)
{
    return ch >= '0' && ch <= '9';
}

static IntParseResult parse_uint64_digits(
    const char *text,
    uint64_t limit,
    bool require_end,
    uint64_t *value,
    size_t *consumed
)
{
    size_t index = 0U;
    uint64_t parsed = 0U;
    bool has_digit = false;
    bool overflow = false;

    while (is_digit_char(text[index])) {
        uint64_t digit = (uint64_t)(text[index] - '0');

        has_digit = true;
        if (!overflow) {
            if (parsed > ((limit - digit) / 10U)) {
                overflow = true;
            } else {
                parsed = parsed * 10U + digit;
            }
        }
        index++;
    }

    if (!has_digit) {
        return INT_PARSE_INVALID;
    }

    if (require_end && text[index] != '\0') {
        return INT_PARSE_INVALID;
    }

    if (overflow) {
        return INT_PARSE_OVERFLOW;
    }

    if (value != NULL) {
        *value = parsed;
    }

    if (consumed != NULL) {
        *consumed = index;
    }

    return INT_PARSE_OK;
}

IntParseResult int_parse_signed_int64(const char *text, bool require_end, int64_t *value, size_t *consumed)
{
    size_t offset = 0U;
    size_t digit_count = 0U;
    uint64_t magnitude = 0U;
    uint64_t limit = (uint64_t)INT64_MAX;
    bool negative = false;
    IntParseResult result = INT_PARSE_INVALID;

    if (text == NULL || text[0] == '\0' || text[0] == '+') {
        return INT_PARSE_INVALID;
    }

    if (text[0] == '-') {
        negative = true;
        limit = ((uint64_t)INT64_MAX) + 1U;
        offset = 1U;
    }

    result = parse_uint64_digits(text + offset, limit, require_end, &magnitude, &digit_count);
    if (result != INT_PARSE_OK) {
        return result;
    }

    if (value != NULL) {
        if (negative && magnitude == (((uint64_t)INT64_MAX) + 1U)) {
            *value = INT64_MIN;
        } else if (negative) {
            *value = -(int64_t)magnitude;
        } else {
            *value = (int64_t)magnitude;
        }
    }

    if (consumed != NULL) {
        *consumed = offset + digit_count;
    }

    return INT_PARSE_OK;
}

IntParseResult int_parse_positive_row_number(const char *text, bool require_end, int64_t *row_number, size_t *consumed)
{
    size_t digit_count = 0U;
    uint64_t parsed = 0U;
    IntParseResult result = INT_PARSE_INVALID;

    if (text == NULL || text[0] == '\0' || text[0] == '+' || text[0] == '-') {
        return INT_PARSE_INVALID;
    }

    result = parse_uint64_digits(text, (uint64_t)INT64_MAX, require_end, &parsed, &digit_count);
    if (result != INT_PARSE_OK) {
        return result;
    }

    if (parsed == 0U) {
        return INT_PARSE_INVALID;
    }

    if (row_number != NULL) {
        *row_number = (int64_t)parsed;
    }

    if (consumed != NULL) {
        *consumed = digit_count;
    }

    return INT_PARSE_OK;
}
