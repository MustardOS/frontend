#include "../../common/config.h"
#include "../../common/input.h"
#include "../../module/muxshare.h"
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
           | (back ? BIT(5) : 0) | nav_mask_page();
}

uint64_t nav_mask_page(void) {
    const int page_up = mux_input_pressed(mux_input_l1);
    const int page_down = mux_input_pressed(mux_input_r1);

    return (page_up ? NAV_PAGE_UP_BIT : 0) | (page_down ? NAV_PAGE_DOWN_BIT : 0);
}

int nav_input_halted(void) {
    return mux_input_pressed(mux_input_l2);
}

static int nav_page_movement(const int direction, const int long_dot) {
    int steps = direction < 0 ? current_item_index : ui_count_static - current_item_index - 1;
    if (steps > theme.mux.item.count) steps = theme.mux.item.count;
    if (steps <= 0) return 0;

    nav_set_last_dir(direction < 0 ? nav_dir_up : nav_dir_down);
    nav_unsuppress_shake();
    gen_step_movement(steps, direction, long_dot, 0, 1);

    return 1;
}

int nav_page_tick(const uint64_t edge, const uint64_t mask, const int long_dot) {
    static nav_repeat_t rpt_page_up = {0};
    static nav_repeat_t rpt_page_down = {0};

    const uint32_t now = SDL_GetTicks();

    const int do_up = nav_repeat_step(
        &rpt_page_up, (edge & NAV_PAGE_UP_BIT) != 0, (mask & NAV_PAGE_UP_BIT) != 0, current_item_index > 0, now
    );
    const int do_down = nav_repeat_step(
        &rpt_page_down, (edge & NAV_PAGE_DOWN_BIT) != 0, (mask & NAV_PAGE_DOWN_BIT) != 0,
        current_item_index < ui_count_static - 1, now
    );

    if (ui_count_static < 2) return 0;

    if (do_up) return nav_page_movement(-1, long_dot);
    if (do_down) return nav_page_movement(+1, long_dot);

    return 0;
}
