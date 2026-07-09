#include "../common/config.h"
#include "../common/input.h"
#include "nav_repeat.h"

int nav_repeat_step(nav_repeat_t *state, const int edge, const int held, const int repeat_allowed, const uint32_t now) {
    if (edge) {
        state->delay = (uint32_t) config.settings.advanced.repeat_delay;
        state->tick = now;
        return 1;
    }

    if (held && now - state->tick >= state->delay) {
        state->delay = (uint32_t) config.settings.advanced.accelerate;
        state->tick = now;
        return repeat_allowed;
    }

    return 0;
}

uint64_t nav_mask_standard(void) {
    const int up = mux_input_pressed(mux_input_dpad_up);
    const int down = mux_input_pressed(mux_input_dpad_down);
    const int left = mux_input_pressed(mux_input_dpad_left);
    const int right = mux_input_pressed(mux_input_dpad_right);
    const int confirm = mux_input_pressed(mux_input_a);
    const int back = mux_input_pressed(mux_input_b);

    return (up ? BIT(0) : 0) | (down ? BIT(1) : 0) | (left ? BIT(2) : 0) | (right ? BIT(3) : 0) | (confirm ? BIT(4) : 0)
           | (back ? BIT(5) : 0);
}
