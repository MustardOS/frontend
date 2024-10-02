#include "input.h"

#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/param.h>
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "device.h"

extern uint32_t mux_tick();

// Whether to exit the input task before the next iteration of the event loop.
static bool stop = false;

// Bitmask of input types that are currently active.
static uint64_t pressed = 0;

// Bitmask of input types that were active during the previous iteration of the event loop.
static uint64_t held = 0;

// Processes gamepad buttons.
static void process_key(const mux_input_options *opts, const struct input_event *event) {
    mux_input_type type;
    if (event->code == device.RAW_INPUT.BUTTON.A) {
        type = !opts->swap_btn ? MUX_INPUT_A : MUX_INPUT_B;
    } else if (event->code == device.RAW_INPUT.BUTTON.B) {
        type = !opts->swap_btn ? MUX_INPUT_B : MUX_INPUT_A;
    } else if (event->code == device.RAW_INPUT.BUTTON.C) {
        type = MUX_INPUT_C;
    } else if (event->code == device.RAW_INPUT.BUTTON.X) {
        type = MUX_INPUT_X;
    } else if (event->code == device.RAW_INPUT.BUTTON.Y) {
        type = MUX_INPUT_Y;
    } else if (event->code == device.RAW_INPUT.BUTTON.Z) {
        type = MUX_INPUT_Z;
    } else if (event->code == device.RAW_INPUT.BUTTON.L1) {
        type = MUX_INPUT_L1;
    } else if (event->code == device.RAW_INPUT.BUTTON.L2) {
        type = MUX_INPUT_L2;
    } else if (event->code == device.RAW_INPUT.ANALOG.LEFT.CLICK) {
        type = MUX_INPUT_L3;
    } else if (event->code == device.RAW_INPUT.BUTTON.R1) {
        type = MUX_INPUT_R1;
    } else if (event->code == device.RAW_INPUT.BUTTON.R2) {
        type = MUX_INPUT_R2;
    } else if (event->code == device.RAW_INPUT.ANALOG.RIGHT.CLICK) {
        type = MUX_INPUT_R3;
    } else if (event->code == device.RAW_INPUT.BUTTON.SELECT) {
        type = MUX_INPUT_SELECT;
    } else if (event->code == device.RAW_INPUT.BUTTON.START) {
        type = MUX_INPUT_START;
    } else if (event->code == device.RAW_INPUT.BUTTON.VOLUME_UP) {
        type = MUX_INPUT_VOL_UP;
    } else if (event->code == device.RAW_INPUT.BUTTON.VOLUME_DOWN) {
        type = MUX_INPUT_VOL_DOWN;
    } else if (event->code == device.RAW_INPUT.BUTTON.MENU_SHORT) {
        type = MUX_INPUT_MENU_SHORT;
    } else if (event->code == device.RAW_INPUT.BUTTON.MENU_LONG) {
        type = MUX_INPUT_MENU_LONG;
    } else {
        return;
    }
    pressed = (event->value == 1) ? (pressed | BIT(type)) : (pressed & ~BIT(type));
}

// Processes gamepad axes (D-pad and the sticks).
static void process_abs(const mux_input_options *opts, const struct input_event *event) {
    int axis;
    bool analog;
    if (event->code == device.RAW_INPUT.DPAD.UP) {
        // Axis: D-pad vertical
        axis = !opts->swap_axis ? MUX_INPUT_DPAD_UP : MUX_INPUT_DPAD_LEFT;
        analog = false;
    } else if (event->code == device.RAW_INPUT.DPAD.LEFT) {
        // Axis: D-pad horizontal
        axis = !opts->swap_axis ? MUX_INPUT_DPAD_LEFT : MUX_INPUT_DPAD_UP;
        analog = false;
    } else if (event->code == device.RAW_INPUT.ANALOG.LEFT.UP) {
        // Axis: left stick vertical
        axis = !opts->swap_axis ? MUX_INPUT_LS_UP : MUX_INPUT_LS_LEFT;
        analog = true;
    } else if (event->code == device.RAW_INPUT.ANALOG.LEFT.LEFT) {
        // Axis: left stick horizontal
        axis = !opts->swap_axis ? MUX_INPUT_LS_LEFT : MUX_INPUT_LS_UP;
        analog = true;
    } else if (event->code == device.RAW_INPUT.ANALOG.RIGHT.UP) {
        // Axis: right stick vertical
        axis = !opts->swap_axis ? MUX_INPUT_RS_UP : MUX_INPUT_RS_LEFT;
        analog = true;
    } else if (event->code == device.RAW_INPUT.ANALOG.RIGHT.LEFT) {
        // Axis: right stick horizontal
        axis = !opts->swap_axis ? MUX_INPUT_RS_LEFT : MUX_INPUT_RS_UP;
        analog = true;
    } else {
        return;
    }

    // Sometimes, hardware issues prevent sticks from reaching the full range of the analog axis.
    // (This is especially common with cheap Hall-effect sticks.)
    //
    // We use threshold of 80% of the nominal axis maximum to detect analog directional presses,
    // which seems to accommodate most variation without being too sensitive for "in-spec" sticks.
    if ((analog && event->value <= -device.INPUT.AXIS + device.INPUT.AXIS / 5) ||
            (!analog && event->value == -1)) {
        // Direction: up/left
        pressed = ((pressed | BIT(axis)) & ~BIT(axis + 1));
    } else if ((analog && event->value >= device.INPUT.AXIS - device.INPUT.AXIS / 5) ||
            (!analog && event->value == 1)) {
        // Direction: down/right
        pressed = ((pressed | BIT(axis + 1)) & ~BIT(axis));
    } else {
        // Direction: center
        pressed &= ~(BIT(axis) | BIT(axis + 1));
    }
}

