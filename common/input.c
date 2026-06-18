#include <stdint.h>
#include <signal.h>
#include <SDL2/SDL.h>
#include "init.h"
#include "ui/common.h"
#include "config.h"
#include "device.h"
#include "board.h"
#include "input.h"
#include "ui/osk.h"
#include "log.h"
#include "display.h"
#include "anim.h"

#define INPUT_COOLDOWN          256
#define AXIS_THRESHOLD_FRACTION 0.80f
#define AXIS_MAX                32767
#define AXIS_THRESHOLD          ((int16_t)((float)AXIS_MAX * AXIS_THRESHOLD_FRACTION))
#define TRIGGER_THRESHOLD       ((int16_t)((float)AXIS_MAX * AXIS_THRESHOLD_FRACTION))

int swap_axis = 0;
int input_init_done = 0;

static int menu_short_pressed = 0;
static int menu_short_consumed = 0;

// Cross-thread quit flag (set during shutdown)
static volatile sig_atomic_t stop_flag = 0;

// System clock tick at the start of this iteration of the event loop.
static uint32_t tick = 0;

// Bitmask of input mux_types that are currently active.
static volatile uint64_t pressed = 0;

// Bitmask of input mux_types that were active during the previous iteration of the event loop.
static volatile uint64_t held = 0;

// Set while the alt ("hold") modifier is held.
int hold_call = 0;

// Whether the alt modifier was active at the start of the current poll cycle.
static int hold_active = 0;

// Bitmask of inputs whose press was dispatched through the alt layer, so their hold and
// release are routed through the same layer regardless of the modifier's current state.
static uint64_t alt_keys = 0;

// Suppress any input during screensaver runs
static volatile uint32_t suppress_until_tick = 0;

// Latest raw stick positions, cached from SDL_CONTROLLERAXISMOTION events.
// Range matches AXIS_MAX (int16_t [-32768, 32767])
static int16_t raw_ls_x = 0;
static int16_t raw_ls_y = 0;
static int16_t raw_rs_x = 0;
static int16_t raw_rs_y = 0;

static uint32_t controller_axis_log_mask = 0;
static uint32_t raw_axis_log_mask = 0;

#define MAX_INPUT_DEVICES 4

typedef struct {
    SDL_GameController *controller;
    SDL_Joystick *joystick;
    SDL_JoystickID instance;
} tracked_device;

static tracked_device devices[MAX_INPUT_DEVICES];
static int device_count = 0;

static SDL_JoystickID primary_instance = -1;
static int mappings_loaded = 0;

static int find_device_by_instance(SDL_JoystickID id) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].instance == id) return i;
    }

    return -1;
}

static int is_tracked_instance(SDL_JoystickID id) {
    return find_device_by_instance(id) >= 0;
}

static int is_tracked_as_controller(SDL_JoystickID id) {
    int idx = find_device_by_instance(id);
    return idx >= 0 && devices[idx].controller != NULL;
}

static inline int input_is_suppressed(void) {
    return mux_tick() < suppress_until_tick;
}

// This looks unused but it isn't... not sure how or why?
static key_event_callback event_handler = NULL;

static mux_input_type controller_button_map[SDL_CONTROLLER_BUTTON_MAX];

static mux_input_type joy_button_map[32];

typedef struct {
    mux_input_type neg;
    mux_input_type pos;
} axis_map_entry;

static axis_map_entry axis_map[SDL_CONTROLLER_AXIS_MAX];
static mux_input_type input_map[SDL_NUM_SCANCODES];

static void map_vol_buttons(mux_input_type *map, int down_idx, int up_idx) {
    map[down_idx] = MUX_INPUT_VOL_DOWN;
    map[up_idx] = MUX_INPUT_VOL_UP;
}

static void apply_face_button_layout(void) {
    if (config.SETTINGS.REMAP.LAYOUT == 1) {
        controller_button_map[SDL_CONTROLLER_BUTTON_A] = MUX_INPUT_B;
        controller_button_map[SDL_CONTROLLER_BUTTON_B] = MUX_INPUT_A;
        controller_button_map[SDL_CONTROLLER_BUTTON_X] = MUX_INPUT_Y;
        controller_button_map[SDL_CONTROLLER_BUTTON_Y] = MUX_INPUT_X;
    } else {
        controller_button_map[SDL_CONTROLLER_BUTTON_A] = MUX_INPUT_A;
        controller_button_map[SDL_CONTROLLER_BUTTON_B] = MUX_INPUT_B;
        controller_button_map[SDL_CONTROLLER_BUTTON_X] = MUX_INPUT_X;
        controller_button_map[SDL_CONTROLLER_BUTTON_Y] = MUX_INPUT_Y;
    }
}

