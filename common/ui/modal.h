#pragma once

#include <stdint.h>
#include "../input.h"

#define MODAL_INPUT(type) (1ULL << (type))

#define MODAL_MASK_MESSAGE                                                                                             \
    (MODAL_INPUT(mux_input_a) | MODAL_INPUT(mux_input_b) | MODAL_INPUT(mux_input_x) | MODAL_INPUT(mux_input_dpad_up)   \
     | MODAL_INPUT(mux_input_dpad_down))

#define MODAL_MASK_ACKNOWLEDGE (MODAL_INPUT(mux_input_a) | MODAL_INPUT(mux_input_b))

#define MODAL_MASK_DIALOGUE (MODAL_MASK_MESSAGE | MODAL_INPUT(mux_input_dpad_left) | MODAL_INPUT(mux_input_dpad_right))

void modal_claim(uint64_t allowed);

void modal_release(void);

int modal_active(void);

void modal_reset(void);
