#pragma once

void persistent_memory_init(const char *core_path, const char *content_path);

void persistent_memory_save(void);

int persistent_memory_flush(void);

void persistent_memory_shutdown(void);