static void init_input_maps(void) {
    if (input_init_done) return;

    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) controller_button_map[i] = MUX_INPUT_COUNT;

    for (int i = 0; i < 32; i++) joy_button_map[i] = MUX_INPUT_COUNT;

    for (int i = 0; i < SDL_NUM_SCANCODES; i++) input_map[i] = MUX_INPUT_COUNT;

    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
        axis_map[i].neg = MUX_INPUT_COUNT;
        axis_map[i].pos = MUX_INPUT_COUNT;
    }

    apply_face_button_layout();

    // TODO: Work out what C and Z would eventually map to,
    // TODO: as well as the TrimUI Switch fella!
    controller_button_map[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = MUX_INPUT_L1;
    controller_button_map[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = MUX_INPUT_R1;
    controller_button_map[SDL_CONTROLLER_BUTTON_LEFTSTICK] = MUX_INPUT_L3;
    controller_button_map[SDL_CONTROLLER_BUTTON_RIGHTSTICK] = MUX_INPUT_R3;
    controller_button_map[SDL_CONTROLLER_BUTTON_BACK] = MUX_INPUT_SELECT;
    controller_button_map[SDL_CONTROLLER_BUTTON_START] = MUX_INPUT_START;
    controller_button_map[SDL_CONTROLLER_BUTTON_GUIDE] = MUX_INPUT_MENU;
    controller_button_map[SDL_CONTROLLER_BUTTON_DPAD_UP] = MUX_INPUT_DPAD_UP;
    controller_button_map[SDL_CONTROLLER_BUTTON_DPAD_DOWN] = MUX_INPUT_DPAD_DOWN;
    controller_button_map[SDL_CONTROLLER_BUTTON_DPAD_LEFT] = MUX_INPUT_DPAD_LEFT;
    controller_button_map[SDL_CONTROLLER_BUTTON_DPAD_RIGHT] = MUX_INPUT_DPAD_RIGHT;

    axis_map[SDL_CONTROLLER_AXIS_LEFTX].neg = MUX_INPUT_LS_LEFT;
    axis_map[SDL_CONTROLLER_AXIS_LEFTX].pos = MUX_INPUT_LS_RIGHT;
    axis_map[SDL_CONTROLLER_AXIS_LEFTY].neg = MUX_INPUT_LS_UP;
    axis_map[SDL_CONTROLLER_AXIS_LEFTY].pos = MUX_INPUT_LS_DOWN;
    axis_map[SDL_CONTROLLER_AXIS_RIGHTX].neg = MUX_INPUT_RS_LEFT;
    axis_map[SDL_CONTROLLER_AXIS_RIGHTX].pos = MUX_INPUT_RS_RIGHT;
    axis_map[SDL_CONTROLLER_AXIS_RIGHTY].neg = MUX_INPUT_RS_UP;
    axis_map[SDL_CONTROLLER_AXIS_RIGHTY].pos = MUX_INPUT_RS_DOWN;
    axis_map[SDL_CONTROLLER_AXIS_TRIGGERLEFT].neg = MUX_INPUT_COUNT;
    axis_map[SDL_CONTROLLER_AXIS_TRIGGERLEFT].pos = MUX_INPUT_L2;
    axis_map[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].neg = MUX_INPUT_COUNT;
    axis_map[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].pos = MUX_INPUT_R2;

    input_map[SDL_SCANCODE_SPACE] = MUX_INPUT_A;
    input_map[SDL_SCANCODE_BACKSPACE] = MUX_INPUT_B;
    input_map[SDL_SCANCODE_R] = MUX_INPUT_X;
    input_map[SDL_SCANCODE_F] = MUX_INPUT_Y;

    input_map[SDL_SCANCODE_Q] = MUX_INPUT_L1;
    input_map[SDL_SCANCODE_E] = MUX_INPUT_R1;
    input_map[SDL_SCANCODE_Z] = MUX_INPUT_L2;
    input_map[SDL_SCANCODE_C] = MUX_INPUT_R2;

    input_map[SDL_SCANCODE_LCTRL] = MUX_INPUT_SELECT;
    input_map[SDL_SCANCODE_LSHIFT] = MUX_INPUT_START;

    input_map[SDL_SCANCODE_F1] = MUX_INPUT_MENU;

    input_map[SDL_SCANCODE_UP] = MUX_INPUT_DPAD_UP;
    input_map[SDL_SCANCODE_DOWN] = MUX_INPUT_DPAD_DOWN;
    input_map[SDL_SCANCODE_LEFT] = MUX_INPUT_DPAD_LEFT;
    input_map[SDL_SCANCODE_RIGHT] = MUX_INPUT_DPAD_RIGHT;

    input_map[SDL_SCANCODE_W] = MUX_INPUT_DPAD_UP;
    input_map[SDL_SCANCODE_S] = MUX_INPUT_DPAD_DOWN;
    input_map[SDL_SCANCODE_A] = MUX_INPUT_DPAD_LEFT;
    input_map[SDL_SCANCODE_D] = MUX_INPUT_DPAD_RIGHT;

    switch (board_special()) {
        case BOARD_SPECIAL_TUI_SPOON:
            map_vol_buttons(joy_button_map, 11, 12);
            break;
        case BOARD_SPECIAL_TUI_BRICK:
            map_vol_buttons(joy_button_map, 0, 1);
            break;
        case BOARD_SPECIAL_VITA_PRO:
            map_vol_buttons(joy_button_map, 13, 14);
            break;
        case BOARD_SPECIAL_G350:
            break;
        default:
            map_vol_buttons(joy_button_map, 1, 2);
            break;
    }

    input_map[SDL_SCANCODE_PAGEUP] = MUX_INPUT_VOL_UP;
    input_map[SDL_SCANCODE_VOLUMEUP] = MUX_INPUT_VOL_UP;

    input_map[SDL_SCANCODE_PAGEDOWN] = MUX_INPUT_VOL_DOWN;
    input_map[SDL_SCANCODE_VOLUMEDOWN] = MUX_INPUT_VOL_DOWN;

    // input_map[SDL_SCANCODE_AC_BACK] = ??????????

    input_init_done = 1;
}

static inline void apply_dir_pair(mux_input_type neg, mux_input_type pos, int direction) {
    uint64_t clear_mask = BIT(neg) | BIT(pos);
    pressed &= ~clear_mask;

    if (direction < 0) {
        pressed |= BIT(neg);
    } else if (direction > 0) {
        pressed |= BIT(pos);
    }
}

static void process_sdl_button(SDL_GameControllerButton btn, int down) {
    if (input_is_suppressed() || btn >= SDL_CONTROLLER_BUTTON_MAX) return;

    mux_input_type t = controller_button_map[btn];

    if (t == MUX_INPUT_COUNT) {
        LOG_DEBUG("input", "Unmapped controller button %d", btn);
        return;
    }

    // Wonky donkey input...
    if (swap_axis && !key_show) {
        switch (t) {
            case MUX_INPUT_DPAD_UP:
                t = MUX_INPUT_DPAD_LEFT;
                break;
            case MUX_INPUT_DPAD_DOWN:
                t = MUX_INPUT_DPAD_RIGHT;
                break;
            case MUX_INPUT_DPAD_LEFT:
                t = MUX_INPUT_DPAD_UP;
                break;
            case MUX_INPUT_DPAD_RIGHT:
                t = MUX_INPUT_DPAD_DOWN;
                break;
            default:
                break;
        }
    }

    if (t == MUX_INPUT_MENU) {
        if (down) {
            menu_short_pressed = 1;
            menu_short_consumed = 0;
        } else {
            menu_short_pressed = 0;
            if (menu_short_consumed) return;
        }
    }

    if (menu_short_pressed && (t == MUX_INPUT_VOL_UP || t == MUX_INPUT_VOL_DOWN) && down) menu_short_consumed = 1;

    pressed = down ? (pressed | BIT(t)) : (pressed & ~BIT(t));
}

static void process_sdl_joy_button(uint8_t button, int down) {
    if (button >= 32) return;

    mux_input_type t = joy_button_map[button];
    if (t == MUX_INPUT_COUNT) {
        LOG_DEBUG("input", "Unmapped joystick button %d", button);
        return;
    }

    pressed = down ? (pressed | BIT(t)) : (pressed & ~BIT(t));
}

static inline int axis_magnitude(int16_t value) {
    return value < 0 ? -(int) value : (int) value;
}

static void log_axis_once(const char *source, uint8_t axis, int16_t value, uint32_t *mask) {
    if (axis >= 32) return;
    if (*mask & (UINT32_C(1) << axis)) return;
    if (axis_magnitude(value) < 4096) return;

    *mask |= UINT32_C(1) << axis;

    LOG_INFO("input", "%s axis %u active: %d", source, axis, value);
}

static inline void reset_raw_analog(void) {
    raw_ls_x = 0;
    raw_ls_y = 0;
    raw_rs_x = 0;
    raw_rs_y = 0;

    controller_axis_log_mask = 0;
    raw_axis_log_mask = 0;
}

static inline void cache_controller_axis(SDL_GameControllerAxis axis, int16_t value) {
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX:
            raw_ls_x = value;
            break;
        case SDL_CONTROLLER_AXIS_LEFTY:
            raw_ls_y = value;
            break;
        case SDL_CONTROLLER_AXIS_RIGHTX:
            raw_rs_x = value;
            break;
        case SDL_CONTROLLER_AXIS_RIGHTY:
            raw_rs_y = value;
            break;
        default:
            break;
    }
}

