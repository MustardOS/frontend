#pragma once

#include <stdint.h>

int16_t config_i16_value(const char *value, int16_t fallback, int has_range, int16_t minimum, int16_t maximum);

double config_float_value(const char *value, double fallback, int has_range, double minimum, double maximum);

int config_port_valid(const char *value);
