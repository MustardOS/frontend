#pragma once

#include <stdint.h>

#define IMAGE_WRITER_PATH_MAX 512
#define IMAGE_WRITER_COPY_MAX 16

int image_writer_available(void);

uint8_t *image_writer_claim(int width, int height);

void image_writer_commit(const char *path, const char *const *copies, unsigned copy_count);

void image_writer_release(void);

void image_writer_flush(void);

void image_writer_shutdown(void);