static int raw_axis_to_controller_axis(uint8_t axis, SDL_GameControllerAxis *out_axis) {
    switch (axis) {
        case 0:
            *out_axis = SDL_CONTROLLER_AXIS_LEFTX;
            return 1;
        case 1:
            *out_axis = SDL_CONTROLLER_AXIS_LEFTY;
            return 1;
        case 2:
            *out_axis = SDL_CONTROLLER_AXIS_RIGHTX;
            return 1;
        case 3:
            *out_axis = SDL_CONTROLLER_AXIS_RIGHTY;
            return 1;
        default:
            break;
    }

    return 0;
}

static axis_map_entry raw_joystick_axis_map(uint8_t axis) {
    axis_map_entry m = {
            .neg = MUX_INPUT_COUNT,
            .pos = MUX_INPUT_COUNT,
    };

    switch (axis) {
        case 0:
            m.neg = MUX_INPUT_LS_LEFT;
            m.pos = MUX_INPUT_LS_RIGHT;
            break;
        case 1:
            m.neg = MUX_INPUT_LS_UP;
            m.pos = MUX_INPUT_LS_DOWN;
            break;
        case 2:
            m.neg = MUX_INPUT_RS_LEFT;
            m.pos = MUX_INPUT_RS_RIGHT;
            break;
        case 3:
            m.neg = MUX_INPUT_RS_UP;
            m.pos = MUX_INPUT_RS_DOWN;
            break;
        default:
            break;
    }

    return m;
}

