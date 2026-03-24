#include <stdint.h>
#include <signal.h>
#include <SDL2/SDL.h>
#include "common.h"
#include "ui_common.h"
#include "config.h"
#include "input.h"
#include "osk.h"
#include "log.h"

#define INPUT_COOLDOWN          256
#define AXIS_THRESHOLD_FRACTION 0.80f
#define AXIS_MAX                32767 // TODO: Maybe set this value as a device variable?
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

// Suppress any input during screensaver runs
static volatile uint32_t suppress_until_tick = 0;

static SDL_GameController *controller = NULL;

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
static mux_input_type key_map[SDL_NUM_SCANCODES];

static void init_input_maps(void) {
    if (input_init_done) return;

    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) controller_button_map[i] = MUX_INPUT_COUNT;

    for (int i = 0; i < 32; i++) joy_button_map[i] = MUX_INPUT_COUNT;

    for (int i = 0; i < SDL_NUM_SCANCODES; i++) key_map[i] = MUX_INPUT_COUNT;

    for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++) {
        axis_map[i].neg = MUX_INPUT_COUNT;
        axis_map[i].pos = MUX_INPUT_COUNT;
    }

    // TODO: Work out what C and Z would eventually map to,
    // TODO: as well as the TrimUI Switch fella!
    controller_button_map[SDL_CONTROLLER_BUTTON_A] = MUX_INPUT_A;
    controller_button_map[SDL_CONTROLLER_BUTTON_B] = MUX_INPUT_B;
    controller_button_map[SDL_CONTROLLER_BUTTON_X] = MUX_INPUT_X;
    controller_button_map[SDL_CONTROLLER_BUTTON_Y] = MUX_INPUT_Y;
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

    joy_button_map[2] = MUX_INPUT_VOL_UP;
    joy_button_map[1] = MUX_INPUT_VOL_DOWN;

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

    key_map[SDL_SCANCODE_SPACE] = MUX_INPUT_A;
    key_map[SDL_SCANCODE_BACKSPACE] = MUX_INPUT_B;
    key_map[SDL_SCANCODE_R] = MUX_INPUT_X;
    key_map[SDL_SCANCODE_F] = MUX_INPUT_Y;

    key_map[SDL_SCANCODE_Q] = MUX_INPUT_L1;
    key_map[SDL_SCANCODE_E] = MUX_INPUT_R1;
    key_map[SDL_SCANCODE_Z] = MUX_INPUT_L2;
    key_map[SDL_SCANCODE_C] = MUX_INPUT_R2;

    key_map[SDL_SCANCODE_LCTRL] = MUX_INPUT_SELECT;
    key_map[SDL_SCANCODE_LSHIFT] = MUX_INPUT_START;

    key_map[SDL_SCANCODE_F1] = MUX_INPUT_MENU;

    key_map[SDL_SCANCODE_UP] = MUX_INPUT_DPAD_UP;
    key_map[SDL_SCANCODE_DOWN] = MUX_INPUT_DPAD_DOWN;
    key_map[SDL_SCANCODE_LEFT] = MUX_INPUT_DPAD_LEFT;
    key_map[SDL_SCANCODE_RIGHT] = MUX_INPUT_DPAD_RIGHT;

    key_map[SDL_SCANCODE_W] = MUX_INPUT_DPAD_UP;
    key_map[SDL_SCANCODE_S] = MUX_INPUT_DPAD_DOWN;
    key_map[SDL_SCANCODE_A] = MUX_INPUT_DPAD_LEFT;
    key_map[SDL_SCANCODE_D] = MUX_INPUT_DPAD_RIGHT;

    key_map[SDL_SCANCODE_PAGEUP] = MUX_INPUT_VOL_UP;
    key_map[SDL_SCANCODE_PAGEDOWN] = MUX_INPUT_VOL_DOWN;

    input_init_done = 1;
}

