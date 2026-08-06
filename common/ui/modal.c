#include <stddef.h>
#include "modal.h"
#include "../video.h"

// Any bigger than 8 and it's a bit too much for 640x480 displays...
#define MODAL_STACK_MAX 8

static uint64_t stack[MODAL_STACK_MAX];
static int depth = 0;

static int gate(const mux_input_type type) {
    if (depth <= 0) return 1;

    return (stack[depth - 1] & MODAL_INPUT(type)) != 0;
}

static void apply(void) {
    mux_input_set_gate(depth > 0 ? gate : NULL);
}

void modal_claim(const uint64_t allowed) {
    video_preview_cancel();

    if (depth >= MODAL_STACK_MAX) return;

    stack[depth++] = allowed;
    apply();
}

void modal_release(void) {
    if (depth <= 0) return;

    depth--;
    apply();
}

int modal_active(void) {
    return depth > 0;
}

void modal_reset(void) {
    depth = 0;
    apply();
}