static inline void cache_raw_joystick_axis(uint8_t axis, int16_t value) {
    switch (axis) {
        case 0:
            raw_ls_x = value;
            break;
        case 1:
            raw_ls_y = value;
            break;
        case 2:
            raw_rs_x = value;
            break;
        case 3:
            raw_rs_y = value;
            break;
        default:
            break;
    }
}

static void apply_axis_motion(axis_map_entry m, int16_t value) {
    if (m.pos == MUX_INPUT_COUNT && m.neg == MUX_INPUT_COUNT) return;

    if (m.neg == MUX_INPUT_COUNT) {
        if (value >= TRIGGER_THRESHOLD) {
            pressed |= BIT(m.pos);
        } else {
            pressed &= ~BIT(m.pos);
        }

        return;
    }

    mux_input_type neg = m.neg;
    mux_input_type pos = m.pos;

    if (swap_axis && !key_show) {
        if (neg == MUX_INPUT_DPAD_UP) {
            neg = MUX_INPUT_DPAD_LEFT;
            pos = MUX_INPUT_DPAD_RIGHT;
        } else if (neg == MUX_INPUT_DPAD_LEFT) {
            neg = MUX_INPUT_DPAD_UP;
            pos = MUX_INPUT_DPAD_DOWN;
        } else if (neg == MUX_INPUT_LS_UP) {
            neg = MUX_INPUT_LS_LEFT;
            pos = MUX_INPUT_LS_RIGHT;
        } else if (neg == MUX_INPUT_LS_LEFT) {
            neg = MUX_INPUT_LS_UP;
            pos = MUX_INPUT_LS_DOWN;
        } else if (neg == MUX_INPUT_RS_UP) {
            neg = MUX_INPUT_RS_LEFT;
            pos = MUX_INPUT_RS_RIGHT;
        } else if (neg == MUX_INPUT_RS_LEFT) {
            neg = MUX_INPUT_RS_UP;
            pos = MUX_INPUT_RS_DOWN;
        }
    }

    int direction = 0;

    if (value <= -AXIS_THRESHOLD) {
        direction = -1;
    } else if (value >= AXIS_THRESHOLD) {
        direction = 1;
    }

    apply_dir_pair(neg, pos, direction);
}

static void process_sdl_axis(SDL_GameControllerAxis axis, int16_t value) {
    if (axis >= SDL_CONTROLLER_AXIS_MAX || input_is_suppressed()) return;

    log_axis_once("Controller", (uint8_t) axis, value, &controller_axis_log_mask);

    cache_controller_axis(axis, value);
    apply_axis_motion(axis_map[axis], value);
}

static void process_sdl_joy_axis(SDL_JoystickID which, uint8_t axis, int16_t value) {
    if (input_is_suppressed() || is_tracked_as_controller(which)) return;

    log_axis_once("Raw joystick", axis, value, &raw_axis_log_mask);

    cache_raw_joystick_axis(axis, value);
    apply_axis_motion(raw_joystick_axis_map(axis), value);
}

static void process_sdl_key(const SDL_KeyboardEvent *key, int down) {
    if (input_is_suppressed()) return;

    SDL_Scancode sc = key->keysym.scancode;
    if (sc >= SDL_NUM_SCANCODES) return;

    mux_input_type t = input_map[sc];
    if (t == MUX_INPUT_COUNT) {
        LOG_DEBUG("input", "Unmapped key %s (%d)", SDL_GetScancodeName(sc), sc);
        return;
    }

    if (key->repeat) return;

    pressed = down ? (pressed | BIT(t)) : (pressed & ~BIT(t));
}

static void close_device_by_instance(SDL_JoystickID id) {
    int idx = find_device_by_instance(id);
    if (idx < 0) return;

    if (devices[idx].controller) {
        SDL_GameControllerClose(devices[idx].controller);
    } else if (devices[idx].joystick) {
        SDL_JoystickClose(devices[idx].joystick);
    }

    devices[idx] = devices[device_count - 1];
    device_count--;
}

static void close_all_devices(void) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].controller) {
            SDL_GameControllerClose(devices[i].controller);
        } else if (devices[i].joystick) {
            SDL_JoystickClose(devices[i].joystick);
        }
    }

    device_count = 0;
}

