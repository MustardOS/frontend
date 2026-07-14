#pragma once

#include <stddef.h>

#define RANDNAME_ADJECTIVE_COUNT 100U
#define RANDNAME_NOUN_COUNT      100U

#define RANDNAME_COUNT RANDNAME_ADJECTIVE_COUNT

#define RANDNAME_COMBINATION_COUNT (RANDNAME_ADJECTIVE_COUNT * RANDNAME_NOUN_COUNT)

#define RANDNAME_SEP     "_"
#define RANDNAME_MAX_LEN 32U

extern const char *const randname_adjectives[RANDNAME_ADJECTIVE_COUNT];
extern const char *const randname_nouns[RANDNAME_NOUN_COUNT];

int randname_generate(char *out, size_t out_size);

int randname_generate_with_separator(char *out, size_t out_size, const char *separator);

int randname_format(char *out, size_t out_size, size_t adjective_index, size_t noun_index, const char *separator);

int randname_from_id(char *out, size_t out_size, size_t id);
