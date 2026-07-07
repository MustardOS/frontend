#include "../common/input.h"
#include "muxretro.h"

static int suppress_until_released[mux_input_count];

void mux_retro_input_poll_cb(void) {
    mux_input_poll();
}

void input_bridge_suppress_held(void) {
    for (int i = 0; i < mux_input_count; i++) {
        suppress_until_released[i] = mux_input_pressed((mux_input_type) i) ? 1 : 0;
    }
}

int16_t mux_retro_input_state_cb(const unsigned port, const unsigned device, const unsigned index, const unsigned id) {
    if (port != 0 || device != RETRO_DEVICE_JOYPAD) return 0;
    (void) index;

    mux_input_type mux_type;
    switch (id) {
        case RETRO_DEVICE_ID_JOYPAD_B:
            mux_type = mux_input_b;
            break;
        case RETRO_DEVICE_ID_JOYPAD_Y:
            mux_type = mux_input_y;
            break;
        case RETRO_DEVICE_ID_JOYPAD_SELECT:
            mux_type = mux_input_select;
            break;
        case RETRO_DEVICE_ID_JOYPAD_START:
            mux_type = mux_input_start;
            break;
        case RETRO_DEVICE_ID_JOYPAD_UP:
            mux_type = mux_input_dpad_up;
            break;
        case RETRO_DEVICE_ID_JOYPAD_DOWN:
            mux_type = mux_input_dpad_down;
            break;
        case RETRO_DEVICE_ID_JOYPAD_LEFT:
            mux_type = mux_input_dpad_left;
            break;
        case RETRO_DEVICE_ID_JOYPAD_RIGHT:
            mux_type = mux_input_dpad_right;
            break;
        case RETRO_DEVICE_ID_JOYPAD_A:
            mux_type = mux_input_a;
            break;
        case RETRO_DEVICE_ID_JOYPAD_X:
            mux_type = mux_input_x;
            break;
        case RETRO_DEVICE_ID_JOYPAD_L:
            mux_type = mux_input_l1;
            break;
        case RETRO_DEVICE_ID_JOYPAD_R:
            mux_type = mux_input_r1;
            break;
        case RETRO_DEVICE_ID_JOYPAD_L2:
            mux_type = mux_input_l2;
            break;
        case RETRO_DEVICE_ID_JOYPAD_R2:
            mux_type = mux_input_r2;
            break;
        case RETRO_DEVICE_ID_JOYPAD_L3:
            mux_type = mux_input_l3;
            break;
        case RETRO_DEVICE_ID_JOYPAD_R3:
            mux_type = mux_input_r3;
            break;
        default:
            return 0;
    }

    const int held = mux_input_pressed(mux_type);

    if (suppress_until_released[mux_type]) {
        if (!held) suppress_until_released[mux_type] = 0;
        return 0;
    }

    return held ? 1 : 0;
}
