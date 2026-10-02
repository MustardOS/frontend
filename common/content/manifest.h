#pragma once

#include <stddef.h>

#include <json/json.h>

int manifest_json_string(struct json object, const char *key, char *output, size_t size);
int manifest_https_url(const char *url);
int manifest_label_valid(const char *text);
int manifest_safe_stem(const char *text, char *output, size_t size);
int manifest_url_stem(const char *url, char *output, size_t size);
int manifest_sha256_valid(const char *text);
