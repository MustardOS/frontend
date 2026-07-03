#pragma once

int asset_cache_get(const char *key, char *path_out, size_t path_out_size);

void asset_cache_put(const char *key, const char *path, int found);

void asset_cache_clear(void);

int asset_cache_context_changed(void);
