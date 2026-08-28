#pragma once

#include <linux/input.h>

static inline unsigned short h700_map_key(unsigned short source_code, int has_sticks) {
    switch (source_code) {
        case KEY_VOLUMEDOWN:
            return KEY_VOLUMEDOWN;
        case KEY_VOLUMEUP:
            return KEY_VOLUMEUP;
        case BTN_SOUTH:
            return BTN_SOUTH;
        case BTN_EAST:
            return BTN_EAST;
        case BTN_C:
            return BTN_WEST;
        case BTN_NORTH:
            return BTN_NORTH;
        case BTN_WEST:
            return BTN_TL;
        case BTN_Z:
            return BTN_TR;
        case BTN_TL:
            return BTN_SELECT;
        case BTN_TR:
            return BTN_START;
        case BTN_TL2:
            return BTN_MODE;
        case BTN_SELECT:
            return BTN_TL2;
        case BTN_START:
            return BTN_TR2;
        case BTN_THUMBL:
            return has_sticks ? BTN_THUMBL : KEY_RESERVED;
        case BTN_THUMBR:
            return has_sticks ? BTN_THUMBR : KEY_RESERVED;
        default:
            return KEY_RESERVED;
    }
}

static inline unsigned short h700_map_abs(unsigned short source_code, int has_sticks) {
    switch (source_code) {
        case ABS_RX:
            return ABS_X;
        case ABS_RY:
            return ABS_Y;
        case ABS_RZ:
            return has_sticks ? ABS_RX : ABS_CNT;
        case ABS_THROTTLE:
            return has_sticks ? ABS_RY : ABS_CNT;
        case ABS_HAT0X:
            return ABS_HAT0X;
        case ABS_HAT0Y:
            return ABS_HAT0Y;
        default:
            return ABS_CNT;
    }
}