static void open_all_input_devices(void) {
    static uint32_t last_no_device_log = UINT32_MAX;
    int was_empty = (device_count == 0);

    if (!mappings_loaded) {
        if (device.BOARD.SDL_MAP[0] != '\0' && SDL_GameControllerAddMapping(device.BOARD.SDL_MAP) < 0) {
            LOG_WARN("input", "Failed to add device SDL_MAP: %s", SDL_GetError());
        }

        const char *info_map = (config.SETTINGS.REMAP.LAYOUT == 1) ? CONTROL_MODERN : CONTROL_RETRO;

        int mappings = SDL_GameControllerAddMappingsFromFile(info_map);
        if (mappings < 0) {
            LOG_WARN("input", "Failed to load gamecontrollerdb: %s", SDL_GetError());
        } else {
            LOG_INFO("input", "Loaded %d controller mappings", mappings);
        }

        mappings_loaded = 1;
    }

    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_PumpEvents();
    SDL_JoystickUpdate();

    const char *video_driver = SDL_GetCurrentVideoDriver();
    int num_joy = SDL_NumJoysticks();

    if (num_joy <= 0) {
        uint32_t now = (uint32_t) mux_tick();

        // Avoid log spam during retry loops...
        if (last_no_device_log == UINT32_MAX || now - last_no_device_log >= 2000U) {
            LOG_INFO("input", "SDL video driver: %s", video_driver ? video_driver : "unknown");
            LOG_INFO("input", "Detected %d joystick(s)", num_joy);
            LOG_WARN("input", "No controllers detected");
            last_no_device_log = now;
        }

        return;
    }

    last_no_device_log = UINT32_MAX;

    LOG_INFO("input", "SDL video driver: %s", video_driver ? video_driver : "unknown");
    LOG_INFO("input", "Detected %d joystick(s)", num_joy);

    for (int i = 0; i < num_joy && device_count < MAX_INPUT_DEVICES; i++) {
        if (!SDL_IsGameController(i)) continue;

        SDL_GameController *gc = SDL_GameControllerOpen(i);
        if (!gc) {
            LOG_WARN("input", "Failed to open controller %d: %s", i, SDL_GetError());
            continue;
        }

        SDL_Joystick *joy = SDL_GameControllerGetJoystick(gc);
        SDL_JoystickID inst = SDL_JoystickInstanceID(joy);

        if (is_tracked_instance(inst)) {
            SDL_GameControllerClose(gc);
            continue;
        }

        SDL_JoystickGUID guid = SDL_JoystickGetGUID(joy);
        char guid_str[64];
        SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));

        const char *name = SDL_GameControllerName(gc);
        LOG_INFO("input", "Controller opened: %s", name ? name : "unknown");
        LOG_INFO("input", "Controller GUID: %s", guid_str);
        LOG_INFO("input", "Controller raw axes: %d", SDL_JoystickNumAxes(joy));
        LOG_INFO("input", "Controller raw buttons: %d", SDL_JoystickNumButtons(joy));

        char *mapping = SDL_GameControllerMapping(gc);
        if (mapping) {
            LOG_DEBUG("input", "Active mapping: %s", mapping);
            SDL_free(mapping);
        }

        devices[device_count++] = (tracked_device) {.controller = gc, .joystick = joy, .instance = inst};
    }

    for (int i = 0; i < num_joy && device_count < MAX_INPUT_DEVICES; i++) {
        if (SDL_IsGameController(i)) continue;

        SDL_Joystick *joy = SDL_JoystickOpen(i);
        if (!joy) {
            LOG_WARN("input", "Failed to open joystick %d: %s", i, SDL_GetError());
            continue;
        }

        SDL_JoystickID inst = SDL_JoystickInstanceID(joy);

        if (is_tracked_instance(inst)) {
            SDL_JoystickClose(joy);
            continue;
        }

        SDL_JoystickGUID guid = SDL_JoystickGetGUID(joy);
        char guid_str[64];
        SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));

        const char *name = SDL_JoystickName(joy);
        LOG_WARN("input", "Using raw joystick fallback: %s", name ? name : "unknown");
        LOG_DEBUG("input", "Raw joystick GUID: %s", guid_str);
        LOG_INFO("input", "Raw joystick axes: %d", SDL_JoystickNumAxes(joy));
        LOG_INFO("input", "Raw joystick buttons: %d", SDL_JoystickNumButtons(joy));

        if (SDL_JoystickNumAxes(joy) < 2) LOG_WARN("input", "Raw joystick fallback has no usable stick axes");

        devices[device_count++] = (tracked_device) {.controller = NULL, .joystick = joy, .instance = inst};
    }

    if (device_count == 0) LOG_WARN("input", "No usable input device found");

    if (was_empty && device_count > 0 && primary_instance < 0) {
        primary_instance = devices[0].instance;
        LOG_INFO("input", "Primary input device set (instance %d)", primary_instance);
    }
}

void mux_input_reload_mappings(void) {
    close_all_devices();
    primary_instance = -1;
    mappings_loaded = 0;
    open_all_input_devices();
    apply_face_button_layout();
}

static inline mux_input_type remap_stick_to_dpad(mux_nav_type nav, mux_input_type mux_type) {
    if (nav & NAV_LEFT_STICK) {
        switch (mux_type) {
            case MUX_INPUT_LS_UP:
                return MUX_INPUT_DPAD_UP;
            case MUX_INPUT_LS_DOWN:
                return MUX_INPUT_DPAD_DOWN;
            case MUX_INPUT_LS_LEFT:
                return MUX_INPUT_DPAD_LEFT;
            case MUX_INPUT_LS_RIGHT:
                return MUX_INPUT_DPAD_RIGHT;
            case MUX_INPUT_L3:
                return MUX_INPUT_A;
            default:
                break;
        }
    }

    if (nav & NAV_RIGHT_STICK) {
        switch (mux_type) {
            case MUX_INPUT_RS_UP:
                return MUX_INPUT_DPAD_UP;
            case MUX_INPUT_RS_DOWN:
                return MUX_INPUT_DPAD_DOWN;
            case MUX_INPUT_RS_LEFT:
                return MUX_INPUT_DPAD_LEFT;
            case MUX_INPUT_RS_RIGHT:
                return MUX_INPUT_DPAD_RIGHT;
            case MUX_INPUT_R3:
                return MUX_INPUT_A;
            default:
                break;
        }
    }

    return mux_type;
}