// Process system buttons.
static void process_sys(const mux_input_options *opts, const struct input_event *event) {
    if (event->code == device.RAW_INPUT.BUTTON.POWER_SHORT) {
        switch (event->value) {
            case 1:
                // Power button: short press
                pressed = ((pressed | BIT(MUX_INPUT_POWER_SHORT)) & ~BIT(MUX_INPUT_POWER_LONG));
                break;
            case 2:
                // Power button: long press
                pressed = ((pressed | BIT(MUX_INPUT_POWER_LONG)) & ~BIT(MUX_INPUT_POWER_SHORT));
                break;
            default:
                // Power button: release
                pressed &= ~(BIT(MUX_INPUT_POWER_SHORT) | BIT(MUX_INPUT_POWER_LONG));
                break;
        }
    }
}

// Invokes the relevant handler(s) for a particular input type and action.
static void dispatch_input(const mux_input_options *opts,
                           mux_input_type type,
                           mux_input_action action) {
    // Remap input types when using left stick as D-pad. (We still track pressed and held status for
    // the stick and D-pad inputs separately to avoid unintuitive hold behavior.)
    if (opts->stick_nav) {
        switch (type) {
            case MUX_INPUT_L3:
                type = MUX_INPUT_A;
                break;
            case MUX_INPUT_LS_UP:
                type = MUX_INPUT_DPAD_UP;
                break;
            case MUX_INPUT_LS_DOWN:
                type = MUX_INPUT_DPAD_DOWN;
                break;
            case MUX_INPUT_LS_LEFT:
                type = MUX_INPUT_DPAD_LEFT;
                break;
            case MUX_INPUT_LS_RIGHT:
                type = MUX_INPUT_DPAD_RIGHT;
                break;
            default:
                break;
        }
    }

    mux_input_handler handler = NULL;
    switch (action) {
        case MUX_INPUT_PRESS:
            handler = opts->press_handler[type];
            break;
        case MUX_INPUT_HOLD:
            handler = opts->hold_handler[type];
            break;
        case MUX_INPUT_RELEASE:
            handler = opts->release_handler[type];
            break;
    }

    // First invoke specific handler (if one was registered for this input type and action).
    if (handler) {
        handler();
    }

    // Then invoke generic handler (if a catchall handler was registered).
    if (opts->input_handler) {
        opts->input_handler(type, action);
    }
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
    if (handler) {
        handler();
    }

    // Then invoke generic handler (if a catchall handler was registered).
    if (opts->combo_handler) {
        opts->combo_handler(num, action);
    }
}

static void handle_inputs(const mux_input_options *opts, uint32_t tick) {
    // Delay (millis) before invoking hold handler again.
    static uint32_t hold_delay[MUX_INPUT_COUNT] = {};
    // Tick (millis) of last press or hold.
    static uint32_t hold_tick[MUX_INPUT_COUNT] = {};

    for (int i = 0; i < MUX_INPUT_COUNT; ++i) {
        if (pressed & BIT(i)) {
            if (!(held & BIT(i))) {
                // Pressed & not held: Invoke "press" handler.
                dispatch_input(opts, i, MUX_INPUT_PRESS);

                // Double delay before initial repeat.
                hold_delay[i] = 2 * config.SETTINGS.ADVANCED.ACCELERATE;
                hold_tick[i] = tick;
            } else if (tick - hold_tick[i] >= hold_delay[i]) {
                // Pressed & held: Invoke "hold" handler.
                dispatch_input(opts, i, MUX_INPUT_HOLD);

                // Single delay for each subsequent repeat.
                hold_delay[i] = config.SETTINGS.ADVANCED.ACCELERATE;
                hold_tick[i] = tick;
            }
        } else if (held & BIT(i)) {
            // Held & not pressed: Invoke "release" handler.
            dispatch_input(opts, i, MUX_INPUT_RELEASE);
        }
    }
}

