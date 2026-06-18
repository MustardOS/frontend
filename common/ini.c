#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "ini.h"
#include "mini/mini.h"
#include "options.h"

uint32_t get_ini_hex(mini_t *ini_config, const char *section, const char *key, uint32_t default_value) {
    const char *meta = mini_get_string(ini_config, section, key, "NOT FOUND");

    if (strcmp(meta, "NOT FOUND") == 0) return default_value;

    char *end = NULL;
    errno = 0;
    unsigned long hex_val = strtoul(meta, &end, 16);
    if (errno || end == meta || *end != '\0' || hex_val > UINT32_MAX) return default_value;
    return (uint32_t) hex_val;
}

uint16_t get_ini_uint(mini_t *ini_config, const char *section, const char *key, uint16_t default_value) {
    const char *meta = mini_get_string(ini_config, section, key, "NOT FOUND");

    if (strcmp(meta, "NOT FOUND") == 0) return default_value;

    char *end = NULL;
    unsigned long result = strtoul(meta, &end, 10);

    if (end == meta || *end != '\0' || result > UINT16_MAX) return default_value;
    return (uint16_t) result;
}

int16_t get_ini_int(mini_t *ini_config, const char *section, const char *key, int default_value) {
    const char *meta = mini_get_string(ini_config, section, key, "NOT FOUND");
    long result;

    if (strcmp(meta, "NOT FOUND") == 0) {
        result = default_value;
    } else {
        char *end = NULL;
        result = strtol(meta, &end, 10);

        if (end == meta || *end != '\0') result = default_value;
    }

    if (result > INT16_MAX) return INT16_MAX;
    if (result < INT16_MIN) return INT16_MIN;

    return (int16_t) result;
}

float get_ini_float(mini_t *ini_config, const char *section, const char *key, float default_value) {
    const char *meta = mini_get_string(ini_config, section, key, "NOT FOUND");

    float result;
    if (strcmp(meta, "NOT FOUND") == 0) {
        result = default_value;
    } else {
        char *end = NULL;
        result = strtof(meta, &end);
        if (end == meta || *end != '\0') result = default_value;
    }

    return result;
}

char *get_ini_string(mini_t *ini_config, const char *section, const char *key, char *default_value) {
    static char meta[MAX_BUFFER_SIZE];
    const char *result = mini_get_string(ini_config, section, key, default_value);

    snprintf(meta, MAX_BUFFER_SIZE, "%s", result);

    return meta;
}