// Invokes the relevant handler(s) for a particular input mux_type and action.
// The alt ("hold") modifier input for this screen, or MUX_INPUT_COUNT when disabled.
static mux_input_type hold_modifier(const mux_input_options *opts) {
    if (opts->hold_disabled) return MUX_INPUT_COUNT;
    return opts->hold_input ? opts->hold_input : MUX_INPUT_L2;
}

// Sample the modifier's raw pressed state at the start of the poll cycle, before any other
// input is dispatched, so a button pressed in the same instant as the modifier is reliably
// treated as held. Updates hold_call on the edge.
static void update_hold_modifier(const mux_input_options *opts) {
    mux_input_type mod = hold_modifier(opts);
    if (mod >= MUX_INPUT_COUNT) {
        hold_active = 0;
        return;
    }

    hold_active = mux_input_pressed(mod) ? 1 : 0;
    hold_call = hold_active;
}

static void dispatch_input(const mux_input_options *opts, mux_input_type mux_type, mux_input_action action) {
    // Remap input mux_types when using left stick as D-pad. (We still track pressed and held status for
    // the stick and D-pad inputs separately to avoid unintuitive hold behavior.)
    if (opts->remap_to_dpad) mux_type = remap_stick_to_dpad(opts->nav, mux_type);

    // Route through the alt layer for inputs whose press happened while the modifier was held,
    // so press/hold/release stay on the same layer even if the modifier is released mid-gesture.
    uint64_t bit = BIT(mux_type);
    int use_alt;
    if (action == MUX_INPUT_PRESS) {
        use_alt = hold_active;
        if (use_alt) alt_keys |= bit; else alt_keys &= ~bit;
    } else {
        use_alt = (alt_keys & bit) != 0;
        if (action == MUX_INPUT_RELEASE) alt_keys &= ~bit;
    }

    const mux_input_handler *press = use_alt ? opts->alt_press_handler : opts->press_handler;
    const mux_input_handler *hold = use_alt ? opts->alt_hold_handler : opts->hold_handler;
    const mux_input_handler *release = use_alt ? opts->alt_release_handler : opts->release_handler;

    mux_input_handler handler = NULL;
    switch (action) {
        case MUX_INPUT_PRESS:
            handler = press[mux_type];
            break;
        case MUX_INPUT_HOLD:
            handler = hold[mux_type];
            break;
        case MUX_INPUT_RELEASE:
            handler = release[mux_type];
            break;
    }

    // First invoke specific handler (if one was registered for this input mux_type and action).
    if (handler) handler();

    // Then invoke generic handler (if a catchall handler was registered).
    if (opts->input_handler) opts->input_handler(mux_type, action);
}

// Invokes the relevant handler(s) for a particular input combo number and action.
static void dispatch_combo(const mux_input_options *opts, int num, mux_input_action action) {
    mux_input_handler handler = NULL;
    switch (action) {
        case MUX_INPUT_PRESS:
            handler = opts->combo[num].press_handler;
            break;
        case MUX_INPUT_HOLD:
            handler = opts->combo[num].hold_handler;
            break;
        case MUX_INPUT_RELEASE:
            handler = opts->combo[num].release_handler;
            break;
    }

    // First invoke specific handler (if one was registered for this combo number and action).
    if (handler) handler();

    // Then invoke generic handler (if a catchall handler was registered).
    if (opts->combo_handler) opts->combo_handler(num, action);
}

static void handle_inputs(const mux_input_options *opts) {
    if (input_is_suppressed()) return;

    static uint32_t hold_delay[MUX_INPUT_COUNT] = {};
    static uint32_t hold_tick[MUX_INPUT_COUNT] = {};

    uint64_t blocked = 0;

    for (int i = 0; i < opts->combo_count; i++) {
        uint64_t mask = opts->combo[i].type_mask;

        if (!mask) continue;

        /* Ignore single-key combos */
        if (__builtin_popcountll(mask) <= 1) continue;

        if ((pressed & mask) == mask) {
            blocked |= mask;
        }
    }

    mux_input_type mod = hold_modifier(opts);
    if (mod < MUX_INPUT_COUNT) blocked |= BIT(mod);

    uint64_t pressed_filtered = pressed & ~blocked;
    uint64_t held_filtered = held & ~blocked;

    uint64_t changed = pressed_filtered ^ held_filtered;
    uint64_t active = pressed_filtered | held_filtered;

    while (active) {
        int i = __builtin_ctzll(active);
        active &= active - 1;

        uint64_t bit = BIT(i);

        if (pressed_filtered & bit) {
            if (changed & bit) {
                dispatch_input(opts, i, MUX_INPUT_PRESS);

                hold_delay[i] = config.SETTINGS.ADVANCED.REPEATDELAY;
                hold_tick[i] = tick;
            } else if (tick - hold_tick[i] >= hold_delay[i]) {
                dispatch_input(opts, i, MUX_INPUT_HOLD);

                hold_delay[i] = config.SETTINGS.ADVANCED.ACCELERATE;
                hold_tick[i] = tick;
            }
        } else {
            dispatch_input(opts, i, MUX_INPUT_RELEASE);
        }
    }
}

