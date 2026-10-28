#pragma once

#include <stdint.h>

int storage_view_bind_all(const char *root);
int storage_view_capacity(const char *root, uint64_t *total, uint64_t *available);