static inline mux_input_type swap_button_type(mux_input_type t) {
    switch (t) {
        case MUX_INPUT_A:
            return MUX_INPUT_B;
        case MUX_INPUT_B:
            return MUX_INPUT_A;
        case MUX_INPUT_X:
            return MUX_INPUT_Y;
        case MUX_INPUT_Y:
            return MUX_INPUT_X;
        default:
            return t;
    }
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

static void process_sdl_button(const mux_input_options *opts, SDL_GameControllerButton btn, int down) {
    if (input_is_suppressed() || btn >= SDL_CONTROLLER_BUTTON_MAX) return;

    mux_input_type t = controller_button_map[btn];

    if (t == MUX_INPUT_COUNT) {
        LOG_DEBUG("input", "Unmapped controller button %d", btn);
        return;
    }

    if (opts->swap_btn) t = swap_button_type(t);

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

static void process_sdl_axis(SDL_GameControllerAxis axis, int16_t value) {
    if (input_is_suppressed() || axis >= SDL_CONTROLLER_AXIS_MAX) return;

    axis_map_entry m = axis_map[axis];

    if (m.pos == MUX_INPUT_COUNT && m.neg == MUX_INPUT_COUNT) {
        LOG_DEBUG("input", "Unmapped axis %d", axis);
        return;
    }

    if (m.neg == MUX_INPUT_COUNT) {
        pressed = (value >= TRIGGER_THRESHOLD) ? (pressed | BIT(m.pos)) : (pressed & ~BIT(m.pos));
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

static void process_sdl_key(const mux_input_options *opts, const SDL_KeyboardEvent *kev, int down) {
    if (input_is_suppressed()) return;

    SDL_Scancode sc = kev->keysym.scancode;
    if (sc >= SDL_NUM_SCANCODES) return;

    mux_input_type t = key_map[sc];

    if (t == MUX_INPUT_COUNT) {
        LOG_DEBUG("input", "Unmapped key %s", SDL_GetScancodeName(sc));
        return;
    }

    if (kev->repeat) return;
    if (opts->swap_btn) t = swap_button_type(t);

    pressed = down ? (pressed | BIT(t)) : (pressed & ~BIT(t));
}

static SDL_GameController *controller_open(void) {
    static int mappings_loaded = 0;

    if (!mappings_loaded) {
        // TODO: Load from our internal path instead and map to either modern or retro...
        int mappings = SDL_GameControllerAddMappingsFromFile("/usr/lib/gamecontrollerdb.txt");

        if (mappings < 0) {
            LOG_WARN("input", "Failed to load gamecontrollerdb: %s", SDL_GetError());
        } else {
            LOG_INFO("input", "Loaded %d controller mappings", mappings);
        }

        mappings_loaded = 1;
    }

    int n = SDL_NumJoysticks();
    if (n <= 0) {
        LOG_WARN("input", "No joysticks detected");
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *c = SDL_GameControllerOpen(i);
            if (c) {
                LOG_INFO("input", "Controller opened: %s", SDL_GameControllerName(c));
                return c;
            }

            LOG_WARN("input", "Failed to open controller %d: %s", i, SDL_GetError());
            continue;
        }

        const char *name = SDL_JoystickNameForIndex(i);
        LOG_WARN("input", "Joystick detected but not mapped: %s", name ? name : "unknown");
    }

    LOG_WARN("input", "No game controller found at startup");
    return NULL;
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
static void dispatch_input(const mux_input_options *opts, mux_input_type mux_type, mux_input_action action) {
    // Remap input mux_types when using left stick as D-pad. (We still track pressed and held status for
    // the stick and D-pad inputs separately to avoid unintuitive hold behavior.)
    if (opts->remap_to_dpad) mux_type = remap_stick_to_dpad(opts->nav, mux_type);

    mux_input_handler handler = NULL;
    switch (action) {
        case MUX_INPUT_PRESS:
            handler = opts->press_handler[mux_type];
            break;
        case MUX_INPUT_HOLD:
            handler = opts->hold_handler[mux_type];
            break;
        case MUX_INPUT_RELEASE:
            handler = opts->release_handler[mux_type];
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

    /*
     * Only block keys involved in multi-key combos.
     * Single-key combos must still pass through
     * so things like MENU and POWER work.
     */
    for (int i = 0; i < opts->combo_count; i++) {
        uint64_t mask = opts->combo[i].type_mask;

        if (!mask) continue;

        /* Ignore single-key combos */
        if (__builtin_popcountll(mask) <= 1) continue;

        if ((pressed & mask) == mask) {
            blocked |= mask;
        }
    }

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

static inline uint64_t combo_pressed_mask(void) {
    return pressed;
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
    uint64_t active_pressed = combo_pressed_mask();
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
    suppress_until_tick = 0;

    swap_axis = opts->swap_axis;
    g350_mode = board_is_g350();
    tui_mode = board_is_tui();

    ((mux_input_options *) opts)->nav = get_sticknav_mask(config.SETTINGS.ADVANCED.STICKNAV);

    LOG_DEBUG("input", "Navigation mask: 0x%x | g350=%d | tui=%d", opts->nav, g350_mode, tui_mode);

    controller = controller_open();

    const int timeout_idle = (opts->max_idle_ms > 0) ? (int) opts->max_idle_ms : (int) IDLE_MS;
    const int accel_ms = config.SETTINGS.ADVANCED.ACCELERATE > 0 ? config.SETTINGS.ADVANCED.ACCELERATE : 1;
    const int timeout_hold = (opts->max_idle_ms > 0) ? ((int) opts->max_idle_ms < accel_ms ? (int) opts->max_idle_ms : accel_ms) : accel_ms;

    fade_in_screen();

    SDL_Event ev;
    while (!stop_flag) {
        int timeout = held ? timeout_hold : timeout_idle;
        if (!SDL_WaitEventTimeout(&ev, timeout)) ev.type = SDL_USEREVENT;

        do {
            switch (ev.type) {
                case SDL_CONTROLLERBUTTONDOWN:
                    process_sdl_button(opts, ev.cbutton.button, 1);
                    break;
                case SDL_CONTROLLERBUTTONUP:
                    process_sdl_button(opts, ev.cbutton.button, 0);
                    break;
                case SDL_CONTROLLERAXISMOTION:
                    process_sdl_axis(ev.caxis.axis, ev.caxis.value);
                    break;
                case SDL_JOYBUTTONDOWN:
                    process_sdl_joy_button(ev.jbutton.button, 1);
                    break;
                case SDL_JOYBUTTONUP:
                    process_sdl_joy_button(ev.jbutton.button, 0);
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                    if (!controller) {
                        controller = SDL_GameControllerOpen(ev.cdevice.which);
                        if (controller) LOG_INFO("input", "Controller connected: %s", SDL_GameControllerName(controller));
                    }
                    break;
                case SDL_CONTROLLERDEVICEREMOVED:
                    if (controller) {
                        SDL_Joystick *joy = SDL_GameControllerGetJoystick(controller);
                        if (joy && SDL_JoystickInstanceID(joy) == ev.cdevice.which) {
                            SDL_GameControllerClose(controller);
                            controller = NULL;
                            pressed = 0;
                            controller = controller_open();
                        }
                    }
                    break;
                case SDL_KEYDOWN:
                    process_sdl_key(opts, &ev.key, 1);
                    break;
                case SDL_KEYUP:
                    process_sdl_key(opts, &ev.key, 0);
                    break;
                default:
                    break;
            }
        } while (!stop_flag && SDL_PollEvent(&ev));

        if (stop_flag) break;

        if (input_is_suppressed()) {
            pressed = 0;
            held = 0;

            if (opts->idle_handler) opts->idle_handler();
            continue;
        }

        tick = (uint32_t) mux_tick();

        handle_inputs(opts);
        handle_combos(opts);

        // Invoke "idle" handler to run extra logic every iteration of the event loop.
        if (opts->idle_handler) opts->idle_handler();

        // Inputs pressed at the end of one iteration are held at the start of the next.
        held = pressed;
    }

    if (controller) {
        SDL_GameControllerClose(controller);
        controller = NULL;
    }
}

int mux_input_pressed(mux_input_type mux_type) {
    if (pressed & BIT(mux_type)) return 1;
    if (g350_mode && mux_type == MUX_INPUT_MENU && g350_menu_pressed) return 1;

    return 0;
}

void mux_input_stop(void) {
    stop_flag = 1;
    SDL_Event ev = {0};
    ev.type = SDL_QUIT;
    SDL_PushEvent(&ev);
}
