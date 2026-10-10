#pragma once

#include <stdint.h>

#define MUGL_FRAME_PATH  "/run/muos/mugl-frame"
#define MUGL_FRAME_MAGIC 0x4D46474DU
#define MUGL_FRAME_DATA  64U

typedef struct {
    uint32_t magic;
    uint32_t server_pid;
    uint32_t width;
    uint32_t height;
    volatile uint32_t seq;
    volatile uint32_t request;
    volatile uint32_t served;
    uint32_t reserved[9];
} mugl_frame_header;