static void handle_combos(const mux_input_options *opts) {
    if (input_is_suppressed()) return;
    // Delay (millis) before invoking hold handler again.
    static uint32_t hold_delay = 0;
    // Tick (millis) of last press or hold.
    static uint32_t hold_tick = 0;

    // Combo number that was active during the previous iteration of the event loop, or
    // MUX_INPUT_COMBO_COUNT when no combo is held.
    static int active_combo = MUX_INPUT_COMBO_COUNT;

    uint64_t active_pressed = pressed;
    uint64_t active_held = held;

    if (!active_pressed) {
        active_combo = MUX_INPUT_COMBO_COUNT;
        return;
    }

    if (active_combo != MUX_INPUT_COMBO_COUNT) {
        // Active combo; check if it's still held or was released.
        uint64_t mask = opts->combo[active_combo].type_mask;

        if ((active_pressed & mask) == mask) {
            if (tick - hold_tick >= hold_delay) {
                // Single hold
                dispatch_combo(opts, active_combo, MUX_INPUT_HOLD);

                // Single delay for each subsequent repeat.
                hold_delay = config.SETTINGS.ADVANCED.ACCELERATE;
                hold_tick = tick;
            }
            return;
        }

        // Held & not pressed: Invoke release handler.
        dispatch_combo(opts, active_combo, MUX_INPUT_RELEASE);
        active_combo = MUX_INPUT_COMBO_COUNT;
    }

    int best_combo = MUX_INPUT_COMBO_COUNT;
    int best_bits = -1;

    // Sometimes, a single evdev event can result in us registering both a release and a press
    // (e.g., when transitioning from POWER_SHORT to POWER_LONG), so we have to check this even
    // if a combo was previously active at the start of the function.
    for (int i = 0; i < opts->combo_count; i++) {
        uint64_t mask = opts->combo[i].type_mask;
        if (!mask) continue;
        if ((active_pressed & mask) != mask) continue;

        int bits = __builtin_popcountll(mask);
        if (bits > best_bits) {
            best_bits = bits;
            best_combo = i;
        }
    }

    if (best_combo != MUX_INPUT_COMBO_COUNT) {
        if (active_pressed & ~active_held) {
            // Pressed & not held: Invoke "press" handler.
            dispatch_combo(opts, best_combo, MUX_INPUT_PRESS);

            // Initial repeat delay
            hold_delay = config.SETTINGS.ADVANCED.REPEATDELAY;
            hold_tick = tick;
            active_combo = best_combo;
        }
    }
}

static const mux_nav_type nav_map[] = {
        NAV_DPAD,
        NAV_LEFT_STICK,
        NAV_RIGHT_STICK,
        NAV_DPAD | NAV_LEFT_STICK,
        NAV_DPAD | NAV_RIGHT_STICK,
        NAV_DPAD | NAV_LEFT_STICK | NAV_RIGHT_STICK,
        NAV_LEFT_STICK | NAV_RIGHT_STICK,
};

mux_nav_type get_sticknav_mask(int sticknav_setting) {
    if (sticknav_setting < 0 ||
        sticknav_setting >= (int) (sizeof nav_map / sizeof nav_map[0])) {
        return NAV_NONE;
    }

    return nav_map[sticknav_setting];
}

void ep_wait_wake(void) {
    SDL_Event ev = {0};
    ev.type = SDL_USEREVENT;
    SDL_PushEvent(&ev);
}

void mux_input_flush_all(void) {
    pressed = 0;
    held = 0;
    suppress_until_tick = UINT32_MAX;
    ep_wait_wake();
}

void mux_input_resume(void) {
    pressed = 0;
    held = 0;
    suppress_until_tick = (uint32_t) (mux_tick() + INPUT_COOLDOWN);
}

uint32_t mux_input_tick(void) {
    return tick;
}

void register_key_event_callback(key_event_callback cb) {
    event_handler = cb;
}

void append_combo(mux_input_options *opts, mux_input_combo combo) {
    if (opts->combo_count >= MUX_INPUT_COMBO_COUNT) return;
    opts->combo[opts->combo_count++] = combo;
}

