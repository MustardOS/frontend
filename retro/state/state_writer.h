#pragma once

#include <stddef.h>
#include <stdint.h>

int state_writer_wait(void);

int state_writer_busy(void);

int state_writer_submit(const char *path, uint8_t *data, size_t size, size_t core_size, size_t cheevo_size);

int state_writer_flush(void);

void state_writer_shutdown(void);
