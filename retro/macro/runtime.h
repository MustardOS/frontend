#pragma once

#include <stdint.h>

void macro_runtime_begin_port(int port);

uint16_t macro_runtime_drive(
    int port, int source, int macro_index, int raw_held, uint64_t input_mask, double frames_per_second
);

int macro_runtime_stick(int port, int stick, int16_t *x, int16_t *y);

void macro_runtime_reset_port(int port);