static void handle_combos(const mux_input_options *opts, uint32_t tick) {
    // Delay (millis) before invoking hold handler again.
    static uint32_t hold_delay = 0;
    // Tick (millis) of last press or hold.
    static uint32_t hold_tick = 0;

    // Combo number that was active during the previous iteration of the event loop, or
    // MUX_INPUT_COMBO_COUNT when no combo is held.
    static int active_combo = MUX_INPUT_COMBO_COUNT;

    if (active_combo != MUX_INPUT_COMBO_COUNT) {
        // Active combo; check if it's still held or was released.
        uint64_t mask = opts->combo[active_combo].type_mask;

        if ((pressed & mask) == mask) {
            if (tick - hold_tick >= hold_delay) {
                // Pressed & held: Invoke "hold" handler.
                dispatch_combo(opts, active_combo, MUX_INPUT_HOLD);

                // Single delay for each subsequent repeat.
                hold_delay = config.SETTINGS.ADVANCED.ACCELERATE;
                hold_tick = tick;
            }
        } else {
            // Held & not pressed: Invoke "release" handler.
            dispatch_combo(opts, active_combo, MUX_INPUT_RELEASE);
            active_combo = MUX_INPUT_COMBO_COUNT;
        }
    }

    if (active_combo == MUX_INPUT_COMBO_COUNT && (pressed & ~held)) {
        // No active combo, but a new input was pressed. Check if a combo should activate.
        //
        // Sometimes, a single evdev event can result in us registering both a release and a press
        // (e.g., when transitioning from POWER_SHORT to POWER_LONG), so we have to check this even
        // if a combo was previously active at the start of the function.
        for (int i = 0; i < MUX_INPUT_COMBO_COUNT; ++i) {
            uint64_t mask = opts->combo[i].type_mask;

            if (mask && (pressed & mask) == mask) {
                // Pressed & not held: Invoke "press" handler.
                dispatch_combo(opts, i, MUX_INPUT_PRESS);
                active_combo = i;

                // Double delay before initial repeat.
                hold_delay = 2 * config.SETTINGS.ADVANCED.ACCELERATE;
                hold_tick = tick;

                // Only one combo can be active at a time.
                break;
            }
        }
    }
}

void mux_input_task(const mux_input_options *opts) {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("mux_input_task: epoll_create1");
        return;
    }

    struct epoll_event epoll_event[device.DEVICE.EVENT];

    epoll_event[0].events = EPOLLIN;
    epoll_event[0].data.fd = opts->gamepad_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, opts->gamepad_fd, &epoll_event[0]) == -1) {
        perror("mux_input_task: epoll_ctl(gamepad_fd)");
        return;
    }

    epoll_event[0].events = EPOLLIN;
    epoll_event[0].data.fd = opts->system_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, opts->system_fd, &epoll_event[0]) == -1) {
        perror("mux_input_task: epoll_ctl(system_fd)");
        return;
    }

    // Delay (millis) to wait for input before timing out. This determines the rate at which the
    // hold_handlers and idle_handler are called. To save CPU, we want to wait as long as possible.
    //
    // If no inputs are held, we only have the idle_handler to worry about, so we wait max_idle_ms
    // (or forever if unspecified).
    //
    // If at least one input is held, we instead wait either max_idle_ms or the "menu acceleration"
    // delay (whichever is shorter).
    int timeout;
    int timeout_hold;
    if (opts->max_idle_ms) {
        timeout = opts->max_idle_ms;
        timeout_hold = MIN(opts->max_idle_ms, config.SETTINGS.ADVANCED.ACCELERATE);
    } else {
        timeout = -1 /* infinite */;
        timeout_hold = config.SETTINGS.ADVANCED.ACCELERATE;
    }

    // Input event loop:
    while (!stop) {
        int num_events = epoll_wait(epoll_fd, epoll_event, device.DEVICE.EVENT,
                                    held ? timeout_hold : timeout);
        if (num_events == -1) {
            perror("mux_input_task: epoll_wait");
            continue;
        }

        // Read evdev input events and update pressed/released inputs.
        for (int i = 0; i < num_events; ++i) {
            struct input_event event;
            if (read(epoll_event[i].data.fd, &event, sizeof(event)) == -1) {
                perror("mux_input_task: read");
                continue;
            }

            if (epoll_event[i].data.fd == opts->gamepad_fd) {
                if (event.type == EV_KEY) {
                    process_key(opts, &event);
                } else if (event.type == EV_ABS) {
                    process_abs(opts, &event);
                }
            } else if (epoll_event[i].data.fd == opts->system_fd) {
                process_sys(opts, &event);
            }
        }

        // Identify and invoke handlers for inputs whose state changed.
        uint32_t tick = mux_tick();
        handle_inputs(opts, tick);
        handle_combos(opts, tick);

        // Invoke "idle" handler to run extra logic every iteration of the event loop.
        if (opts->idle_handler) {
            opts->idle_handler();
        }

        // Inputs pressed at the end of one iteration are held at the start of the next.
        held = pressed;
    }
}

bool mux_input_pressed(mux_input_type type) {
    return pressed & BIT(type);
}

void mux_input_stop() {
    stop = true;
}
