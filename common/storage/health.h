#pragma once

#include <stdint.h>

typedef enum {
    storage_health_ok = 0,
    storage_health_missing,
    storage_health_read_only,
    storage_health_full,
    storage_health_error
} storage_health_status;

typedef struct {
    storage_health_status status;
    uint64_t free_bytes;
    int error_number;
} storage_health_snapshot;

storage_health_status storage_preflight_write(
    const char *path, uint64_t required_bytes, uint64_t reserve_bytes, storage_health_snapshot *snapshot
);
