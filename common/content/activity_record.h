#pragma once

#include <stddef.h>
#include <stdint.h>

void activity_content_key(const char *content_path, char *key, size_t key_size);

uint32_t activity_key_hash(const char *content_path);