void mux_input_task(const mux_input_options *opts) {
    init_input_maps();

    stop_flag = 0;
    pressed = 0;
    held = 0;
    hold_active = 0;
    alt_keys = 0;
    suppress_until_tick = 0;
    primary_instance = -1;

    reset_raw_analog();

    swap_axis = opts->swap_axis;
    ((mux_input_options *) opts)->nav = get_sticknav_mask(config.SETTINGS.ADVANCED.STICKNAV);

    open_all_input_devices();

    const int timeout_idle = (opts->max_idle_ms > 0) ? (int) opts->max_idle_ms : (int) IDLE_MS;
    const int accel_ms = config.SETTINGS.ADVANCED.ACCELERATE > 0 ? config.SETTINGS.ADVANCED.ACCELERATE : 1;
    const int timeout_hold = (opts->max_idle_ms > 0) ? ((int) opts->max_idle_ms < accel_ms ? (int) opts->max_idle_ms : accel_ms) : accel_ms;

    const uint32_t retry_interval_fast_ms = 750U;
    const uint32_t retry_interval_slow_ms = 5000U;
    const int retry_fast_count = 5;
    const int no_device_wait_ms = 250;
    uint32_t next_retry_tick = 0;
    int retry_count = 0;

    fade_in_screen();

    SDL_Event ev;
    while (!stop_flag) {
        int timeout = held ? timeout_hold : timeout_idle;

        if (device_count == 0) {
            int poll_cap = (retry_count <= retry_fast_count) ? no_device_wait_ms : (int) retry_interval_slow_ms;
            if (timeout > poll_cap) timeout = poll_cap;
        }

        if (anim_is_active() && timeout > (int) IDLE_MS) timeout = (int) IDLE_MS;
        if (!SDL_WaitEventTimeout(&ev, timeout)) ev.type = SDL_USEREVENT;

        do {
            switch (ev.type) {
                case SDL_CONTROLLERBUTTONDOWN:
                    if (is_tracked_as_controller(ev.cbutton.which)) process_sdl_button(ev.cbutton.button, 1);
                    break;
                case SDL_CONTROLLERBUTTONUP:
                    if (is_tracked_as_controller(ev.cbutton.which)) process_sdl_button(ev.cbutton.button, 0);
                    break;
                case SDL_CONTROLLERAXISMOTION:
                    if (is_tracked_as_controller(ev.caxis.which)) process_sdl_axis(ev.caxis.axis, ev.caxis.value);
                    break;
                case SDL_JOYBUTTONDOWN:
                    if (ev.jbutton.which == primary_instance) process_sdl_joy_button(ev.jbutton.button, 1);
                    break;
                case SDL_JOYBUTTONUP:
                    if (ev.jbutton.which == primary_instance) process_sdl_joy_button(ev.jbutton.button, 0);
                    break;
                case SDL_JOYAXISMOTION:
                    if (is_tracked_instance(ev.jaxis.which) && !is_tracked_as_controller(ev.jaxis.which)) {
                        process_sdl_joy_axis(ev.jaxis.which, ev.jaxis.axis, ev.jaxis.value);
                    }
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                case SDL_JOYDEVICEADDED:
                    if (device_count < MAX_INPUT_DEVICES) {
                        LOG_INFO("input", "Input device connected");
                        open_all_input_devices();
                        retry_count = 0;
                        next_retry_tick = tick + retry_interval_fast_ms;
                    }
                    break;
                case SDL_CONTROLLERDEVICEREMOVED:
                    if (is_tracked_as_controller(ev.cdevice.which)) {
                        LOG_INFO("input", "Controller removed");
                        close_device_by_instance(ev.cdevice.which);
                        if (device_count == 0) {
                            pressed = 0;
                            held = 0;
                            reset_raw_analog();
                            next_retry_tick = 0;
                        }
                        open_all_input_devices();
                    }
                    break;
                case SDL_JOYDEVICEREMOVED:
                    if (is_tracked_instance(ev.jdevice.which) && !is_tracked_as_controller(ev.jdevice.which)) {
                        LOG_INFO("input", "Joystick removed");
                        close_device_by_instance(ev.jdevice.which);
                        if (device_count == 0) {
                            pressed = 0;
                            held = 0;
                            reset_raw_analog();
                            next_retry_tick = 0;
                        }
                        open_all_input_devices();
                    }
                    break;
                case SDL_KEYDOWN:
                    process_sdl_key(&ev.key, 1);
                    break;
                case SDL_KEYUP:
                    process_sdl_key(&ev.key, 0);
                    break;
                default:
                    break;
            }
            if (opts->raw_event_handler) opts->raw_event_handler(&ev);
        } while (!stop_flag && SDL_PollEvent(&ev));

        if (anim_is_active()) display_composite_frame();

        if (stop_flag) break;

        tick = (uint32_t) mux_tick();

        if (device_count == 0 && tick >= next_retry_tick) {
            retry_count++;
            uint32_t interval = (retry_count <= retry_fast_count) ? retry_interval_fast_ms : retry_interval_slow_ms;
            LOG_DEBUG("input", "Retrying input device detection...");
            open_all_input_devices();
            next_retry_tick = tick + interval;
            if (device_count > 0) retry_count = 0;
        }

        if (input_is_suppressed()) {
            pressed = 0;
            held = 0;

            if (opts->idle_handler) opts->idle_handler();
            continue;
        }

        update_hold_modifier(opts);

        handle_inputs(opts);
        handle_combos(opts);

        if (opts->analog_handler) opts->analog_handler(raw_ls_x, raw_ls_y, raw_rs_x, raw_rs_y);
        if (opts->idle_handler) opts->idle_handler();

        // Inputs pressed at the end of one iteration are held at the start of the next.
        held = pressed;
    }

    close_all_devices();
}

int mux_input_pressed_any(uint64_t mask) {
    return (pressed & mask) != 0;
}

int mux_input_pressed(mux_input_type mux_type) {
    if (pressed & BIT(mux_type)) return 1;
    if (board_is(BOARD_SPECIAL_G350) && mux_type == MUX_INPUT_MENU && g350_menu_pressed) return 1;

    return 0;
}

void mux_input_stop(void) {
    stop_flag = 1;
    SDL_Event ev = {0};
    ev.type = SDL_QUIT;
    SDL_PushEvent(&ev);
}
