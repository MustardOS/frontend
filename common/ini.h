#pragma once

#include <stdint.h>
#include "mini/mini.h"

uint32_t get_ini_hex(mini_t *ini_config, const char *section, const char *key, uint32_t default_value);

uint16_t get_ini_uint(mini_t *ini_config, const char *section, const char *key, uint16_t default_value);

int16_t get_ini_int(mini_t *ini_config, const char *section, const char *key, int default_value);

float get_ini_float(mini_t *ini_config, const char *section, const char *key, float default_value);

char *get_ini_string(mini_t *ini_config, const char *section, const char *key, char *default_value);
