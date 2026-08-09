#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include "config_value.h"

int16_t config_i16_value(
    const char *value, const int16_t fallback, const int has_range, const int16_t minimum, const int16_t maximum
) {
    if (!value || !*value) return fallback;

    errno = 0;
    char *end = NULL;
    const long parsed = strtol(value, &end, 10);
    if (errno == ERANGE || end == value || *end != '\0' || parsed < INT16_MIN || parsed > INT16_MAX) return fallback;
    if (has_range && (parsed < minimum || parsed > maximum)) return fallback;
    return (int16_t) parsed;
}

double config_float_value(
    const char *value, const double fallback, const int has_range, const double minimum, const double maximum
) {
    if (!value || !*value) return fallback;

    errno = 0;
    char *end = NULL;
    const double parsed = strtod(value, &end);
    if (errno == ERANGE || end == value || *end != '\0' || !isfinite(parsed)) return fallback;
    if (has_range && (parsed < minimum || parsed > maximum)) return fallback;
    return parsed;
}

int config_port_valid(const char *value) {
    if (!value || !*value) return 0;
    for (const unsigned char *part = (const unsigned char *) value; *part; part++) {
        if (!isdigit(*part)) return 0;
    }

    errno = 0;
    char *end = NULL;
    const unsigned long parsed = strtoul(value, &end, 10);
    return errno == 0 && end != value && *end == '\0' && parsed > 0 && parsed <= UINT16_MAX;
}
