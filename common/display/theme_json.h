#pragma once

#include <json/json.h>

char *theme_json_read(const char *base);

char *theme_json_read_file(const char *path);

struct json theme_json_module(struct json root, const char *module, const char *part);

struct json theme_json_lookup(struct json scheme, const char *section, const char *key);

long theme_json_number(struct json value, int base);

char *theme_json_module_layout(const char *base, const char *module);
